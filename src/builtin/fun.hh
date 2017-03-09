//
//  The V programming language compiler
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

#include <set>

#include "ast.hh"
#include "compile.hh"
#include "function.hh"
#include "scope.hh"
#include "value.hh"

static value_ptr _call_fun(function &f, scope_ptr s, value_ptr fn, ast_node_ptr node)
{
	if (node->type != AST_BRACKETS)
		throw compile_error(node, "expected parantheses");

	// TODO: save/restore caller save registers
	auto type = fn->type;

	// TODO: abstract away ABI details
	machine_register args_regs[] = {
		RDI, RSI, RDX, RCX, R8, R9,
	};

	std::vector<std::pair<ast_node_ptr, value_ptr>> args;
	for (auto arg_node: traverse<AST_COMMA>(node->unop))
		args.push_back(std::make_pair(arg_node, compile(f, s, arg_node)));

	if (args.size() != type->argument_types.size())
		throw compile_error(node, "expected %u arguments; got %u", type->argument_types.size(), args.size());

	for (unsigned int i = 0; i < args.size(); ++i) {
		auto arg_node = args[i].first;
		auto arg_value = args[i].second;

		if (type->argument_types[i] != arg_value->type)
			throw compile_error(arg_node, "wrong argument type");

		f.emit_move(arg_value, args_regs[i]);
	}

	f.emit_call(fn);

	value_ptr ret_value = f.alloc_local_value(type->return_type);
	f.emit_move(RAX, ret_value);
	return ret_value;
}

static value_ptr builtin_macro_fun(function &f, scope_ptr s, ast_node_ptr node)
{
	// Extract parameters and code block from AST

	if (node->type != AST_JUXTAPOSE)
		throw compile_error(node, "expected 'fun (<expression>) <expression>'");

	auto brackets_node = node->binop.lhs;
	if (brackets_node->type != AST_BRACKETS)
		throw compile_error(brackets_node, "expected (<expression>...)");

	auto code_node = node->binop.rhs;
	auto args_node = brackets_node->unop;

	// TODO: abstract away ABI details
	machine_register args_regs[] = {
		RDI, RSI, RDX, RCX, R8, R9,
	};
	unsigned int current_arg = 0;

	auto new_f = std::make_shared<function>(false);
	new_f->emit_prologue();

	auto new_scope = std::make_shared<scope>(s);

	std::vector<value_type_ptr> argument_types;
	for (auto arg_node: traverse<AST_COMMA>(args_node)) {
		if (arg_node->type != AST_PAIR)
			throw compile_error(arg_node, "expected <name>: <type> pair");

		auto name_node = arg_node->binop.lhs;
		if (name_node->type != AST_SYMBOL_NAME)
			throw compile_error(arg_node, "argument name must be a symbol name");

		// Find the type by evaluating the <type> expression
		auto type_node = arg_node->binop.rhs;
		value_ptr arg_type_value = eval(f, s, type_node);
		if (arg_type_value->storage_type != VALUE_GLOBAL)
			throw compile_error(type_node, "argument type must be known at compile time");
		if (arg_type_value->type != builtin_type_type)
			throw compile_error(type_node, "argument type must be an instance of a type");
		auto arg_type = *(value_type_ptr *) arg_type_value->global.host_address;

		argument_types.push_back(arg_type);

		// Define arguments as local values
		value_ptr arg_value = new_f->alloc_local_value(arg_type);
		new_scope->define(node, name_node->literal_string, arg_value);

		// TODO: use multiple regs or pass on stack
		if (arg_type->size > sizeof(unsigned long))
			throw compile_error(arg_node, "argument too big to fit in register");

		// TODO: pass args on stack if there are too many to fit in registers
		if (current_arg >= sizeof(args_regs) / sizeof(*args_regs))
			throw compile_error(arg_node, "too many arguments");

		new_f->emit_move(args_regs[current_arg++], arg_value);
	}

	// v is the return value of the compiled expression
	auto v = compile(*new_f, new_scope, code_node);
	auto v_type = v->type;

	// TODO: use multiple regs or pass on stack
	if (v_type->size > sizeof(unsigned long))
		throw compile_error(code_node, "return value too big to fit in register");

	if (v_type->size)
		new_f->emit_move(v, RAX);

	new_f->emit_epilogue();

	// Now that we know the function's return type, we can finalize
	// the signature and either find or create a type to represent
	// the function signature.
	//.push_back(v_type);

	// We memoise function types so that two functions with the same
	// signature always get the same type
	static std::map<std::pair<std::vector<value_type_ptr>, value_type_ptr>, value_type_ptr> signature_cache;

	// TODO: This is a *bit* ugly
	auto signature = std::make_pair(argument_types, v_type);

	value_type_ptr ret_type;
	auto it = signature_cache.find(signature);
	if (it == signature_cache.end()) {
		// Create new type for this signature
		ret_type = std::make_shared<value_type>();
		ret_type->alignment = 8;
		ret_type->size = 8;
		ret_type->constructor = nullptr;
		ret_type->argument_types = argument_types;
		ret_type->return_type = v_type;
		ret_type->call = _call_fun;

		signature_cache[signature] = ret_type;
	} else {
		ret_type = it->second;
	}

	// We need this to keep the new function from getting freed when this
	// function returns.
	// TODO: Another solution?
	static std::set<function_ptr> functions;
	functions.insert(new_f);

	auto ret = std::make_shared<value>(VALUE_GLOBAL, ret_type);
	void *mem = map(new_f);

	auto global = new void *;
	*global = mem;
	ret->global.host_address = (void *) global;

	disassemble((const uint8_t *) mem, new_f->bytes.size(), (uint64_t) mem, new_f->comments);
	return ret;
}

#endif
