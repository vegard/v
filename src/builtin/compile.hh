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

#ifndef V_BUILTIN_COMPILE_HH
#define V_BUILTIN_COMPILE_HH

#include "../ast.hh"
#include "../compile.hh"
#include "../function.hh"
#include "../scope.hh"
#include "../value.hh"

// Just a wrapper around compile() that we can use to hook compile() calls from user code
static value_ptr _compile(context_ptr c, function_ptr f, scope_ptr s, ast_node_ptr node)
{
	//asm volatile ("int3");
	return compile(compile_state(c, f, s), node);
}

static value_ptr builtin_function_compile(const compile_state &state, ast_node_ptr node)
{
	// XXX: make wrapping functions easier
	assert(node->type == AST_BRACKETS);

	auto fun_type = std::make_shared<value_type>();
	fun_type->alignment = alignof(void *);
	fun_type->size = alignof(void *);
	fun_type->argument_types = std::vector<value_type_ptr>({
		builtin_type_context,
		builtin_type_function,
		builtin_type_scope,
		builtin_type_ast_node,
	});
	fun_type->return_type = builtin_type_value;

	auto val = std::make_shared<value>(nullptr, VALUE_GLOBAL, fun_type);
	auto global = new void *;
	*global = (void *) &_compile;
	val->global.host_address = global;

	return _call_fun(state, val, node);
}

#endif

