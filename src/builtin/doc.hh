//
//  V compiler
//  Copyright (C) 2019  Vegard Nossum <vegard.nossum@gmail.com>
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

#ifndef V_BUILTIN_DOC_HH
#define V_BUILTIN_DOC_HH

#include "../ast.hh"
#include "../compile.hh"

static value_ptr builtin_macro_doc(const compile_state &state, ast_node_ptr node)
{
	state.expect_type(node, AST_JUXTAPOSE);

	auto lhs_node = state.get_node(node->binop.lhs);
	state.expect_type(lhs_node, AST_LITERAL_STRING);

	// TODO: figure out what to do with doc strings
	//printf("%s\n", state.get_literal_string(lhs_node).c_str());

	auto rhs_node = state.get_node(node->binop.rhs);
	return compile(state, rhs_node);
}

#endif
