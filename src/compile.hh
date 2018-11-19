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
#include "ast_serializer.hh"
#include "compile_error.hh"
#include "format.hh"
#include "function.hh"
#include "globals.hh"
#include "scope.hh"
#include "source_file.hh"
#include "value.hh"

typedef std::shared_ptr<std::vector<object_ptr>> objects_ptr;

// This is a badly named; for the future I'd like to rename 'context' to
// something else and then rename this to 'compile_context'
struct compile_state {
	// XXX: Implicit assumption: when objects == nullptr, we're compiling for the host
	objects_ptr objects;
	source_file_ptr source;
	context_ptr context;
	function_ptr function;
	scope_ptr scope;

	compile_state(source_file_ptr &source, context_ptr &context, function_ptr &function, scope_ptr &scope):
		source(source),
		context(context),
		function(function),
		scope(scope)
	{
	}

	unsigned int new_object(object_ptr object) const
	{
		assert(objects);
		unsigned int object_id = objects->size();
		objects->push_back(object);
		return object_id;
	}

	compile_state set_objects(objects_ptr objects) const
	{
		auto ret = *this;
		ret.objects = objects;
		return ret;
	}

	compile_state set_source(source_file_ptr &new_source, scope_ptr &new_scope) const
	{
		auto ret = *this;
		ret.source = new_source;
		ret.scope = new_scope;
		return ret;
	}

	compile_state set_context(context_ptr &new_context) const
	{
		auto ret = *this;
		ret.context = new_context;
		return ret;
	}

	compile_state set_scope(scope_ptr &new_scope) const
	{
		auto ret = *this;
		ret.scope = new_scope;
		return ret;
	}

	compile_state set_function(context_ptr &new_context, function_ptr &new_function) const
	{
		auto ret = *this;
		ret.context = new_context;
		ret.function = new_function;
		return ret;
	}

	compile_state set_function(function_ptr &new_function, scope_ptr &new_scope) const
	{
		auto ret = *this;
		ret.function = new_function;
		ret.scope = new_scope;
		return ret;
	}

	template<typename... Args>
	void __attribute__((noreturn)) error(const ast_node_ptr &node, const char *fmt, Args... args) const
	{
		throw compile_error(source, node, fmt, args...);
	}

	template<typename... Args>
	void expect(const ast_node_ptr &node, bool cond, const char *fmt, Args... args) const
	{
		if (!cond)
			error(node, fmt, args...);
	}

	void expect_type(const ast_node_ptr &node, ast_node_type type) const
	{
		// TODO: stringify the expected and actual types
		expect(node, node->type == type, "got AST node type $, expected $", node->type, type);
	}

	void expect_type(const ast_node_ptr &node, value_ptr value, value_type_ptr type) const
	{
		// TODO: can we even stringify these types?
		expect(node, value->type == type, "unexpected type");
	}

	value_ptr lookup(const ast_node_ptr &node, const std::string name) const
	{
		scope::entry e;
		if (!scope->lookup(name, e))
			return nullptr;

		// We can always access globals
		auto val = e.val;
		if (val->storage_type == VALUE_GLOBAL || val->storage_type == VALUE_TARGET_GLOBAL || val->storage_type == VALUE_CONSTANT)
			return val;

		if (e.f != function)
			error(node, "cannot access local variable of different function");

		return val;
	}

	void use_value(const ast_node_ptr &node, value_ptr val) const
	{
		if (!can_use_value(context, val))
			error(node, "cannot access value at compile time");
	}
};

static value_ptr compile(const compile_state &state, ast_node_ptr node);

static void disassemble(const uint8_t *buf, size_t len, uint64_t pc, const std::map<size_t, std::vector<std::pair<unsigned int, std::string>>> &comments)
{
	ud_t u;
	ud_init(&u);
	ud_set_input_buffer(&u, buf, len);
	ud_set_mode(&u, 64);
	ud_set_pc(&u, pc);
	ud_set_syntax(&u, UD_SYN_ATT);

	printf("Disassembly at 0x%08lx:\n", pc);

	unsigned int indentation = 0;
	while (ud_disassemble(&u)) {
		uint64_t offset = ud_insn_off(&u) - pc;
		auto comments_it = comments.find(offset);
		if (comments_it != comments.end()) {
			for (const auto &comment: comments_it->second) {
				indentation = comment.first;
				const auto &str = comment.second;
				printf("\e[33m%4s//%*.s %s\n", "", 2 * indentation, "", str.c_str());
			}
		}

		printf("\e[0m %4lx: %*.s%s\n", offset, 2 * indentation, "", ud_insn_asm(&u));
	}

	printf("\e[0m\n");
}

static void *map(function_ptr f)
{
	size_t length = (f->bytes.size() + 4095) & ~4095;
	void *mem = mmap(NULL, length,
		PROT_READ | PROT_WRITE | PROT_EXEC,
		MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
	if (mem == MAP_FAILED)
		throw std::runtime_error(format("mmap() failed: $", strerror(errno)));

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

	if (global_disassemble)
		disassemble((const uint8_t *) mem, f->bytes.size(), (uint64_t) mem, f->this_object->comments);

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

static value_ptr eval(const compile_state &state, ast_node_ptr node)
{
#if 0
	std::cout << "eval: ";
	ast_serializer().serialize(std::cout, node);
	std::cout << std::endl;
#endif

	auto new_c = std::make_shared<context>(state.context);
	auto new_f = std::make_shared<function>(true);
	new_f->emit_prologue();

	auto v = compile(state.set_function(new_c, new_f), node);

	value_ptr ret;
	if (v->storage_type == VALUE_LOCAL || v->storage_type == VALUE_LOCAL_POINTER) {
		// Make sure we copy the value out to a new global in case the
		// returned value is a local (which cannot be accessed outside
		// "new_f" itself).
		ret = std::make_shared<value>(new_c, VALUE_GLOBAL, v->type);
		auto global = new uint8_t[v->type->size];
		ret->global.host_address = (void *) global;
		new_f->emit_move(v, ret);
	} else {
		// We can return it directly
		ret = v;
	}

	new_f->emit_epilogue();

	run(new_f);
	return ret;
}

static value_ptr compile_brackets(const compile_state &state, ast_node_ptr node)
{
	return compile(state, node->unop);
}

static value_ptr compile_curly_brackets(const compile_state &state, ast_node_ptr node)
{
	// Curly brackets create a new scope parented to the old one
	auto new_scope = std::make_shared<scope>(state.scope);
	return compile(state.set_scope(new_scope), node->unop);
}

static value_ptr compile_member(const compile_state &state, ast_node_ptr node)
{
	assert(node->type == AST_MEMBER);

	auto lhs = compile(state, node->binop.lhs);
	auto lhs_type = lhs->type;

	auto rhs_node = node->binop.rhs;
	if (rhs_node->type != AST_SYMBOL_NAME)
		// TODO: say which AST node type we got instead of a symbol name
		state.error(node, "member name must be a symbol");

	auto it = lhs_type->members.find(rhs_node->literal_string);
	if (it == lhs_type->members.end())
		state.error(node, "unknown member: $", rhs_node->literal_string.c_str());

	return it->second->invoke(state, lhs, rhs_node);
}

static value_ptr _compile_juxtapose(const compile_state &state, ast_node_ptr lhs_node, value_ptr lhs, ast_node_ptr rhs_node)
{
	auto lhs_type = lhs->type;

	if (lhs_type == builtin_type_macro) {
		// macros are evaluated directly
		auto new_c = std::make_shared<context>(state.context);
		state.set_context(new_c).use_value(lhs_node, lhs);
		assert(lhs->storage_type == VALUE_GLOBAL);

		auto m = *(macro_ptr *) lhs->global.host_address;
		return m->invoke(state, rhs_node);
	} else if (lhs_type == builtin_type_type) {
		auto new_c = std::make_shared<context>(state.context);
		state.set_context(new_c).use_value(lhs_node, lhs);
		assert(lhs->storage_type == VALUE_GLOBAL);

		// call type's constructor
		auto type = *(value_type_ptr *) lhs->global.host_address;
		if (!type->constructor)
			state.error(lhs_node, "type doesn't have a constructor");

		// TODO: functions as constructors
		return type->constructor(type, state, rhs_node);
	}

	auto it = lhs_type->members.find("_call");
	if (it != lhs_type->members.end())
		return it->second->invoke(state, lhs, rhs_node);

	state.error(lhs_node, "type is not callable");
}

static value_ptr compile_juxtapose(const compile_state &state, ast_node_ptr node)
{
	assert(node->type == AST_JUXTAPOSE);

	auto lhs_node = node->binop.lhs;
	auto rhs_node = node->binop.rhs;
	auto lhs = compile(state, lhs_node);
	return _compile_juxtapose(state, lhs_node, lhs, rhs_node);
}

static value_ptr compile_symbol_name(const compile_state &state, ast_node_ptr node)
{
	auto ret = state.lookup(node, node->symbol_name);
	if (!ret)
		state.error(node, "could not resolve symbol $", node->symbol_name.c_str());

	return ret;
}

static value_ptr compile_semicolon(const compile_state &state, ast_node_ptr node)
{
	// TODO: should we return the result of compiling LHS or void?
	compile(state, node->binop.lhs);
	return compile(state, node->binop.rhs);
}

static value_ptr builtin_type_u64_constructor(value_type_ptr, const compile_state &, ast_node_ptr);
static value_ptr builtin_type_str_constructor(value_type_ptr, const compile_state &, ast_node_ptr);

static value_ptr compile(const compile_state &state, ast_node_ptr node)
{
	if (!node)
		return builtin_value_void;

	function_enter(state.function, get_source_for(state.source, node));

	switch (node->type) {
	case AST_LITERAL_INTEGER:
		// TODO: evaluate as int rather than u64
		return builtin_type_u64_constructor(nullptr, state, node);
	case AST_LITERAL_STRING:
		return builtin_type_str_constructor(nullptr, state, node);

	case AST_BRACKETS:
		return compile_brackets(state, node);
	case AST_CURLY_BRACKETS:
		return compile_curly_brackets(state, node);

	case AST_MEMBER:
		return compile_member(state, node);
	case AST_JUXTAPOSE:
		return compile_juxtapose(state, node);
	case AST_SYMBOL_NAME:
		return compile_symbol_name(state, node);
	case AST_SEMICOLON:
		return compile_semicolon(state, node);
	default:
		state.error(node, "internal compiler error: unrecognised AST node type $: $", node->type, abbreviate(node));
	}

	assert(false);
	return nullptr;
}

#endif
