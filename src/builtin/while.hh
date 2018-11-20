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

#include "../ast.hh"
#include "../compile.hh"
#include "../function.hh"
#include "../macro.hh"
#include "../scope.hh"
#include "../value.hh"

struct break_macro: macro {
	function_ptr f;
	scope_ptr s;
	label &done_label;

	break_macro(function_ptr f, scope_ptr s, label &done_label):
		f(f),
		s(s),
		done_label(done_label)
	{
	}

	value_ptr invoke(const compile_state &state, ast_node_ptr node)
	{
		if (state.function != this->f)
			state.error(node, "'break' used outside defining function");

		// The scope where we are used must be the scope where we
		// were defined or a child.
		if (!is_parent_of(this->s, state.scope))
			state.error(node, "'break' used outside defining scope");

		state.function->comment("break");
		state.function->emit_jump(done_label);

		return builtin_value_void;
	}
};

struct continue_macro: macro {
	function_ptr f;
	scope_ptr s;
	label &loop_label;

	continue_macro(function_ptr f, scope_ptr s, label &loop_label):
		f(f),
		s(s),
		loop_label(loop_label)
	{
	}

	value_ptr invoke(const compile_state &state, ast_node_ptr node)
	{
		if (state.function != this->f)
			state.error(node, "'continue' used outside defining function");

		// The scope where we are used must be the scope where we
		// were defined or a child.
		if (!is_parent_of(this->s, state.scope))
			state.error(node, "'continue' used outside defining scope");

		state.function->comment("continue");
		state.function->emit_jump(loop_label);

		return builtin_value_void;
	}
};


static value_ptr builtin_macro_while(const compile_state &state, ast_node_ptr node)
{
	auto c = state.context;
	auto f = state.function;

	f->comment("while");

	if (node->type != AST_JUXTAPOSE)
		state.error(node, "expected 'while <expression> <expression>'");

	auto condition_node = state.get_node(node->binop.lhs);
	auto body_node = state.get_node(node->binop.rhs);

	label loop_label;
	f->emit_label(loop_label);

	// condition
	auto condition_value = compile(state, condition_node);
	if (condition_value->type != builtin_type_boolean)
		state.error(condition_node, "'while' condition must be boolean");

	label done_label;
	f->emit_jump_if_zero(condition_value, done_label);

	// body
	auto new_scope = std::make_shared<scope>(state.scope);
	new_scope->define_builtin_macro("break", std::make_shared<break_macro>(f, new_scope, done_label));
	new_scope->define_builtin_macro("continue", std::make_shared<break_macro>(f, new_scope, loop_label));

	compile(state.set_scope(new_scope), body_node);
	f->emit_jump(loop_label);

	f->emit_label(done_label);

	// finalize
	f->link_label(loop_label);
	f->link_label(done_label);

	return builtin_value_void;
}

#endif
