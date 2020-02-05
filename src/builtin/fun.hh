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

#ifndef V_BUILTIN_FUN_HH
#define V_BUILTIN_FUN_HH

#include <array>
#include <set>

#include "../ast.hh"
#include "../compile.hh"
#include "../function.hh"
#include "../scope.hh"
#include "../value.hh"

struct return_macro: macro {
	function_ptr f;
	scope_ptr s;
	value_type_ptr return_type;
	value_ptr return_value;
	label_ptr return_label;

	return_macro(function_ptr f, scope_ptr s, value_type_ptr return_type, value_ptr return_value, label_ptr return_label):
		f(f),
		s(s),
		return_type(return_type),
		return_value(return_value),
		return_label(return_label)
	{
	}

	value_ptr invoke(const compile_state &state, ast_node_ptr node)
	{
		if (state.function != this->f)
			state.error(node, "'return' used outside defining function");

		// The scope where we are used must be the scope where we
		// were defined or a child.
		if (!is_parent_of(this->s, state.scope))
			state.error(node, "'return' used outside defining scope");

		f->comment("return");

		auto v = compile(state, node);
		if (v->type != return_type)
			state.error(node, "wrong return type for function");

		if (return_value)
			state.function->emit_move(v, return_value);

		state.function->emit_jump(return_label);

		// TODO: if the last "statement" in a function is a return, then we
		// want that to be the return type/value of the expression.
		return v;
	}
};

// _define inside functions always creates locals
static value_ptr fun_define_macro(const compile_state &state, ast_node_ptr node)
{
	if (node->type != AST_JUXTAPOSE)
		state.error(node, "expected juxtaposition");

	auto lhs = state.get_node(node->binop.lhs);
	if (lhs->type != AST_SYMBOL_NAME)
		state.error(node, "definition of non-symbol");

	auto symbol_name = state.get_symbol_name(lhs);

	auto rhs = compile(state, state.get_node(node->binop.rhs));
	auto val = state.function->alloc_local_value(state.scope, state.context, rhs->type);
	state.scope->define(state.function, state.source, node, symbol_name, val);
	state.function->emit_move(rhs, val);
	return val;
}

// Low-level helper (for use after data has been extracted from syntax)
static value_ptr __construct_fun(value_type_ptr type, const compile_state &state, ast_node_ptr node,
	std::vector<std::string> &args, ast_node_ptr body_node)
{
	auto c = state.context;

	auto &argument_types = type->argument_types;
	auto &return_type = type->return_type;

	// TODO: this is a bit ugly, especially with the downcasting afterwards
	function_ptr new_f;
	if (state.objects)
		new_f = std::make_shared<x86_64_function>(state.scope, c, !state.objects, argument_types, return_type);
	else
		new_f = std::make_shared<bytecode_function>(state.scope, c, !state.objects, argument_types, return_type);

	auto new_scope = std::make_shared<scope>(state.scope);

	auto return_label = new_f->new_label();

	// TODO: use multiple regs or pass on stack
	// AMD64 ABI: return types with non-trivial copy constructors or destructors are
	// passed through a pointer in the first argument.
	assert(return_type);

	new_f->emit_prologue();

	for (unsigned int i = 0; i < args.size(); ++i)
		new_scope->define(new_f, state.source, node, args[i], new_f->args_values[i]);

	new_scope->define_builtin_macro("_define", fun_define_macro);
	new_scope->define_builtin_macro("return", std::make_shared<return_macro>(new_f, new_scope, return_type, new_f->return_value, return_label));

	auto v = compile(state.set_function(new_f, new_scope), body_node);
	auto v_type = v->type;
	if (v_type != return_type)
		state.error(node, "wrong return type for function");

	new_f->emit_move(v, new_f->return_value);
	new_f->emit_label(return_label);
	new_f->link_label(return_label);
	new_f->emit_epilogue();

	if (state.objects) {
		// target
		auto x86_64_f = std::dynamic_pointer_cast<x86_64_function>(new_f);
		// TODO: use new_state/new_scope?
		return state.scope->make_value(nullptr, type, state.new_object(x86_64_f->this_object));
	} else {
		// host
		auto bytecode_f = std::dynamic_pointer_cast<bytecode_function>(new_f);

		// We need this to keep the new function from getting freed when this
		// function returns.
		// TODO: Another solution?
		static std::set<function_ptr> functions;
		functions.insert(bytecode_f);

		auto ret = state.scope->make_value(nullptr, VALUE_GLOBAL, type);
		auto jf = new jit_function(bytecode_f);
		ret->global.host_address = new void *(jf);

		if (global_disassemble) {
			printf("host fun: %p\n", jf);
			disassemble_bytecode(bytecode_f->constants.data(), bytecode_f->bytes.data(), bytecode_f->bytes.size(), bytecode_f->comments);
			printf("\n");
		}

		return ret;
	}
}

// actually compile a function body
//  - 'type' is the function type (signature)
//  - 'node' is the function body
static value_ptr _construct_fun(value_type_ptr type, const compile_state &state, ast_node_ptr node)
{
	if (node->type != AST_JUXTAPOSE)
		state.error(node, "expected (<argument types>...) <body>");

	auto args_node = state.get_node(node->binop.lhs);
	if (args_node->type != AST_BRACKETS)
		state.error(node, "expected (<argument names>...)");

	std::vector<std::string> args;
	for (auto arg_node: traverse<AST_COMMA>(state.source->tree, state.get_node(args_node->unop))) {
		if (arg_node->type != AST_SYMBOL_NAME)
			state.error(node, "expected symbol for argument name");

		auto symbol_name = state.get_symbol_name(arg_node);
		args.push_back(symbol_name);
	}

	if (args.size() != type->argument_types.size())
		state.error(node, "expected $ arguments; got $", type->argument_types.size(), args.size());

	auto body_node = state.get_node(node->binop.rhs);
	return __construct_fun(type, state, node, args, body_node);
}

// Low-level helper (for use after data has been extracted from syntax)
static value_ptr __call_fun(const compile_state &state, value_ptr fn, ast_node_ptr node, std::vector<std::pair<ast_node_ptr, value_ptr>> args, bool c_call)
{
	auto f = state.function;

	// TODO: save/restore caller save registers
	auto type = fn->type;

	if (args.size() != type->argument_types.size())
		state.error(node, "expected $ arguments; got $", type->argument_types.size(), args.size());

	auto return_type = type->return_type;
	value_ptr return_value;
	if (return_type == builtin_type_void)
		return_value = builtin_value_void;
	else
		return_value = f->alloc_local_value(state.scope, state.context, return_type);

	std::vector<value_ptr> args_values;
	for (unsigned int i = 0; i < args.size(); ++i) {
		auto arg_node = args[i].first;
		auto arg_value = args[i].second;

		auto arg_type = arg_value->type;
		if (arg_type != type->argument_types[i])
			state.error(arg_node, "wrong argument type");

		args_values.push_back(arg_value);
	}

	if (c_call)
		f->emit_c_call(fn, args_values, return_value);
	else
		f->emit_call(fn, args_values, return_value);

	return return_value;
}

static value_ptr _call_fun(const compile_state &state, value_ptr fn, ast_node_ptr node)
{
	if (node->type != AST_BRACKETS)
		state.error(node, "expected parantheses");

	std::vector<std::pair<ast_node_ptr, value_ptr>> args;
	for (auto arg_node: traverse<AST_COMMA>(state.source->tree, state.get_node(node->unop)))
		args.push_back(std::make_pair(arg_node, compile(state, arg_node)));

	return __call_fun(state, fn, node, args, false);
}

// Low-level helper (for use after data has been extracted from syntax)
static value_ptr _builtin_macro_fun(const compile_state &state, value_type_ptr ret_type, const std::vector<value_type_ptr> &argument_types)
{
	value_type_ptr type;

	// Create new type for this signature
	// TODO: use sizeof(void (*)())?
	type = std::make_shared<value_type>();
	type->alignment = alignof(void *);
	type->size = sizeof(void *);
	type->constructor = _construct_fun;
	type->argument_types = argument_types;
	type->return_type = ret_type;
	type->members["_call"] = std::make_shared<callback_member>(_call_fun);

	// XXX: refcounting
	auto type_value = state.scope->make_value(nullptr, VALUE_GLOBAL, builtin_type_type);
	auto type_copy = new value_type_ptr(type);
	type_value->global.host_address = (void *) type_copy;
	return type_value;
}

static value_ptr builtin_macro_fun(const compile_state &state, ast_node_ptr node)
{
	// Extract parameters and code block from AST

	if (node->type != AST_JUXTAPOSE)
		state.error(node, "expected 'fun <expression> (<expression>)'");

	auto ret_type_node = state.get_node(node->binop.lhs);
	auto ret_type_value = eval(state, ret_type_node);
	if (ret_type_value->storage_type != VALUE_GLOBAL)
		state.error(ret_type_node, "return type must be known at compile time");
	if (ret_type_value->type != builtin_type_type)
		state.error(ret_type_node, "return type must be an instance of a type");
	auto ret_type = *(value_type_ptr *) ret_type_value->global.host_address;

	auto brackets_node = state.get_node(node->binop.rhs);
	if (brackets_node->type != AST_BRACKETS)
		state.error(brackets_node, "expected (<expression>...)");

	auto args_node = state.get_node(brackets_node->unop);

	std::vector<value_type_ptr> argument_types;
	for (auto arg_type_node: traverse<AST_COMMA>(state.source->tree, args_node)) {
		value_ptr arg_type_value = eval(state, arg_type_node);
		if (arg_type_value->storage_type != VALUE_GLOBAL)
			state.error(arg_type_node, "argument type must be known at compile time");
		if (arg_type_value->type != builtin_type_type)
			state.error(arg_type_node, "argument type must be an instance of a type");
		auto arg_type = *(value_type_ptr *) arg_type_value->global.host_address;

		argument_types.push_back(arg_type);
	}

	return _builtin_macro_fun(state, ret_type, argument_types);
}

#endif
