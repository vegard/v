#ifndef V_BUILTIN_HH
#define V_BUILTIN_HH

#include "ast.hh"
#include "compile.hh"
#include "function.hh"
#include "scope.hh"
#include "value.hh"

static value_ptr builtin_macro_define(function &f, scope_ptr s, ast_node_ptr node)
{
	auto lhs = node->binop.lhs;
	if (lhs->type != AST_SYMBOL_NAME)
		throw compile_error(node, "definition of non-symbol");

	// Create new local
	auto rhs = compile(f, s, node->binop.rhs);
	auto val = f.alloc_local_value(rhs->type);
	s->define(node, lhs->symbol_name, val);
	f.emit_move(rhs, val);
	return val;
}

static value_ptr builtin_macro_assign(function &f, scope_ptr s, ast_node_ptr node)
{
	auto rhs = compile(f, s, node->binop.rhs);
	auto lhs = compile(f, s, node->binop.lhs);
	f.emit_move(rhs, lhs);
	return lhs;
}

static value_ptr builtin_macro_equals(function &f, scope_ptr s, ast_node_ptr node)
{
	auto lhs = compile(f, s, node->binop.lhs);
	auto rhs = compile(f, s, node->binop.rhs);
	if (lhs->type != rhs->type)
		throw compile_error(node, "cannot compare values of different types");

	f.emit_compare(lhs, rhs);

	// TODO: actually set the value
	return std::make_shared<value>(VALUE_LOCAL, &boolean_type);
}

static value_ptr builtin_macro_debug(function &f, scope_ptr s, ast_node_ptr node)
{
	// TODO: need context so we can print line numbers and stuff too
	node->dump();
	printf("\n");

	return std::make_shared<value>(VALUE_CONSTANT, &void_type);
}

static value_ptr builtin_macro_if(function &f, scope_ptr s, ast_node_ptr node)
{
	// Extract condition, true block, and false block (if any) from AST
	//
	// Input: "if a b else c";
	// Parse tree:
	// (juxtapose
	//     (symbol_name if)
	//     (juxtapose <-- node
	//         (symbol_name a) <-- node->binop.lhs AKA condition_node
	//         (juxtapose      <-- node->binop.rhs AKA rhs
	//             (symbol_name b) <-- rhs->binop.lhs AKA true_node
	//             (juxtapose      <-- rhs->binop.rhs AKA rhs
	//                 (symbol_name else) <-- rhs->binop.lhs AKA else_node
	//                 (symbol_name b)    <-- rhs->binop.rhs AKA false_node
	//             )
	//         )
	//     )
	// )

	if (node->type != AST_JUXTAPOSE)
		throw compile_error(node, "expected 'if <expression> <expression>'");

	ast_node_ptr condition_node = node->binop.lhs;
	ast_node_ptr true_node;
	ast_node_ptr false_node;

	auto rhs = node->binop.rhs;
	if (rhs->type == AST_JUXTAPOSE) {
		true_node = rhs->binop.lhs;

		rhs = rhs->binop.rhs;
		if (rhs->type != AST_JUXTAPOSE)
			throw compile_error(rhs, "expected 'else <expression>'");

		auto else_node = rhs->binop.lhs;
		if (else_node->type != AST_SYMBOL_NAME || else_node->symbol_name != "else")
			throw compile_error(else_node, "expected 'else'");

		false_node = rhs->binop.rhs;
	} else {
		true_node = rhs;
	}

	// Got all the bits that we need, now try to compile it.

	value_ptr return_value;

	// "if" condition
	auto condition_value = compile(f, s, condition_node);
	if (condition_value->type != &boolean_type)
		throw compile_error(condition_node, "'if' condition must be boolean");

	label false_label;
	f.emit_jump_if_zero(false_label);

	// "if" block
	auto true_value = compile(f, s, true_node);
	if (true_value->type != &void_type) {
		return_value = f.alloc_local_value(true_value->type);
		f.emit_move(true_value, return_value);
	}

	label end_label;
	f.emit_jump(end_label);

	// "else" block
	f.emit_label(false_label);
	if (false_node) {
		auto false_value = compile(f, s, false_node);
		if (false_value->type != true_value->type)
			throw compile_error(false_node, "'else' block must return the same type as 'if' block");
		if (false_value->type != &void_type)
			f.emit_move(false_value, return_value);
	} else {
		if (true_value->type != &void_type)
			throw compile_error(node, "expected 'else' since 'if' block has return value");
	}

	// next statement
	f.emit_label(end_label);

	// finalize
	f.link_label(false_label);
	f.link_label(end_label);

	// TODO: just return nullptr instead?
	if (!return_value) {
		return_value = std::make_shared<value>(VALUE_CONSTANT, &void_type);
		//new (&return_value->constant.node) ast_node_ptr;
	}
	return return_value;
}

static value_ptr builtin_macro_fun(function &f, scope_ptr s, ast_node_ptr node)
{
	// TODO
	assert(false);
	return nullptr;
}

#endif
