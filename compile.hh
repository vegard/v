#ifndef V_COMPILE_HH
#define V_COMPILE_HH

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
	if (lhs->type == &builtin_type_macro) {
		// macros are evaluated directly
		auto fn = (value_ptr (*)(function &, scope_ptr, ast_node_ptr)) lhs->global.host_address;
		return fn(f, s, node->binop.rhs);
	}

	// TODO: handle functions?
	assert(false);
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
			auto ret = std::make_shared<value>(VALUE_CONSTANT, &builtin_type_int);
			// TODO: handle 'int' as arbitrary-precision
			if (!node->literal_integer.fits_slong_p())
				throw compile_error(node, "[tmp] int is too large to fit in 64 bits");
			ret->constant.u64 = node->literal_integer.get_si();
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
