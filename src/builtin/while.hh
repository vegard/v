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

#ifndef V_BUILTIN_WHILE_HH
#define V_BUILTIN_WHILE_HH

#include "ast.hh"
#include "compile.hh"
#include "function.hh"
#include "scope.hh"
#include "value.hh"

static value_ptr builtin_macro_while(function &f, scope_ptr s, ast_node_ptr node)
{
	f.comment("while");

	if (node->type != AST_JUXTAPOSE)
		throw compile_error(node, "expected 'while <expression> <expression>'");

	auto condition_node = node->binop.lhs;
	auto body_node = node->binop.rhs;

	label loop_label;
	f.emit_label(loop_label);

	// condition
	auto condition_value = compile(f, s, condition_node);
	if (condition_value->type != builtin_type_boolean)
		throw compile_error(condition_node, "'while' condition must be boolean");

	label done_label;
	f.emit_jump_if_zero(condition_value, done_label);

	// body
	compile(f, s, body_node);
	f.emit_jump(loop_label);

	f.emit_label(done_label);

	// finalize
	f.link_label(loop_label);
	f.link_label(done_label);

	return std::make_shared<value>(VALUE_CONSTANT, builtin_type_void);
}

#endif