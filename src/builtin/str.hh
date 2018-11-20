//
//  V compiler
//  Copyright (C) 2018  Vegard Nossum <vegard.nossum@gmail.com>
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

#ifndef V_BUILTIN_STR_H
#define V_BUILTIN_STR_H

#include "../compile.hh"
#include "../value.hh"

static value_ptr builtin_type_str_constructor(value_type_ptr, const compile_state &, ast_node_ptr);

// TODO: don't use std::string internally
static auto builtin_type_str = std::make_shared<value_type>(value_type {
	.alignment = alignof(std::string),
	.size = sizeof(std::string),
	.constructor = &builtin_type_str_constructor,
	.argument_types = std::vector<value_type_ptr>(),
	.return_type = value_type_ptr(),
	.members = std::map<std::string, member_ptr>({
		// TODO
	}),
});

static value_ptr builtin_type_str_constructor(value_type_ptr, const compile_state &state, ast_node_ptr node)
{
	if (node->type != AST_LITERAL_STRING)
		state.error(node, "expected literal string");

	auto ret = std::make_shared<value>(nullptr, VALUE_GLOBAL, builtin_type_str);

	auto global = new std::string;
	*global = state.get_literal_string(node);
	ret->global.host_address = (void *) global;
	return ret;
}

#endif
