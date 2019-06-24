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

#ifndef V_BUILTIN_IF_HH
#define V_BUILTIN_IF_HH

#include "../ast.hh"
#include "../compile.hh"
#include "../function.hh"
#include "../scope.hh"
#include "../value.hh"

static value_ptr builtin_macro_if(const compile_state &state, ast_node_ptr node)
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

	auto c = state.context;
	auto f = state.function;

	if (node->type != AST_JUXTAPOSE)
		state.error(node, "expected 'if <expression> <expression>'");

	ast_node_ptr condition_node = state.get_node(node->binop.lhs);
	ast_node_ptr true_node = nullptr;
	ast_node_ptr false_node = nullptr;

	auto rhs = state.get_node(node->binop.rhs);
	if (rhs->type == AST_JUXTAPOSE) {
		true_node = state.get_node(rhs->binop.lhs);

		rhs = state.get_node(rhs->binop.rhs);
		if (rhs->type != AST_JUXTAPOSE)
			state.error(rhs, "expected 'else <expression>'");

		auto else_node = state.get_node(rhs->binop.lhs);
		if (else_node->type != AST_SYMBOL_NAME || state.get_symbol_name(else_node) != "else")
			state.error(else_node, "expected 'else'");

		false_node = state.get_node(rhs->binop.rhs);
	} else {
		true_node = rhs;
	}

	// Got all the bits that we need, now try to compile it.

	value_ptr return_value;

	// "if" condition
	auto condition_value = compile(state, condition_node);
	if (condition_value->type != builtin_type_boolean)
		state.error(condition_node, "'if' condition must be boolean");

	auto false_label = f->new_label();
	f->emit_jump_if_zero(condition_value, false_label);

	// "if" block
	auto true_value = compile(state, true_node);
	if (true_value->type != builtin_type_void) {
		return_value = f->alloc_local_value(c, true_value->type);
		f->emit_move(true_value, return_value);
	}

	auto end_label = f->new_label();
	f->emit_jump(end_label);

	// "else" block
	f->emit_label(false_label);
	value_ptr false_value;
	if (false_node) {
		false_value = compile(state, false_node);
		if (false_value->type != builtin_type_void && false_value->type == true_value->type)
			f->emit_move(false_value, return_value);
	}

	// next statement
	f->emit_label(end_label);

	// finalize
	f->link_label(false_label);
	f->link_label(end_label);

	if (!return_value || !false_value || true_value->type != false_value->type)
		return_value = builtin_value_void;

	return return_value;
}

#endif
