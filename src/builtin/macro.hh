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

#include "builtin.hh"
#include "macro.hh"
#include "value.hh"
#include "builtin/fun.hh"
#include "builtin/value.hh"

// Macros defined by a program we're compiling
struct user_macro: macro {
	value_ptr fn_value;

	user_macro(value_ptr fn_value):
		fn_value(fn_value)
	{
		assert(fn_value->storage_type == VALUE_GLOBAL);
	}

	value_ptr invoke(ast_node_ptr node)
	{
		assert(fn_value->storage_type == VALUE_GLOBAL);
		auto fn = *(jit_function **) fn_value->global.host_address;

		value_ptr result;
		uint64_t args[] = {
			// XXX: uint64 vs. unsigned long?
			(uint64_t) &result,
			(uint64_t) &state,
			(uint64_t) node,
		};

		run_bytecode(&fn->constants[0], &fn->bytecode[0], &args[0], sizeof(args) / sizeof(*args));

		assert(result);
		return result;
	}
};

#if 0
static void _compile_state_new_scope(uint64_t *args)
{
	old_scope = state->scope;
	auto new_scope = std::make_shared<scope>(state->scope);
	state->scope = new_scope;
}

static void _compile_state_restore_scope(uint64_t *args)
{
	auto new_scope = state->scope;
	state->scope = old_scope;
	delete new_scope;
}
#endif

static value_ptr builtin_macro__new_scope(ast_node_ptr node)
{
#if 0
	// TODO: create a local variable to hold a pointer to the old scope
	auto old_scope = state->function->alloc_local_value(state->scope, state->context, builtin_type_scope);
#endif

#if 0
	{
		auto fun_type = std::make_shared<value_type>();
		fun_type->alignment = alignof(void *);
		fun_type->size = alignof(void *);
		fun_type->return_type = builtin_type_void;

		auto val = state->scope->make_value(nullptr, VALUE_GLOBAL, fun_type);
		auto global = new void *;
		*global = (void *) &_compile_state_new_scope;
		val->global.host_address = global;

		__call_fun(val, node, std::vector<std::pair<ast_node_ptr, value_ptr>>(), true);
	}
#endif

	// XXX: for now we're really cheating and not creating a new scope
	// at all.

	auto ret = compile(node);

#if 0
	// TODO: this actually needs to be done as a scope destruction hook, since
	// if compile() includes a 'return' statement then it won't get executed.
	{
		auto fun_type = std::make_shared<value_type>();
		fun_type->alignment = alignof(void *);
		fun_type->size = alignof(void *);
		fun_type->return_type = builtin_type_void;

		auto val = state->scope->make_value(nullptr, VALUE_GLOBAL, fun_type);
		auto global = new void *;
		*global = (void *) &_compile_state_restore_scope;
		val->global.host_address = global;

		__call_fun(val, node, std::vector<std::pair<ast_node_ptr, value_ptr>>(), true);
	}
#endif

	return ret;
}

static void _builtin_macro__define(uint64_t *args)
{
	auto name = (ast_node_ptr) args[0];
	auto value = (value_ptr) args[1];

	if (name->type != AST_SYMBOL_NAME)
		error(name, "expected symbol");

	assert(value);
	state->scope->define(state->function, state->source, name, get_symbol_name(name), value);
}

static value_ptr builtin_macro__define(ast_node_ptr node)
{
	expect_type(node, AST_BRACKETS);

	std::vector<std::pair<ast_node_ptr, value_ptr>> args;
	for (auto arg_node: traverse<AST_COMMA>(state->source->tree, get_node(node->unop)))
		args.push_back(std::make_pair(arg_node, compile(arg_node)));

	auto fun_type = std::make_shared<value_type>();
	fun_type->alignment = alignof(void *);
	fun_type->size = alignof(void *);
	fun_type->argument_types = std::vector<value_type_ptr>({
		builtin_type_ast_node,
		builtin_type_value,
	});
	fun_type->return_type = builtin_type_void;

	auto val = state->scope->make_value(nullptr, VALUE_GLOBAL, fun_type);
	auto global = new void *;
	*global = (void *) &_builtin_macro__define;
	val->global.host_address = global;

	return __call_fun(val, node, args, true);
}

static void _builtin_macro__compile(uint64_t *args)
{
	auto &retval = *(value_ptr *) args[0];
	auto node = (ast_node_ptr) args[1];

	new (&retval) value_ptr(compile(node));
}

static value_ptr builtin_macro__compile(ast_node_ptr node)
{
	expect_type(node, AST_BRACKETS);

	std::vector<std::pair<ast_node_ptr, value_ptr>> args;
	for (auto arg_node: traverse<AST_COMMA>(state->source->tree, get_node(node->unop)))
		args.push_back(std::make_pair(arg_node, compile(arg_node)));

	auto fun_type = std::make_shared<value_type>();
	fun_type->alignment = alignof(void *);
	fun_type->size = alignof(void *);
	fun_type->argument_types = std::vector<value_type_ptr>({
		builtin_type_ast_node,
	});
	fun_type->return_type = builtin_type_value;

	auto val = state->scope->make_value(nullptr, VALUE_GLOBAL, fun_type);
	auto global = new void *;
	*global = (void *) &_builtin_macro__compile;
	val->global.host_address = global;

	return __call_fun(val, node, args, true);
}

static void _builtin_macro__eval(uint64_t *args)
{
	auto &retval = *(value_ptr *) args[0];
	auto node = (ast_node_ptr) args[1];

	new (&retval) value_ptr(eval(node));
}

static value_ptr builtin_macro__eval(ast_node_ptr node)
{
	expect_type(node, AST_BRACKETS);

	std::vector<std::pair<ast_node_ptr, value_ptr>> args;
	for (auto arg_node: traverse<AST_COMMA>(state->source->tree, get_node(node->unop)))
		args.push_back(std::make_pair(arg_node, compile(arg_node)));

	auto fun_type = std::make_shared<value_type>();
	fun_type->alignment = alignof(void *);
	fun_type->size = alignof(void *);
	fun_type->argument_types = std::vector<value_type_ptr>({
		builtin_type_ast_node,
	});
	fun_type->return_type = builtin_type_value;

	auto val = state->scope->make_value(nullptr, VALUE_GLOBAL, fun_type);
	auto global = new void *;
	*global = (void *) &_builtin_macro__eval;
	val->global.host_address = global;

	return __call_fun(val, node, args, true);
}

static value_ptr builtin_type_macro_constructor(value_type_ptr type, ast_node_ptr node)
{
	auto new_scope = std::make_shared<scope>(state->scope);
	new_scope->define_builtin_macro("new_scope", builtin_macro__new_scope);
	new_scope->define_builtin_macro("define", builtin_macro__define);
	new_scope->define_builtin_macro("compile", builtin_macro__compile);
	new_scope->define_builtin_macro("eval", builtin_macro__eval);

	static std::vector<value_type_ptr> argument_types = {
		builtin_type_compile_state,
		builtin_type_ast_node,
	};

	static auto macro_fun_type_value = _builtin_macro_fun(builtin_type_value, argument_types);
	static auto macro_fun_type = *(value_type_ptr *) macro_fun_type_value->global.host_address;

	std::vector<std::string> args;
	args.push_back("state");
	args.push_back("node");

	auto macro_fun = (use_scope(new_scope), __construct_fun(macro_fun_type, node, args, node));

	auto m = std::make_shared<user_macro>(macro_fun);

	auto ret = state->scope->make_value(nullptr, VALUE_GLOBAL, builtin_type_macro);
	auto global = new macro_ptr(m);
	ret->global.host_address = (void *) global;
	return ret;
}

#endif
