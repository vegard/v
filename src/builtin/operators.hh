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

#ifndef V_BUILTIN_ADD_HH
#define V_BUILTIN_ADD_HH

#include "../ast.hh"
#include "../compile.hh"
#include "../function.hh"
#include "../scope.hh"
#include "../value.hh"

template<operator_fn_type value_type::*operator_fn>
static value_ptr call_operator_fn(context_ptr c, function_ptr f, scope_ptr s, ast_node_ptr node)
{
	// So this is probably a result of something like (x + y), which got
	// parsed as (juxtapose _add (juxtapose x y)).
	//
	// Compiling the "juxtapose" decided we're a macro, and "node" here
	// refers to the (juxtapose x y) part.
	//
	// What we'd like to do is to evaluate 'x' to figure out what type
	// it is. Once we know its type, we can call that type's ->add()
	// operator.
	//
	// In general, we should be careful about "type only" evaluations
	// because it's more expensive to first evaluate the type and then
	// evaluate the type AND value than to just evaluate the type and
	// the value at the same time.
	//
	// However, this allows operators to be macros, which is a very
	// powerful feature.

	// TODO: bad error message
	if (node->type != AST_JUXTAPOSE)
		throw compile_error(node, "expected juxtaposition");

	// TODO: only evaluate the type so we don't evaluate the value twice
	auto lhs = compile(c, f, s, node->binop.lhs);
	auto lhs_type = lhs->type;

	auto callback_fn = (*lhs_type).*operator_fn;
	if (!callback_fn)
		throw compile_error(node, "type doesn't support operation");

	return callback_fn(c, f, s, lhs, node);
}

static value_ptr builtin_macro_add(context_ptr c, function_ptr f, scope_ptr s, ast_node_ptr node)
{
	return call_operator_fn<&value_type::add>(c, f, s, node);
}

static value_ptr builtin_macro_subtract(context_ptr c, function_ptr f, scope_ptr s, ast_node_ptr node)
{
	return call_operator_fn<&value_type::subtract>(c, f, s, node);
}

static value_ptr builtin_macro_less(context_ptr c, function_ptr f, scope_ptr s, ast_node_ptr node)
{
	return call_operator_fn<&value_type::less>(c, f, s, node);
}

static value_ptr builtin_macro_less_equal(context_ptr c, function_ptr f, scope_ptr s, ast_node_ptr node)
{
	return call_operator_fn<&value_type::less_equal>(c, f, s, node);
}

static value_ptr builtin_macro_greater(context_ptr c, function_ptr f, scope_ptr s, ast_node_ptr node)
{
	return call_operator_fn<&value_type::greater>(c, f, s, node);
}

static value_ptr builtin_macro_greater_equal(context_ptr c, function_ptr f, scope_ptr s, ast_node_ptr node)
{
	return call_operator_fn<&value_type::greater_equal>(c, f, s, node);
}

#endif
