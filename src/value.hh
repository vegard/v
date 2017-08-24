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

#ifndef V_VALUE_HH
#define V_VALUE_HH

enum value_storage_type {
	VALUE_GLOBAL,
	// a local (on-stack) value
	VALUE_LOCAL,
	// a local (on-stack) pointer to the value itself
	VALUE_LOCAL_POINTER,
	VALUE_CONSTANT,
};

struct value_type;
typedef std::shared_ptr<value_type> value_type_ptr;

struct value;
typedef std::shared_ptr<value> value_ptr;

struct function;
typedef std::shared_ptr<function> function_ptr;

struct scope;
typedef std::shared_ptr<scope> scope_ptr;

struct ast_node;
typedef std::shared_ptr<ast_node> ast_node_ptr;

typedef value_ptr (*operator_fn_type)(function_ptr, scope_ptr, value_ptr, ast_node_ptr);

struct value_type {
	// TODO
	unsigned int alignment;
	unsigned int size;

	// TODO
	value_ptr (*constructor)(value_type_ptr, function_ptr, scope_ptr, ast_node_ptr);

	// Operators
	std::vector<value_type_ptr> argument_types;
	value_type_ptr return_type;
	value_ptr (*call)(function_ptr, scope_ptr, value_ptr, ast_node_ptr);

	operator_fn_type add;
	operator_fn_type subtract;

	operator_fn_type less;
	operator_fn_type less_equal;
	operator_fn_type greater;
	operator_fn_type greater_equal;
};

struct value {
	value_storage_type storage_type;

	union {
		struct {
			void *host_address;
		} global;

		struct {
			// offset in the local stack frame
			unsigned int offset;
		} local;

		struct {
			//ast_node_ptr node;
			uint64_t u64;
		} constant;
	};

	value_type_ptr type;

	value()
	{
	}

	value(value_storage_type storage_type, value_type_ptr type):
		storage_type(storage_type),
		type(type)
	{
	}

	~value()
	{
	}
};

// Some builtin types

static auto builtin_type_void = std::make_shared<value_type>(value_type{0, 0});
static auto builtin_type_type = std::make_shared<value_type>(value_type{alignof(value_type_ptr), sizeof(value_type_ptr)});

// TODO: make boolean size 1 (requires adjustments to the assembly code generation)
static auto builtin_type_boolean = std::make_shared<value_type>(value_type{8, 8});

// TODO: "int" is 64-bit for the time being, see copmile(AST_LITERAL_INTEGER) in compile.hh
//static value_type builtin_type_int = {alignof(mpz_class), sizeof(mpz_class)};
static auto builtin_type_int = std::make_shared<value_type>(value_type{8, 8});

#endif
