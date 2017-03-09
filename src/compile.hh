//
//  V compiler
//  Copyright (C) 2017  Vegard Nossum <vegard.nossum@gmail.com>
//
//  This program is free software: you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation, either version 3 of the License, or
//  (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program.  If not, see <http://www.gnu.org/licenses/>.
//

#ifndef V_COMPILE_HH
#define V_COMPILE_HH

#include "libudis86/extern.h"

#include "ast.hh"
#include "format.hh"
#include "function.hh"
#include "scope.hh"

struct compile_error: std::runtime_error {
	unsigned int pos;
	unsigned int end;

	template<typename... Args>
	compile_error(const ast_node_ptr &node, const char *fmt, Args... args):
		std::runtime_error(format(fmt, args...)),
		pos(node->pos),
		end(node->end)
	{
		assert(end >= pos);
	}
};

static value_ptr compile(function &f, scope_ptr s, ast_node_ptr node);

static void disassemble(const uint8_t *buf, size_t len, uint64_t pc, const std::map<size_t, std::vector<std::string>> &comments)
{
	ud_t u;
	ud_init(&u);
	ud_set_input_buffer(&u, buf, len);
	ud_set_mode(&u, 64);
	ud_set_pc(&u, pc);
	ud_set_syntax(&u, UD_SYN_ATT);

#if 0
	printf("Disassembly at 0x%08lx:\n", pc);

	while (ud_disassemble(&u)) {
		uint64_t offset = ud_insn_off(&u) - pc;
		auto comments_it = comments.find(offset);
		if (comments_it != comments.end()) {
			for (const auto &comment: comments_it->second)
				printf(" %4s  // %s\n", "", comment.c_str());
		}

		printf(" %4lu: %s\n", offset, ud_insn_asm(&u));
	}

	printf("\n");
#endif
}

static void *map(function_ptr f)
{
	size_t length = (f->bytes.size() + 4095) & ~4095;
	void *mem = mmap(NULL, length,
		PROT_READ | PROT_WRITE | PROT_EXEC,
		MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
	if (mem == MAP_FAILED)
		throw std::runtime_error(format("mmap() failed: %s", strerror(errno)));

	memcpy(mem, &f->bytes[0], f->bytes.size());

	// Flush instruction cache so we know we'll
	// execute what we compiled and not some
	// garbage that happened to be in the cache.
	__builtin___clear_cache((char *) mem, (char *) mem + length);

	return mem;
}

static void run(function_ptr f)
{
	void *mem = map(f);
	disassemble((const uint8_t *) mem, f->bytes.size(), (uint64_t) mem, f->comments);

	// TODO: ABI
	auto ret = f->return_value;
	if (!ret || ret->type->size == 0) {
		// No return value
		auto fn = (void (*)()) mem;
		fn();
	} else if (ret->type->size > sizeof(unsigned long)) {
		// TODO
		// If the return value is bigger than a long, we need to pass
		// a pointer to it as the first argument.
		//auto fn = (void (*)(void *)) mem;
		//fn();
		assert(false);
	} else {
		// The return value fits in a long
		auto fn = (long (*)()) mem;
		fn();
	}

	size_t length = (f->bytes.size() + 4095) & ~4095;
	munmap(mem, length);
}

static value_ptr eval(function &f, scope_ptr s, ast_node_ptr node)
{
	auto new_f = std::make_shared<function>(true);
	new_f->emit_prologue();
	auto v = compile(*new_f, s, node);
	new_f->emit_epilogue();

	run(new_f);

	// TODO
	return v;
}

static value_ptr compile_brackets(function &f, scope_ptr s, ast_node_ptr node)
{
	return compile(f, s, node->unop);
}

static value_ptr compile_curly_brackets(function &f, scope_ptr s, ast_node_ptr node)
{
	// Curly brackets create a new scope parented to the old one
	auto new_scope = std::make_shared<scope>(s);
	return compile(f, new_scope, node->unop);
}

static value_ptr compile_juxtapose(function &f, scope_ptr s, ast_node_ptr node)
{
	auto lhs = compile(f, s, node->binop.lhs);
	auto lhs_type = lhs->type;
	if (lhs_type == builtin_type_macro) {
		assert(lhs->storage_type == VALUE_GLOBAL);

		// macros are evaluated directly
		auto fn = (value_ptr (*)(function &, scope_ptr, ast_node_ptr)) lhs->global.host_address;
		return fn(f, s, node->binop.rhs);
	}

	if (lhs_type == builtin_type_type) {
		assert(lhs->storage_type == VALUE_GLOBAL);

		// call type's constructor
		auto type = *(value_type_ptr *) lhs->global.host_address;
		if (!type->constructor)
			throw compile_error(node, "type doesn't have a constructor");

		// TODO: functions as constructors
		return type->constructor(f, s, node->binop.rhs);
	}

	if (lhs_type->call)
		return lhs_type->call(f, s, lhs, node->binop.rhs);

	throw compile_error(node, "type is not callable");
}

static value_ptr compile_symbol_name(function &f, scope_ptr s, ast_node_ptr node)
{
	auto ret = s->lookup(node->symbol_name);
	if (!ret)
		throw compile_error(node, "could not resolve symbol %s", node->symbol_name.c_str());

	return ret;
}

static value_ptr compile_semicolon(function &f, scope_ptr s, ast_node_ptr node)
{
	// TODO: should we return the result of compiling LHS or void?
	compile(f, s, node->binop.lhs);
	return compile(f, s, node->binop.rhs);
}

static value_ptr compile(function &f, scope_ptr s, ast_node_ptr node)
{
	switch (node->type) {
	case AST_BRACKETS:
		return compile_brackets(f, s, node);
	case AST_CURLY_BRACKETS:
		return compile_curly_brackets(f, s, node);

	case AST_JUXTAPOSE:
		return compile_juxtapose(f, s, node);
	case AST_SYMBOL_NAME:
		return compile_symbol_name(f, s, node);
	case AST_SEMICOLON:
		return compile_semicolon(f, s, node);
	default:
		node->dump(stderr);
		fprintf(stderr, "\n");
		throw compile_error(node, "internal compiler error: unrecognised AST node type");
	}

	assert(false);
	return nullptr;
}

#endif
