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

#ifndef V_BUILTIN_U64_H
#define V_BUILTIN_U64_H

#include "../compile.hh"
#include "../value.hh"

struct macrofy_callback_member: member {
	value_ptr (*fn)(value_ptr, ast_node_ptr);

	macrofy_callback_member(value_ptr (*fn)(value_ptr, ast_node_ptr)):
		fn(fn)
	{
	}

	value_ptr invoke(value_ptr v, ast_node_ptr node)
	{
		auto m = std::make_shared<val_macro>(fn, v);
		auto macro_value = state->scope->make_value(nullptr, VALUE_GLOBAL, builtin_type_macro);
		auto macro_copy = new macro_ptr(m);
		macro_value->global.host_address = (void *) macro_copy;
		return macro_value;
	}
};

static value_ptr builtin_type_u64_constructor(value_type_ptr, ast_node_ptr);
static value_ptr builtin_type_u64_add(value_ptr lhs, ast_node_ptr node);
static value_ptr builtin_type_u64_subtract(value_ptr lhs, ast_node_ptr node);
static value_ptr builtin_type_u64_less(value_ptr lhs, ast_node_ptr node);

static auto builtin_type_u64 = std::make_shared<value_type>(value_type {
	.alignment = 8,
	.size = 8,
	.constructor = &builtin_type_u64_constructor,
	.argument_types = std::vector<value_type_ptr>(),
	.return_type = value_type_ptr(),
	.members = std::map<std::string, member_ptr>({
		{"_add", std::make_shared<macrofy_callback_member>(&builtin_type_u64_add)},
		{"_subtract", std::make_shared<macrofy_callback_member>(&builtin_type_u64_subtract)},
		{"_less", std::make_shared<macrofy_callback_member>(&builtin_type_u64_less)},
	}),
});

static value_ptr builtin_type_u64_constructor(value_type_ptr, ast_node_ptr node)
{
	// TODO: support conversion from other integer types?
	if (node->type != AST_LITERAL_INTEGER)
		state->error(node, "expected literal integer");

	auto literal_integer = state->get_literal_integer(node);
	if (!literal_integer.fits_ulong_p())
		state->error(node, "literal integer is too large to fit in u64");

	auto ret = state->scope->make_value(nullptr, VALUE_CONSTANT, builtin_type_u64);
	ret->constant.u64 = literal_integer.get_si();
	return ret;
}

// TODO: maybe this should really be a function rather than a macro
static value_ptr builtin_type_u64_add(value_ptr lhs, ast_node_ptr node)
{
	auto rhs = compile(node);
	if (rhs->type != lhs->type)
		state->error(node, "expected u64");

	auto ret = state->function->alloc_local_value(state->scope, state->context, lhs->type);
	state->function->emit_add(lhs, rhs, ret);
	return ret;
}

static value_ptr builtin_type_u64_subtract(value_ptr lhs, ast_node_ptr node)
{
	auto rhs = compile(node);
	if (rhs->type != lhs->type)
		state->error(node, "expected u64");

	auto ret = state->function->alloc_local_value(state->scope, state->context, lhs->type);
	state->function->emit_sub(lhs, rhs, ret);
	return ret;
}

static value_ptr builtin_type_u64_less(value_ptr lhs, ast_node_ptr node)
{
	auto rhs = compile(node);
	if (rhs->type != lhs->type)
		state->error(node, "expected u64");

	auto ret = state->function->alloc_local_value(state->scope, state->context, builtin_type_boolean);
	state->function->emit_compare(function::CMP_LESS, lhs, rhs, ret);
	return ret;
}

#endif
