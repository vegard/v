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

#include "./ast.hh"
#include "./builtin.hh"
#include "./function.hh"
#include "./scope.hh"
#include "./value.hh"

struct macro;
typedef std::shared_ptr<macro> macro_ptr;

struct macro {
	virtual ~macro()
	{
	}

	virtual value_ptr invoke(const compile_state &state, ast_node_ptr node) = 0;
};

static value_ptr builtin_type_macro_constructor(value_type_ptr type, const compile_state &state, ast_node_ptr node);

static auto builtin_type_macro = std::make_shared<value_type>(value_type {
	.alignment = alignof(macro_ptr),
	.size = sizeof(macro_ptr),
	.constructor = &builtin_type_macro_constructor,
});

// Helper for macros that can be implemented simply as a callback function
struct simple_macro: macro {
	value_ptr (*fn)(const compile_state &, ast_node_ptr);

	simple_macro(value_ptr (*fn)(const compile_state &, ast_node_ptr)):
		fn(fn)
	{
	}

	~simple_macro()
	{
	}

	value_ptr invoke(const compile_state &state, ast_node_ptr node)
	{
		return fn(state, node);
	}
};

// Helper for macros that operate on a (compile-time) value.
// This is used for example when compiling (x + y); here, 'x' is first
// compiled (so we can get its type), then an '_add' macro is called with
// 'x' as a value and 'y' as an unevaluated AST node.
struct val_macro: macro {
	value_ptr (*fn)(const compile_state &state, value_ptr, ast_node_ptr);
	value_ptr val;

	val_macro(value_ptr (*fn)(const compile_state &state, value_ptr, ast_node_ptr), value_ptr val):
		fn(fn),
		val(val)
	{
	}

	~val_macro()
	{
	}

	value_ptr invoke(const compile_state &state, ast_node_ptr node)
	{
		return fn(state, val, node);
	}
};

#endif
