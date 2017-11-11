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

#ifndef V_BUILTIN_SCOPE_HH
#define V_BUILTIN_SCOPE_HH

#include <cassert>

#include "../builtin.hh"
#include "./fun.hh"
#include "./value.hh"

// We take 'this_scope' by reference so that we can construct a new scope_ptr
// in-place on the memory allocated by the caller using placement-new.
static void _scope_constructor(scope_ptr &this_scope, scope_ptr parent)
{
	new (&this_scope) scope_ptr(std::make_shared<scope>(parent));
}

static value_ptr builtin_type_scope_constructor(value_type_ptr type, const compile_state &state, ast_node_ptr node)
{
	// XXX: make wrapping functions easier

	auto this_val = state.function->alloc_local_value(state.context, builtin_type_scope);

	std::vector<std::pair<ast_node_ptr, value_ptr>> args;
	args.push_back(std::make_pair(node, this_val));
	args.push_back(std::make_pair(node, compile(state, node)));

	assert(type == builtin_type_scope);

	auto fun_type = std::make_shared<value_type>();
	fun_type->alignment = alignof(void *);
	fun_type->size = alignof(void *);
	fun_type->argument_types = std::vector<value_type_ptr>({
		builtin_type_scope,
		builtin_type_scope,
	});
	fun_type->return_type = builtin_type_void;

	auto val = std::make_shared<value>(nullptr, VALUE_GLOBAL, fun_type);
	auto global = new void *;
	*global = (void *) &_scope_constructor;
	val->global.host_address = global;

	__call_fun(state, val, node, args);
	return this_val;
}

static void _scope_define(scope_ptr this_scope, function_ptr f, ast_node_ptr name, value_ptr value)
{
	if (name->type != AST_SYMBOL_NAME)
		throw compile_error(name, "expected symbol");

	this_scope->define(f, name, name->literal_string, value);
}

static value_ptr builtin_type_scope_define(const compile_state &state, value_ptr this_scope, ast_node_ptr node)
{
	// XXX: make wrapping functions easier

	assert(this_scope->type == builtin_type_scope);
	assert(node->type == AST_BRACKETS);

	std::vector<std::pair<ast_node_ptr, value_ptr>> args;
	args.push_back(std::make_pair(node, this_scope));
	for (auto arg_node: traverse<AST_COMMA>(node->unop))
		args.push_back(std::make_pair(arg_node, compile(state, arg_node)));

	assert(this_scope->type == builtin_type_scope);

	auto fun_type = std::make_shared<value_type>();
	fun_type->alignment = alignof(void *);
	fun_type->size = alignof(void *);
	fun_type->argument_types = std::vector<value_type_ptr>({
		builtin_type_scope,
		builtin_type_function,
		builtin_type_ast_node,
		builtin_type_value,
	});
	fun_type->return_type = builtin_type_void;

	auto val = std::make_shared<value>(nullptr, VALUE_GLOBAL, fun_type);
	auto global = new void *;
	*global = (void *) &_scope_define;
	val->global.host_address = global;

	return __call_fun(state, val, node, args);
}

#endif
