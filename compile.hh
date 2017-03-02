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

static void disassemble(const uint8_t *buf, size_t len, uint64_t pc)
{
	ud_t u;
	ud_init(&u);
	ud_set_input_buffer(&u, buf, len);
	ud_set_mode(&u, 64);
	ud_set_pc(&u, pc);
	ud_set_syntax(&u, UD_SYN_ATT);

	printf("Disassembly at 0x%08lx:\n", pc);

	while (ud_disassemble(&u))
		printf("  %s\n", ud_insn_asm(&u));
}

static void run(function_ptr f)
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

	disassemble((const uint8_t *) mem, f->bytes.size(), (uint64_t) mem);

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
		assert(lhs->metatype == VALUE_GLOBAL);

		// macros are evaluated directly
		auto fn = (value_ptr (*)(function &, scope_ptr, ast_node_ptr)) lhs->global.host_address;
		return fn(f, s, node->binop.rhs);
	}

	if (lhs_type == builtin_type_type) {
		assert(lhs->metatype == VALUE_GLOBAL);

		// call type's constructor
		auto type = (value_type *) lhs->global.host_address;
		if (!type->constructor)
			throw compile_error(node, "type doesn't have a constructor");

		// TODO
		return type->constructor(/*f, s, node->binop.rhs*/);
	}

	if (lhs_type->call)
		return lhs_type->call(f, s, node);

	throw compile_error(node, "type is not callable");
}

static value_ptr compile_symbol_name(function &f, scope_ptr s, ast_node_ptr node)
{
	auto ret = s->lookup(node->symbol_name);
	if (!ret)
		throw compile_error(node, "could not resolve symbol");

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
	case AST_LITERAL_INTEGER:
		{
			auto ret = std::make_shared<value>(VALUE_GLOBAL, builtin_type_int);
			// TODO: handle 'int' as arbitrary-precision
			if (!node->literal_integer.fits_slong_p())
				throw compile_error(node, "[tmp] int is too large to fit in 64 bits");
			//ret->constant.u64 = node->literal_integer.get_si();
			auto global = new uint64_t;
			*global = node->literal_integer.get_si();
			ret->global.host_address = (void *) global;
			return ret;
		}
#if 0
	case AST_LITERAL_STRING:
		{
			auto ret = std::make_shared<value>(VALUE_CONSTANT, &string_type);
			new (&ret->constant.node) ast_node_ptr(node);
			return ret;
		}
#endif

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
