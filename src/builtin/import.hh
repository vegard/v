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

#ifndef V_BUILTIN_IMPORT_HH
#define V_BUILTIN_IMPORT_HH

#include "../ast.hh"
#include "../compile.hh"
#include "../function.hh"
#include "../namespace.hh"
#include "../scope.hh"
#include "../value.hh"

static value_ptr builtin_macro_import(const compile_state &state, ast_node_ptr node)
{
	// TODO: take dotted path instead of literal filename
	state.expect_type(node, AST_LITERAL_STRING);

	// XXX: restrict accessible paths?
	// TODO: search multiple paths rather than just the current dir
	auto literal_string = state.get_literal_string(node);
	source_file_ptr source = std::make_shared<mmap_source_file>(literal_string.c_str());

	int source_node;
	try {
		source_node = source->parse();
	} catch (const parse_error &e) {
		throw compile_error(source, e.pos, e.end, "parse error: $", e.what());
	}

	auto new_scope = std::make_shared<scope>(state.scope);
	compile(state.set_source(source, new_scope), source->tree.get(source_node));

	// Create new namespace with the contents of the new scope as members
	auto members = std::map<std::string, member_ptr>();
	for (auto &it: new_scope->contents) {
		// TODO: preserve location of definition
		members[it.first] = std::make_shared<namespace_member>(it.second.val);
	}

	auto new_namespace = std::make_shared<value>(nullptr, VALUE_CONSTANT,
		std::make_shared<value_type>(value_type {
			.alignment = 0,
			.size = 0,
			.constructor = nullptr,
			.argument_types = std::vector<value_type_ptr>(),
			.return_type = nullptr,
			.members = members,
		})
	);

	return new_namespace;
}

#endif
