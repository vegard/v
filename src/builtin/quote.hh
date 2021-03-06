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

#ifndef V_BUILTIN_QUOTE_HH
#define V_BUILTIN_QUOTE_HH

#include "ast.hh"
#include "builtin.hh"
#include "compile.hh"
#include "function.hh"
#include "scope.hh"
#include "value.hh"

static value_ptr builtin_macro_quote(ast_node_ptr node)
{
	auto ret = state->scope->make_value(nullptr, VALUE_GLOBAL, builtin_type_ast_node);
	auto global = new ast_node_ptr(node);
	ret->global.host_address = (void *) global;
	return ret;
}

#endif

