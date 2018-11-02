//
//  V compiler
//  Copyright (C) 2018  Vegard Nossum <vegard.nossum@gmail.com>
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

#ifndef V_BUILTIN_ELF_HH
#define V_BUILTIN_ELF_HH

#include <array>
#include <set>
#include <vector>

#include "../ast.hh"
#include "../compile.hh"
#include "../function.hh"
#include "../scope.hh"
#include "../value.hh"
#include "../builtin/str.hh"

struct elf_data {
	value_ptr entry_point;
	std::map<std::string, value_ptr> exports;

	elf_data():
		entry_point(builtin_value_void)
	{
	}
};

struct entry_macro: macro {
	scope_ptr s;
	elf_data &elf;

	entry_macro(scope_ptr s, elf_data &elf):
		s(s),
		elf(elf)
	{
	}

	value_ptr invoke(const compile_state &state, ast_node_ptr node)
	{
		if (!is_parent_of(this->s, state.scope))
			state.error(node, "'entry' used outside defining scope");

		auto entry_value = eval(state, node);
		if (entry_value->storage_type != VALUE_GLOBAL)
			state.error(node, "entry point must be known at compile time");

		elf.entry_point = entry_value;
		return builtin_value_void;
	}
};

struct export_define_macro: macro {
	scope_ptr s;
	elf_data &elf;

	export_define_macro(scope_ptr s, elf_data &elf):
		s(s),
		elf(elf)
	{
	}

	value_ptr invoke(const compile_state &state, ast_node_ptr node)
	{
		//printf("define something\n");
		//serializer().serialize(std::cout, node);

		if (node->type != AST_JUXTAPOSE)
			state.error(node, "expected juxtaposition");

		auto lhs = node->binop.lhs;
		if (lhs->type != AST_SYMBOL_NAME)
			state.error(node, "definition of non-symbol");

		// TODO: create new value?
		auto rhs = compile(state.set_scope(s), node->binop.rhs);
		s->define(state.function, node, lhs->symbol_name, rhs);

		elf.exports[lhs->symbol_name] = rhs;
		return builtin_value_void;
	}
};

struct export_macro: macro {
	scope_ptr s;
	elf_data &elf;

	export_macro(scope_ptr s, elf_data &elf):
		s(s),
		elf(elf)
	{
	}

	value_ptr invoke(const compile_state &state, ast_node_ptr node)
	{
		if (!is_parent_of(this->s, state.scope))
			state.error(node, "'export' used outside defining scope");

		// TODO: we really need to implement read vs. write scopes so
		// that when the user defines something it still becomes visible
		// in the parent scope
		auto new_scope = std::make_shared<scope>(state.scope);
		new_scope->define_builtin_macro("_define", std::make_shared<export_define_macro>(s, elf));

		return eval(state.set_scope(new_scope), node);
	}
};

static value_ptr builtin_macro_elf(const compile_state &state, ast_node_ptr node)
{
	if (node->type != AST_JUXTAPOSE)
		state.error(node, "expected 'elf filename:<expression> <expression>'");

	auto filename_node = node->binop.lhs;
	auto filename_value = eval(state, filename_node);
	if (filename_value->storage_type != VALUE_GLOBAL)
		state.error(filename_node, "output filename must be known at compile time");
	if (filename_value->type != builtin_type_str)
		state.error(filename_node, "output filename must be a string");
	auto filename = *(std::string *) filename_value->global.host_address;

	elf_data elf;

	auto new_scope = std::make_shared<scope>(state.scope);
	new_scope->define_builtin_macro("entry", std::make_shared<entry_macro>(new_scope, elf));
	new_scope->define_builtin_macro("export", std::make_shared<export_macro>(new_scope, elf));

	auto expr_node = node->binop.rhs;
	auto expr_value = eval(state.set_scope(new_scope), expr_node);

	// TODO: error handling, temporaries, etc.
	int fd = open(filename.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
	if (fd == -1)
		state.error(filename_node, "couldn't open '$' for writing: $", filename.c_str(), strerror(errno));

	// TODO: is this check sufficient?
	if (elf.entry_point != builtin_value_void) {
		assert(elf.entry_point->storage_type == VALUE_GLOBAL);
	}

	// TODO: traverse entry point + exports
	for (const auto it: elf.exports) {
		//printf("export: %s\n", it.first.c_str());
	}

	close(fd);
	return builtin_value_void;
}

#endif
