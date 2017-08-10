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

#ifndef V_BUILTIN_DEFINE_HH
#define V_BUILTIN_DEFINE_HH

#include "../ast.hh"
#include "../compile.hh"
#include "../function.hh"
#include "../scope.hh"
#include "../value.hh"

static value_ptr builtin_macro_define(function_ptr f, scope_ptr s, ast_node_ptr node)
{
	if (node->type != AST_JUXTAPOSE)
		throw compile_error(node, "expected juxtaposition");

	auto lhs = node->binop.lhs;
	if (lhs->type != AST_SYMBOL_NAME)
		throw compile_error(node, "definition of non-symbol");

	auto rhs = compile(f, s, node->binop.rhs);
	value_ptr val;
	if (f->target_jit) {
		// For functions that are run at compile-time, we allocate
		// a new global value. The _name_ is still scoped as usual,
		// though.
		val = std::make_shared<value>(VALUE_GLOBAL, rhs->type);
		auto global = new uint8_t[rhs->type->size];
		val->global.host_address = (void *) global;
	} else {
		// Create new local
		val = f->alloc_local_value(rhs->type);
	}
	s->define(node, lhs->symbol_name, val);
	f->emit_move(rhs, val);

	return val;
}

#endif
