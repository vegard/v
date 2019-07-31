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

#ifndef V_BUILTIN_EQUALS_HH
#define V_BUILTIN_EQUALS_HH

#include "../ast.hh"
#include "../compile.hh"
#include "../function.hh"
#include "../scope.hh"
#include "../value.hh"

static value_ptr builtin_macro_equals(const compile_state &state, ast_node_ptr node)
{
	if (node->type != AST_JUXTAPOSE)
		state.error(node, "expected juxtaposition");

	auto lhs = compile(state, state.get_node(node->binop.lhs));
	auto rhs = compile(state, state.get_node(node->binop.rhs));
	if (lhs->type != rhs->type)
		state.error(node, "cannot compare values of different types");

	auto ret = state.function->alloc_local_value(state.context, builtin_type_boolean);
	state.function->emit_eq(function::CMP_EQ, lhs, rhs, ret);
	return ret;
}

static value_ptr builtin_macro_notequals(const compile_state &state, ast_node_ptr node)
{
	if (node->type != AST_JUXTAPOSE)
		state.error(node, "expected juxtaposition");

	auto lhs = compile(state, state.get_node(node->binop.lhs));
	auto rhs = compile(state, state.get_node(node->binop.rhs));
	if (lhs->type != rhs->type)
		state.error(node, "cannot compare values of different types");

	auto ret = state.function->alloc_local_value(state.context, builtin_type_boolean);
	state.function->emit_eq(function::CMP_NEQ, lhs, rhs, ret);
	return ret;
}

#endif
