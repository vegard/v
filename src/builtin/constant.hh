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

#ifndef V_BUILTIN_CONSTANT_HH
#define V_BUILTIN_CONSTANT_HH

#include "../ast.hh"
#include "../compile.hh"
#include "../function.hh"
#include "../scope.hh"
#include "../value.hh"

struct constant_define_macro: macro {
	scope_ptr s;

	constant_define_macro(scope_ptr s):
		s(s)
	{
	}

	value_ptr invoke(ast_node_ptr node)
	{
		if (node->type != AST_JUXTAPOSE)
			state->error(node, "expected juxtaposition");

		auto lhs = get_node(node->binop.lhs);
		if (lhs->type != AST_SYMBOL_NAME)
			state->error(node, "definition of non-symbol");

		auto symbol_name = get_symbol_name(lhs);

		// TODO: We shouldn't be generating any code -- it must be a compile-time constant expression.
		auto rhs = (use_scope(s), compile(get_node(node->binop.rhs)));
		assert(rhs->type->size == 8);

		auto val = s->make_value(state->context, VALUE_CONSTANT, rhs->type);
		switch (rhs->storage_type) {
		case VALUE_GLOBAL:
			// XXX: Oh man, this is so wrong.
			val->constant.u64 = *(uint64_t *) rhs->global.host_address;
			break;
		case VALUE_TARGET_GLOBAL:
			{
				auto obj = (*state->objects)[rhs->target_global.object_id];
				assert(obj->relocations.empty());
				// XXX: Oh man, this is so wrong.
				val->constant.u64 = *(uint64_t *) obj->bytes.data();
			}
			break;
		case VALUE_CONSTANT:
			val->constant.u64 = rhs->constant.u64;
			break;
		default:
			assert(false);
		}

		s->define(state->function, state->source, node, symbol_name, val);
		return &builtin_value_void;
	}
};

static value_ptr builtin_macro_constant(ast_node_ptr node)
{
	auto old_scope = state->scope;
	auto new_scope = std::make_shared<scope>(state->scope);
	new_scope->define_builtin_macro("_define", std::make_shared<constant_define_macro>(old_scope));

	return (use_scope(new_scope), compile(node));
}

#endif
