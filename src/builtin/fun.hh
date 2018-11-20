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
	label &return_label;

	return_macro(function_ptr f, scope_ptr s, value_type_ptr return_type, value_ptr return_value, label &return_label):
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

// TODO: abstract away ABI details

static const machine_register regs[] = {
	RDI, RSI, RDX, RCX, R8, R9,
};

struct args_allocator {
	const compile_state &state;
	const machine_register *it;

	args_allocator(const compile_state &state):
		state(state),
		it(std::cbegin(regs))
	{
	}

	machine_register next(ast_node_ptr node)
	{
		// TODO: use stack
		if (it == std::cend(regs))
			state.error(node, "out of registers");

		return *(it++);
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

	auto rhs = compile(state, state.get_node(node->binop.rhs));
	auto val = state.function->alloc_local_value(state.context, rhs->type);
	state.scope->define(state.function, node, lhs->symbol_name, val);
	state.function->emit_move(rhs, val);
	return val;
}

// Low-level helper (for use after data has been extracted from syntax)
static value_ptr __construct_fun(value_type_ptr type, const compile_state &state, ast_node_ptr node,
	std::vector<std::string> &args, ast_node_ptr body_node)
{
	auto c = state.context;

	auto new_f = std::make_shared<function>(!state.objects);

	new_f->emit_prologue();

	value_type_ptr return_type = type->return_type;
	value_ptr return_value;
	label return_label;

	args_allocator regs(state);

	// TODO: use multiple regs or pass on stack
	// AMD64 ABI: return types with non-trivial copy constructors or destructors are
	// passed through a pointer in the first argument.
	assert(return_type);
	if (return_type->size <= sizeof(unsigned long)) {
		return_value = new_f->alloc_local_value(c, return_type);
	} else {
		return_value = new_f->alloc_local_pointer_value(c, type->return_type);
		new_f->emit_move_address(regs.next(node), return_value);
	}

	auto new_scope = std::make_shared<scope>(state.scope);
	new_scope->define_builtin_macro("_define", fun_define_macro);
	new_scope->define_builtin_macro("return", std::make_shared<return_macro>(new_f, new_scope, return_type, return_value, return_label));

	for (unsigned int i = 0; i < args.size(); ++i) {
		auto arg_type = type->argument_types[i];
		value_ptr arg_value;

		if (arg_type->size <= sizeof(unsigned long)) {
			arg_value = new_f->alloc_local_value(c, arg_type);
			new_f->emit_move(regs.next(node), arg_value, 0);
		} else {
			arg_value = new_f->alloc_local_pointer_value(c, arg_type);
			new_f->emit_move_address(regs.next(node), arg_value);
		}

		new_scope->define(new_f, node, args[i], arg_value);
	}

	auto v = compile(state.set_function(new_f, new_scope), body_node);
	auto v_type = v->type;
	if (v_type != return_type)
		state.error(node, "wrong return type for function");

	new_f->emit_move(v, return_value);

	new_f->emit_label(return_label);
	if (return_type->size <= sizeof(unsigned long)) {
		new_f->emit_move(return_value, 0, RAX);
	} else {
		new_f->emit_move_address(return_value, RAX);
	}

	new_f->link_label(return_label);
	new_f->emit_epilogue();

	if (state.objects) {
		// target
		return std::make_shared<value>(nullptr, type, state.new_object(new_f->this_object));
	} else {
		// host

		// We need this to keep the new function from getting freed when this
		// function returns.
		// TODO: Another solution?
		static std::set<function_ptr> functions;
		functions.insert(new_f);

		auto ret = std::make_shared<value>(nullptr, VALUE_GLOBAL, type);
		void *mem = map(new_f);

		// XXX: why the indirection? I forgot why I did it this way.
		auto global = new void *;
		*global = mem;
		ret->global.host_address = (void *) global;

		if (global_disassemble)
			disassemble((const uint8_t *) mem, new_f->bytes.size(), (uint64_t) mem, new_f->this_object->comments);

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

		args.push_back(arg_node->symbol_name);
	}

	if (args.size() != type->argument_types.size())
		state.error(node, "expected $ arguments; got $", type->argument_types.size(), args.size());

	auto body_node = state.get_node(node->binop.rhs);
	return __construct_fun(type, state, node, args, body_node);
}

// Low-level helper (for use after data has been extracted from syntax)
static value_ptr __call_fun(const compile_state &state, value_ptr fn, ast_node_ptr node, std::vector<std::pair<ast_node_ptr, value_ptr>> args)
{
	auto f = state.function;

	// TODO: save/restore caller save registers
	auto type = fn->type;

	if (args.size() != type->argument_types.size())
		state.error(node, "expected $ arguments; got $", type->argument_types.size(), args.size());

	args_allocator regs(state);

	auto return_type = type->return_type;
	value_ptr return_value;
	if (return_type == builtin_type_void)
		return_value = builtin_value_void;
	else
		return_value = f->alloc_local_value(state.context, return_type);

	// TODO: should really use a "non-trivial *structor" flag
	if (return_type->size <= sizeof(unsigned long))
		;
	else
		f->emit_move_address(return_value, regs.next(node));

	for (unsigned int i = 0; i < args.size(); ++i) {
		auto arg_node = args[i].first;
		auto arg_value = args[i].second;

		auto arg_type = arg_value->type;
		if (arg_type != type->argument_types[i])
			state.error(arg_node, "wrong argument type");

		// TODO: should really use a "non-trivial *structor" flag
		if (arg_type->size <= sizeof(unsigned long))
			f->emit_move(arg_value, 0, regs.next(arg_node));
		else
			f->emit_move_address(arg_value, regs.next(arg_node));
	}

	f->emit_call(fn);

	// TODO: should really use a "non-trivial *structor" flag
	if (return_type->size > 0 && return_type->size <= sizeof(unsigned long))
		f->emit_move(RAX, return_value, 0);

	return return_value;
}

static value_ptr _call_fun(const compile_state &state, value_ptr fn, ast_node_ptr node)
{
	if (node->type != AST_BRACKETS)
		state.error(node, "expected parantheses");

	std::vector<std::pair<ast_node_ptr, value_ptr>> args;
	for (auto arg_node: traverse<AST_COMMA>(state.source->tree, state.get_node(node->unop)))
		args.push_back(std::make_pair(arg_node, compile(state, arg_node)));

	return __call_fun(state, fn, node, args);
}

// Low-level helper (for use after data has been extracted from syntax)
static value_ptr _builtin_macro_fun(context_ptr c, value_type_ptr ret_type, const std::vector<value_type_ptr> &argument_types)
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
	auto type_value = std::make_shared<value>(nullptr, VALUE_GLOBAL, builtin_type_type);
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

	return _builtin_macro_fun(state.context, ret_type, argument_types);
}

#endif
