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

#ifndef V_TYPES_HH
#define V_TYPES_HH

#include <memory>

#include "./compile.hh"
#include "./function.hh"
#include "./scope.hh"
#include "./value.hh"
#include "builtin/u64.hh"

static auto builtin_type_context = std::make_shared<value_type>(value_type {
	.alignment = alignof(context_ptr),
	.size = sizeof(context_ptr),
});

static auto builtin_type_function = std::make_shared<value_type>(value_type {
	.alignment = alignof(function_ptr),
	.size = sizeof(function_ptr),
});

static auto builtin_type_scope = std::make_shared<value_type>(value_type {
	.alignment = alignof(scope_ptr),
	.size = sizeof(scope_ptr),
	.constructor = nullptr,
	.argument_types = std::vector<value_type_ptr>(),
	.return_type = nullptr,
	.members = std::map<std::string, member_ptr>({
	}),
});

static auto builtin_type_ast_node = std::make_shared<value_type>(value_type {
	.alignment = alignof(ast_node_ptr),
	.size = sizeof(ast_node_ptr),
});

static auto builtin_type_compile_state = std::make_shared<value_type>(value_type {
	.alignment = alignof(compile_state_ptr),
	.size = sizeof(compile_state_ptr),
	.constructor = nullptr,
	.argument_types = std::vector<value_type_ptr>(),
	.return_type = nullptr,
});

#endif
