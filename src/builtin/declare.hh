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

#ifndef V_BUILTIN_DECLARE_HH
#define V_BUILTIN_DECLARE_HH

#include "../ast.hh"
#include "../compile.hh"
#include "../function.hh"
#include "../scope.hh"
#include "../value.hh"

static value_ptr builtin_macro_declare(const compile_state &state, ast_node_ptr node)
{
	if (node->type != AST_JUXTAPOSE)
		state.error(node, "expected juxtaposition");

	auto lhs = node->binop.lhs;
	if (lhs->type != AST_SYMBOL_NAME)
		state.error(node, "declaration of non-symbol");

	// see builtin_macro_define
	auto rhs_node = node->binop.rhs;
	auto rhs = eval(state, rhs_node);
	if (rhs->storage_type != VALUE_GLOBAL)
		state.error(rhs_node, "type must be known at compile time");
	if (rhs->type != builtin_type_type)
		state.error(rhs_node, "type must be an instance of a type");
	auto rhs_type = *(value_type_ptr *) rhs->global.host_address;

	// For functions that are run at compile-time, we allocate
	// a new global value. The _name_ is still scoped as usual,
	// though.
	auto val = std::make_shared<value>(state.context, VALUE_GLOBAL, rhs_type);
	auto global = new uint8_t[rhs_type->size];
	val->global.host_address = (void *) global;

	state.scope->define(state.function, node, lhs->symbol_name, val);
	return val;
}

#endif
