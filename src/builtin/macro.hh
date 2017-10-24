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

#ifndef V_BUILTIN_MACRO_H
#define V_BUILTIN_MACRO_H

#include "../builtin.hh"
#include "../macro.hh"
#include "../value.hh"
#include "./fun.hh"
#include "./value.hh"

typedef value_ptr (*user_macro_fn_type)(context_ptr, function_ptr, scope_ptr, ast_node_ptr);

// Macros defined by a program we're compiling
struct user_macro: macro {
	value_ptr fn_value;

	user_macro(value_ptr fn_value):
		fn_value(fn_value)
	{
		assert(fn_value->storage_type == VALUE_GLOBAL);
	}

	value_ptr invoke(context_ptr c, function_ptr f, scope_ptr s, ast_node_ptr node)
	{
		assert(fn_value->storage_type == VALUE_GLOBAL);
		auto fn = *(user_macro_fn_type *) fn_value->global.host_address;

		// XXX: why the indirection? I forgot why I did it this way.
		return (*fn)(c, f, s, node);
	}
};

static value_ptr builtin_type_macro_constructor(value_type_ptr type, context_ptr c, function_ptr f, scope_ptr s, ast_node_ptr node)
{
	static std::vector<value_type_ptr> argument_types = {
		builtin_type_context,
		builtin_type_function,
		builtin_type_scope,
		builtin_type_ast_node,
	};

	static auto macro_fun_type_value = _builtin_macro_fun(c, builtin_type_value, argument_types);
	static auto macro_fun_type = *(value_type_ptr *) macro_fun_type_value->global.host_address;

	std::vector<std::string> args;
	args.push_back("context");
	args.push_back("function");
	args.push_back("scope");
	args.push_back("node");

	auto macro_fun = __construct_fun(macro_fun_type, c, f, s, node, args, node);

	auto m = std::make_shared<user_macro>(macro_fun);

	auto ret = std::make_shared<value>(nullptr, VALUE_GLOBAL, builtin_type_macro);
	auto global = new macro_ptr(m);
	ret->global.host_address = (void *) global;
	return ret;
}

#endif
