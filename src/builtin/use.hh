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

#ifndef V_BUILTIN_USE_HH
#define V_BUILTIN_USE_HH

#include "../ast.hh"
#include "../compile.hh"
#include "../function.hh"
#include "../scope.hh"
#include "../value.hh"

static value_ptr builtin_macro_use(ast_node_ptr node)
{
	auto new_scope = std::make_shared<scope>(state->scope);
	auto v = (use_scope(new_scope), compile(node));

	// Move each newly defined variable to the current scope
	for (auto &it: v->type->members) {
		// XXX: what about shadowed variables? should probably be an error
		// XXX: preserve location of original definition
		// XXX: should we really invoke the macro here?
		state->scope->define(nullptr, nullptr, nullptr, it.first, it.second->invoke(v, node));
	}

	return builtin_value_void;
}

#endif
