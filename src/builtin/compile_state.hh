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

#ifndef V_BUILTIN_COMPILE_STATE_HH
#define V_BUILTIN_COMPILE_STATE_HH

#include "../compile.hh"
#include "./fun.hh"
#include "./value.hh"

// TODO for this whole file: provide helper for calling a function!

static void _compile_state_new_scope(uint64_t *args)
{
	auto &retval = *(compile_state_ptr *) args[0];
	auto state = *(compile_state_ptr *) args[1];

	auto new_scope = std::make_shared<scope>(state->scope);

	new (&retval) compile_state_ptr(std::make_shared<compile_state>(state->set_scope(new_scope)));
}

static value_ptr builtin_type_compile_state_new_scope(const compile_state &state, value_ptr this_state, ast_node_ptr node)
{
	// XXX: make wrapping functions easier

	state.expect_type(node, this_state, builtin_type_compile_state);
	state.expect_type(node, AST_BRACKETS);

	std::vector<std::pair<ast_node_ptr, value_ptr>> args;
	args.push_back(std::make_pair(node, this_state));
	for (auto arg_node: traverse<AST_COMMA>(state.source->tree, state.get_node(node->unop)))
		args.push_back(std::make_pair(arg_node, compile(state, arg_node)));

	auto fun_type = std::make_shared<value_type>();
	fun_type->alignment = alignof(void *);
	fun_type->size = alignof(void *);
	fun_type->argument_types = std::vector<value_type_ptr>({
		builtin_type_compile_state,
	});
	fun_type->return_type = builtin_type_compile_state;

	auto val = state.scope->make_value(nullptr, VALUE_GLOBAL, fun_type);
	auto global = new void *;
	*global = (void *) &_compile_state_new_scope;
	val->global.host_address = global;

	return __call_fun(state, val, node, args, true);
}

static void _compile_state_new_value(uint64_t *args)
{
	auto &retval = *(value_ptr *) args[0];
	auto state = *(compile_state_ptr *) args[1];
	auto node = (ast_node_ptr) args[2];

	auto v = compile(*state, node);

	// XXX: leak
	new (&retval) value_ptr(state->scope->make_value(nullptr, VALUE_GLOBAL, builtin_type_value));
	retval->global.host_address = (void *) new value_ptr(v);
}

// "value" is a macro that evaluates an expression and returns a "value_ptr"
// rather than the value itself (is this the same as "compile"?)
static value_ptr builtin_type_compile_state_new_value(const compile_state &state, value_ptr this_state, ast_node_ptr node)
{
	state.expect_type(node, this_state, builtin_type_compile_state);
	state.expect_type(node, AST_BRACKETS);

	std::vector<std::pair<ast_node_ptr, value_ptr>> args;
	args.push_back(std::make_pair(node, this_state));
	for (auto arg_node: traverse<AST_COMMA>(state.source->tree, state.get_node(node->unop)))
		args.push_back(std::make_pair(arg_node, compile(state, arg_node)));

	auto fun_type = std::make_shared<value_type>();
	fun_type->alignment = alignof(void *);
	fun_type->size = alignof(void *);
	fun_type->argument_types = std::vector<value_type_ptr>({
		builtin_type_compile_state,
	});
	fun_type->return_type = builtin_type_value;

	auto val = state.scope->make_value(nullptr, VALUE_GLOBAL, fun_type);
	auto global = new void *;
	*global = (void *) &_compile_state_new_value;
	val->global.host_address = global;

	return __call_fun(state, val, node, args, true);

}

static void _compile_state_define(uint64_t *args)
{
	auto state = *(compile_state_ptr *) args[0];
	auto name = (ast_node_ptr) args[1];
	auto value = (value_ptr) args[2];

	if (name->type != AST_SYMBOL_NAME)
		state->error(name, "expected symbol");

	assert(value);
	state->scope->define(state->function, state->source, name, state->get_symbol_name(name), value);
}

static value_ptr builtin_type_compile_state_define(const compile_state &state, value_ptr this_state, ast_node_ptr node)
{
	// XXX: make wrapping functions easier

	state.expect_type(node, this_state, builtin_type_compile_state);
	state.expect_type(node, AST_BRACKETS);

	std::vector<std::pair<ast_node_ptr, value_ptr>> args;
	args.push_back(std::make_pair(node, this_state));
	for (auto arg_node: traverse<AST_COMMA>(state.source->tree, state.get_node(node->unop)))
		args.push_back(std::make_pair(arg_node, compile(state, arg_node)));

	auto fun_type = std::make_shared<value_type>();
	fun_type->alignment = alignof(void *);
	fun_type->size = alignof(void *);
	fun_type->argument_types = std::vector<value_type_ptr>({
		builtin_type_compile_state,
		builtin_type_ast_node,
		builtin_type_value,
	});
	fun_type->return_type = builtin_type_void;

	auto val = state.scope->make_value(nullptr, VALUE_GLOBAL, fun_type);
	auto global = new void *;
	*global = (void *) &_compile_state_define;
	val->global.host_address = global;

	return __call_fun(state, val, node, args, true);
}

static void _compile_state_eval(uint64_t *args)
{
	auto &retval = *(value_ptr *) args[0];
	auto state = *(compile_state_ptr *) args[1];
	auto node = (ast_node_ptr) args[2];

	new (&retval) value_ptr(eval(*state, node));
}

static value_ptr builtin_type_compile_state_eval(const compile_state &state, value_ptr this_state, ast_node_ptr node)
{
	// XXX: make wrapping functions easier

	state.expect_type(node, this_state, builtin_type_compile_state);
	state.expect_type(node, AST_BRACKETS);

	std::vector<std::pair<ast_node_ptr, value_ptr>> args;
	args.push_back(std::make_pair(node, this_state));
	for (auto arg_node: traverse<AST_COMMA>(state.source->tree, state.get_node(node->unop)))
		args.push_back(std::make_pair(arg_node, compile(state, arg_node)));

	auto fun_type = std::make_shared<value_type>();
	fun_type->alignment = alignof(void *);
	fun_type->size = alignof(void *);
	fun_type->argument_types = std::vector<value_type_ptr>({
		builtin_type_compile_state,
		builtin_type_ast_node,
	});
	fun_type->return_type = builtin_type_value;

	auto val = state.scope->make_value(nullptr, VALUE_GLOBAL, fun_type);
	auto global = new void *;
	*global = (void *) &_compile_state_eval;
	val->global.host_address = global;

	return __call_fun(state, val, node, args, true);
}

static void _compile_state_compile(uint64_t *args)
{
	auto &retval = *(value_ptr *) args[0];
	auto state = *(compile_state_ptr *) args[1];
	auto node = (ast_node_ptr) args[2];

	new (&retval) value_ptr(compile(*state, node));
}

static value_ptr builtin_type_compile_state_compile(const compile_state &state, value_ptr this_state, ast_node_ptr node)
{
	// XXX: make wrapping functions easier

	state.expect_type(node, this_state, builtin_type_compile_state);
	state.expect_type(node, AST_BRACKETS);

	std::vector<std::pair<ast_node_ptr, value_ptr>> args;
	args.push_back(std::make_pair(node, this_state));
	for (auto arg_node: traverse<AST_COMMA>(state.source->tree, state.get_node(node->unop)))
		args.push_back(std::make_pair(arg_node, compile(state, arg_node)));

	auto fun_type = std::make_shared<value_type>();
	fun_type->alignment = alignof(void *);
	fun_type->size = alignof(void *);
	fun_type->argument_types = std::vector<value_type_ptr>({
		builtin_type_compile_state,
		builtin_type_ast_node,
	});
	fun_type->return_type = builtin_type_value;

	auto val = state.scope->make_value(nullptr, VALUE_GLOBAL, fun_type);
	auto global = new void *;
	*global = (void *) &_compile_state_compile;
	val->global.host_address = global;

	return __call_fun(state, val, node, args, true);
}

#endif
