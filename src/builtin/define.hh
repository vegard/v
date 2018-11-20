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

#ifndef V_BUILTIN_DEFINE_HH
#define V_BUILTIN_DEFINE_HH

#include "../ast.hh"
#include "../compile.hh"
#include "../function.hh"
#include "../scope.hh"
#include "../value.hh"

// _define at the top-level, creates globals
static value_ptr builtin_macro_define(const compile_state &state, ast_node_ptr node)
{
	if (node->type != AST_JUXTAPOSE)
		state.error(node, "expected juxtaposition");

	auto lhs = state.get_node(node->binop.lhs);
	if (lhs->type != AST_SYMBOL_NAME)
		state.error(node, "definition of non-symbol");

	auto symbol_name = state.get_symbol_name(lhs);

	// For functions that are run at compile-time, we allocate
	// a new global value. The _name_ is still scoped as usual,
	// though.
	auto rhs = compile(state, state.get_node(node->binop.rhs));
	auto val = std::make_shared<value>(state.context, VALUE_GLOBAL, rhs->type);
	auto global = new uint8_t[rhs->type->size];
	val->global.host_address = (void *) global;

	state.scope->define(state.function, node, symbol_name, val);
	state.function->emit_move(rhs, val);
	return val;
}

#endif
