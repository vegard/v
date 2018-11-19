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

#ifndef V_BUILTIN_DEBUG_HH
#define V_BUILTIN_DEBUG_HH

#include <iostream>

#include "../ast.hh"
#include "../ast_serializer.hh"
#include "../compile.hh"
#include "../function.hh"
#include "../scope.hh"
#include "../value.hh"

static value_ptr builtin_macro_debug(const compile_state &state, ast_node_ptr node)
{
	// TODO: need context so we can print line numbers and stuff too
	ast_serializer(state.source).serialize(std::cout, node);
	std::cout << std::endl;

	return builtin_value_void;
}

#endif
