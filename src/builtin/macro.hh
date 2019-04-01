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

typedef value_ptr (*user_macro_fn_type)(compile_state_ptr state, ast_node_ptr node);

// Macros defined by a program we're compiling
struct user_macro: macro {
	value_ptr fn_value;

	user_macro(value_ptr fn_value):
		fn_value(fn_value)
	{
		assert(fn_value->storage_type == VALUE_GLOBAL);
	}

	value_ptr invoke(const compile_state &state, ast_node_ptr node)
	{
		assert(fn_value->storage_type == VALUE_GLOBAL);
		auto fn = *(user_macro_fn_type *) fn_value->global.host_address;

		// NOTE: it's easier for us to pass a shared_ptr to compiled
		// code, that's why we create one here (from a copy of the
		// state that was passed to us). There is a little bit of
		// associated overhead, but makes things a lot easier since
		// then the compiled code doesn't have to know anything about
		// proper pointers.
		auto new_state = std::make_shared<compile_state>(state);

		// XXX: why the indirection? I forgot why I did it this way.
		return (*fn)(new_state, node);
	}
};

static value_ptr builtin_type_macro_constructor(value_type_ptr type, const compile_state &state, ast_node_ptr node)
{
	static std::vector<value_type_ptr> argument_types = {
		builtin_type_compile_state,
		builtin_type_ast_node,
	};

	static auto macro_fun_type_value = _builtin_macro_fun(state.context, builtin_type_value, argument_types);
	static auto macro_fun_type = *(value_type_ptr *) macro_fun_type_value->global.host_address;

	std::vector<std::string> args;
	args.push_back("state");
	args.push_back("node");

	auto macro_fun = __construct_fun(macro_fun_type, state, node, args, node);

	auto m = std::make_shared<user_macro>(macro_fun);

	auto ret = std::make_shared<value>(nullptr, VALUE_GLOBAL, builtin_type_macro);
	auto global = new macro_ptr(m);
	ret->global.host_address = (void *) global;
	return ret;
}

#endif
