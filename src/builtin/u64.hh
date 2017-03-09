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

#include "compile.hh"
#include "value.hh"

static value_ptr builtin_type_u64_constructor(function &, scope_ptr, ast_node_ptr);
static value_ptr builtin_type_u64_add(function &f, scope_ptr s, value_ptr lhs, ast_node_ptr node);

static auto builtin_type_u64 = std::make_shared<value_type>(value_type {
	.alignment = 8,
	.size = 8,
	.constructor = &builtin_type_u64_constructor,
	.argument_types = std::vector<value_type_ptr>(),
	.return_type = value_type_ptr(),
	.call = nullptr,
	.add = &builtin_type_u64_add,
});

static value_ptr builtin_type_u64_constructor(function &f, scope_ptr s, ast_node_ptr node)
{
	// TODO: support conversion from other integer types?
	if (node->type != AST_LITERAL_INTEGER)
		throw compile_error(node, "expected literal integer");

	auto ret = std::make_shared<value>(VALUE_GLOBAL, builtin_type_u64);
	if (!node->literal_integer.fits_ulong_p())
		throw compile_error(node, "literal integer is too large to fit in u64");

	auto global = new uint64_t;
	*global = node->literal_integer.get_si();
	ret->global.host_address = (void *) global;
	return ret;
}

// TODO: maybe this should really be a function rather than a macro
static value_ptr builtin_type_u64_add(function &f, scope_ptr s, value_ptr lhs, ast_node_ptr node)
{
	auto rhs = compile(f, s, node->binop.rhs);
	if (rhs->type != lhs->type)
		throw compile_error(node->binop.rhs, "expected u64");

	auto ret = f.alloc_local_value(lhs->type);
	f.emit_add(lhs, rhs, ret);
	return ret;
}

#endif
