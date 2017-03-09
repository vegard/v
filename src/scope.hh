//
//  The V programming language compiler
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

#ifndef V_SCOPE_HH
#define V_SCOPE_HH

#include <map>
#include <memory>
#include <string>

#include "ast.hh"
#include "value.hh"

struct scope;
typedef std::shared_ptr<scope> scope_ptr;

// Map symbol names to values.
// TODO: keep track of _where_ a symbol was defined?
struct scope {
	scope_ptr parent;
	std::map<std::string, value_ptr> contents;

	scope(scope_ptr parent = nullptr):
		parent(parent)
	{
	}

	~scope()
	{
	}

	void define(const ast_node_ptr def, const std::string name, value_ptr val)
	{
		contents[name] = val;
	}

	// Helper for defining builtin types
	// NOTE: builtin types are always global
	void define_builtin_type(const std::string name, value_type_ptr type)
	{
		auto type_value = std::make_shared<value>(VALUE_GLOBAL, builtin_type_type);
		auto type_copy = new value_type_ptr(type);
		type_value->global.host_address = (void *) type_copy;
		contents[name] = type_value;
	}

	// Helper for defining builtin macros
	// NOTE: builtin macros are always global
	void define_builtin_macro(const std::string name, value_ptr (*fn)(function &, scope_ptr, ast_node_ptr))
	{
		auto macro_value = std::make_shared<value>(VALUE_GLOBAL, builtin_type_macro);
		macro_value->global.host_address = (void *) fn;
		contents[name] = macro_value;
	}

	value_ptr lookup(const std::string name)
	{
		auto it = contents.find(name);
		if (it != contents.end())
			return it->second;

		// Recursively search parent scopes
		if (parent)
			return parent->lookup(name);

		return nullptr;
	}
};

#endif
