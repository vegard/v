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

#ifndef V_MACRO_HH
#define V_MACRO_HH

#include <memory>

struct value;
typedef std::shared_ptr<value> value_ptr;

struct function;
typedef std::shared_ptr<function> function_ptr;

struct scope;
typedef std::shared_ptr<scope> scope_ptr;

struct ast_node;
typedef std::shared_ptr<ast_node> ast_node_ptr;

struct macro;
typedef std::shared_ptr<macro> macro_ptr;

struct macro {
	virtual ~macro()
	{
	}

	virtual value_ptr invoke(function_ptr ptr, scope_ptr s, ast_node_ptr node) = 0;
};

static auto builtin_type_macro = std::make_shared<value_type>(value_type {
	.alignment = alignof(macro),
	.size = sizeof(macro),
});

// Helper for macros that can be implemented simply as a callback function
struct simple_macro: macro {
	value_ptr (*fn)(function_ptr, scope_ptr, ast_node_ptr);

	simple_macro(value_ptr (*fn)(function_ptr, scope_ptr, ast_node_ptr)):
		fn(fn)
	{
	}

	~simple_macro()
	{
	}

	value_ptr invoke(function_ptr ptr, scope_ptr s, ast_node_ptr node)
	{
		return fn(ptr, s, node);
	}
};

#endif
