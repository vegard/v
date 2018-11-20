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

#ifndef V_BUILTIN_STRUCT_HH
#define V_BUILTIN_STRUCT_HH

#include "../ast.hh"
#include "../compile.hh"
#include "../function.hh"
#include "../scope.hh"
#include "../value.hh"

struct struct_field: member {
	value_type_ptr field_type;
	unsigned int offset;

	struct_field(value_type_ptr field_type, unsigned int offset):
		field_type(field_type),
		offset(offset)
	{
	}

	value_ptr invoke(const compile_state &state, value_ptr v, ast_node_ptr node)
	{
		auto ret_value = std::make_shared<value>(state.context, v->storage_type, field_type);
		switch (v->storage_type) {
		case VALUE_GLOBAL:
			ret_value->global.host_address = (void *) ((unsigned long) v->global.host_address + offset);
			break;
		case VALUE_LOCAL:
			ret_value->local.offset = v->local.offset + offset;
			break;
		case VALUE_LOCAL_POINTER:
			// TODO
			// We probably need to load the value of the pointer and then
			// add the offset to it and return a new local.
			assert(false);
			break;
		default:
			assert(false);
		}

		return ret_value;
	}
};

static value_ptr _struct_constructor(value_type_ptr v, const compile_state &state, ast_node_ptr node)
{
	// TODO: zero value
	// TODO: move allocation out!
	return state.function->alloc_local_value(state.context, v);
}

// handle 'x := y' in structs
struct struct_declare_macro: macro {
	value_type_ptr struct_type;
	unsigned int offset;

	struct_declare_macro(value_type_ptr struct_type):
		struct_type(struct_type),
		offset(0)
	{
	}

	value_ptr invoke(const compile_state &state, ast_node_ptr node)
	{
		if (node->type != AST_JUXTAPOSE)
			state.error(node, "expected juxtaposition");

		auto name_node = state.get_node(node->binop.lhs);
		if (name_node->type != AST_SYMBOL_NAME)
			state.error(name_node, "expected symbol for member name");

		auto field_name = state.get_symbol_name(name_node);

		// TODO: we should call eval() here with a scope that
		// "undefines" _declare so we get the normal definition
		// of it
		auto type_node = state.get_node(node->binop.rhs);
		auto type_value = eval(state, type_node);
		assert(type_value->storage_type == VALUE_GLOBAL);
		state.expect_type(node, type_value, builtin_type_type);

		auto field_type = *(value_type_ptr *) type_value->global.host_address;

		// Align the field's offset according to the field's type's alignment
		// TODO: factor out alignment code
		assert(field_type->alignment);
		assert((field_type->alignment & (field_type->alignment - 1)) == 0);
		offset = (offset + field_type->alignment - 1) & ~(field_type->alignment - 1);

		//printf("member: name %s\n", name_node->literal_string.c_str());
		struct_type->members[field_name] = std::make_shared<struct_field>(field_type, offset);

		offset += field_type->size;

		return builtin_value_void;
	}
};

static value_ptr builtin_macro_struct(const compile_state &state, ast_node_ptr node)
{
	auto type = std::make_shared<value_type>();
	// TODO: We can get away with smaller alignment for small structs
	type->alignment = alignof(unsigned long);
	type->constructor = &_struct_constructor;

	auto new_scope = std::make_shared<scope>(state.scope);
	auto macro = std::make_shared<struct_declare_macro>(type);
	new_scope->define_builtin_macro("_declare", macro);

	compile(state.set_scope(new_scope), node);

	// Align the final size for arrays
	type->size = (macro->offset + type->alignment - 1) & ~(type->alignment - 1);

	// XXX: refcounting
	auto type_value = std::make_shared<value>(state.context, VALUE_GLOBAL, builtin_type_type);
	auto type_copy = new value_type_ptr(type);
	type_value->global.host_address = (void *) type_copy;
	return type_value;
}

#endif
