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

#ifndef V_BUILTIN_VALUE_H
#define V_BUILTIN_VALUE_H

#include "../compile.hh"

static value_ptr builtin_macro_value_constructor(value_type_ptr type, const compile_state &state, ast_node_ptr node);

static auto builtin_type_value = std::make_shared<value_type>(value_type {
	.alignment = alignof(value_ptr),
	.size = sizeof(value_ptr),
	.constructor = &builtin_macro_value_constructor,
});

// "value" is a macro that evaluates an expression and returns a "value_ptr"
// rather than the value itself (is this the same as "compile"?)
static value_ptr builtin_macro_value_constructor(value_type_ptr type, const compile_state &state, ast_node_ptr node)
{
	auto v = compile(state, node);

	auto value_value = std::make_shared<value>(nullptr, VALUE_GLOBAL, builtin_type_value);
	auto value_copy = new value_ptr(v);
	value_value->global.host_address = (void *) value_copy;
	return value_value;
}

#endif
