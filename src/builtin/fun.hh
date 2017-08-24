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

	value_ptr invoke(function_ptr f, scope_ptr s, ast_node_ptr node)
	{
		if (this->f != f)
			throw compile_error(node, "'return' used outside defining function");

		// The scope where we are used must be the scope where we
		// were defined or a child.
		if (!is_parent_of(this->s, s))
			throw compile_error(node, "'return' used outside defining scope");

		f->comment("return");

		auto v = compile(f, s, node);
		if (v->type != return_type)
			throw compile_error(node, "wrong return type for function");

		if (return_value)
			f->emit_move(v, return_value);

		f->emit_jump(return_label);

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
	const machine_register *it;

	args_allocator():
		it(std::cbegin(regs))
	{
	}

	machine_register next(ast_node_ptr node)
	{
		// TODO: use stack
		if (it == std::cend(regs))
			throw compile_error(node, "out of registers");

		return *(it++);
	}
};

// Low-level helper (for use after data has been extracted from syntax)
static value_ptr __construct_fun(value_type_ptr type, function_ptr f, scope_ptr s, ast_node_ptr node,
	std::vector<std::string> &args, ast_node_ptr body_node)
{
	auto new_f = std::make_shared<function>(false);
	new_f->emit_prologue();

	value_type_ptr return_type = type->return_type;
	value_ptr return_value;
	label return_label;

	args_allocator regs;

	// TODO: use multiple regs or pass on stack
	// AMD64 ABI: return types with non-trivial copy constructors or destructors are
	// passed through a pointer in the first argument.
	assert(return_type);
	if (return_type->size <= sizeof(unsigned long)) {
		return_value = new_f->alloc_local_value(return_type);
	} else {
		return_value = new_f->alloc_local_pointer_value(type->return_type);
		new_f->emit_move_address(regs.next(node), return_value);
	}

	auto new_scope = std::make_shared<scope>(s);
	new_scope->define_builtin_macro("return", std::make_shared<return_macro>(new_f, new_scope, return_type, return_value, return_label));

	for (unsigned int i = 0; i < args.size(); ++i) {
		auto arg_value = new_f->alloc_local_value(type->argument_types[i]);
		new_scope->define(node, args[i], arg_value);
		new_f->emit_move(regs.next(node), arg_value, 0);
	}

	auto v = compile(new_f, new_scope, body_node);
	auto v_type = v->type;
	if (v_type != return_type)
		throw compile_error(node, "wrong return type for function");

	new_f->emit_move(v, return_value);

	new_f->emit_label(return_label);
	if (return_type->size <= sizeof(unsigned long)) {
		new_f->emit_move(return_value, 0, RAX);
	} else {
		new_f->emit_move_address(return_value, RAX);
	}

	new_f->link_label(return_label);
	new_f->emit_epilogue();

	// We need this to keep the new function from getting freed when this
	// function returns.
	// TODO: Another solution?
	static std::set<function_ptr> functions;
	functions.insert(new_f);

	auto ret = std::make_shared<value>(VALUE_GLOBAL, type);
	void *mem = map(new_f);

	// XXX: why the indirection? I forgot why I did it this way.
	auto global = new void *;
	*global = mem;
	ret->global.host_address = (void *) global;

	disassemble((const uint8_t *) mem, new_f->bytes.size(), (uint64_t) mem, new_f->comments);
	return ret;
}

// actually compile a function body
//  - 'type' is the function type (signature)
//  - 'node' is the function body
static value_ptr _construct_fun(value_type_ptr type, function_ptr f, scope_ptr s, ast_node_ptr node)
{
	if (node->type != AST_JUXTAPOSE)
		throw compile_error(node, "expected (<argument types>...) <body>");

	auto args_node = node->binop.lhs;
	if (args_node->type != AST_BRACKETS)
		throw compile_error(node, "expected (<argument names>...)");

	std::vector<std::string> args;
	for (auto arg_node: traverse<AST_COMMA>(args_node->unop)) {
		if (arg_node->type != AST_SYMBOL_NAME)
			throw compile_error(node, "expected symbol for argument name");

		args.push_back(arg_node->symbol_name);
	}

	if (args.size() != type->argument_types.size())
		throw compile_error(node, "expected %u arguments; got %u", type->argument_types.size(), args.size());

	auto body_node = node->binop.rhs;
	return __construct_fun(type, f, s, node, args, body_node);
}

static value_ptr _call_fun(function_ptr f, scope_ptr s, value_ptr fn, ast_node_ptr node)
{
	if (node->type != AST_BRACKETS)
		throw compile_error(node, "expected parantheses");

	// TODO: save/restore caller save registers
	auto type = fn->type;

	std::vector<std::pair<ast_node_ptr, value_ptr>> args;
	for (auto arg_node: traverse<AST_COMMA>(node->unop))
		args.push_back(std::make_pair(arg_node, compile(f, s, arg_node)));

	if (args.size() != type->argument_types.size())
		throw compile_error(node, "expected %u arguments; got %u", type->argument_types.size(), args.size());

	args_allocator regs;

	for (unsigned int i = 0; i < args.size(); ++i) {
		auto arg_node = args[i].first;
		auto arg_value = args[i].second;

		if (type->argument_types[i] != arg_value->type)
			throw compile_error(arg_node, "wrong argument type");

		f->emit_move(arg_value, 0, regs.next(arg_node));
	}

	f->emit_call(fn);

	value_ptr ret_value = f->alloc_local_value(type->return_type);
	f->emit_move(RAX, ret_value, 0);
	return ret_value;
}

// Low-level helper (for use after data has been extracted from syntax)
static value_ptr _builtin_macro_fun(function_ptr f, scope_ptr s, value_type_ptr ret_type, std::vector<value_type_ptr> &argument_types)
{
	value_type_ptr type;

	// Create new type for this signature
	type = std::make_shared<value_type>();
	type->alignment = 8;
	type->size = 8;
	type->constructor = _construct_fun;
	type->argument_types = argument_types;
	type->return_type = ret_type;
	type->call = _call_fun;

	// XXX: refcounting
	auto type_value = std::make_shared<value>(VALUE_GLOBAL, builtin_type_type);
	auto type_copy = new value_type_ptr(type);
	type_value->global.host_address = (void *) type_copy;
	return type_value;
}

static value_ptr builtin_macro_fun(function_ptr f, scope_ptr s, ast_node_ptr node)
{
	// Extract parameters and code block from AST

	if (node->type != AST_JUXTAPOSE)
		throw compile_error(node, "expected 'fun <expression> (<expression>)'");

	auto ret_type_node = node->binop.lhs;
	auto ret_type_value = eval(f, s, ret_type_node);
	if (ret_type_value->storage_type != VALUE_GLOBAL)
		throw compile_error(ret_type_node, "return type must be known at compile time");
	if (ret_type_value->type != builtin_type_type)
		throw compile_error(ret_type_node, "return type must be an instance of a type");
	auto ret_type = *(value_type_ptr *) ret_type_value->global.host_address;

	auto brackets_node = node->binop.rhs;
	if (brackets_node->type != AST_BRACKETS)
		throw compile_error(brackets_node, "expected (<expression>...)");

	auto args_node = brackets_node->unop;

	std::vector<value_type_ptr> argument_types;
	for (auto arg_type_node: traverse<AST_COMMA>(args_node)) {
		value_ptr arg_type_value = eval(f, s, arg_type_node);
		if (arg_type_value->storage_type != VALUE_GLOBAL)
			throw compile_error(arg_type_node, "argument type must be known at compile time");
		if (arg_type_value->type != builtin_type_type)
			throw compile_error(arg_type_node, "argument type must be an instance of a type");
		auto arg_type = *(value_type_ptr *) arg_type_value->global.host_address;

		argument_types.push_back(arg_type);
	}

	return _builtin_macro_fun(f, s, ret_type, argument_types);
}

#endif
