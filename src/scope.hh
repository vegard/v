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

#ifndef V_SCOPE_HH
#define V_SCOPE_HH

#include <map>
#include <memory>
#include <string>

#include "ast.hh"
#include "compile_error.hh"
#include "macro.hh"
#include "value.hh"

struct scope;
typedef std::shared_ptr<scope> scope_ptr;

// Evaluation context (used to detect when trying to evaluate a symbol which
// was defined in the same context)
struct context;
typedef std::shared_ptr<context> context_ptr;

struct context {
	context_ptr parent;

	// No default argument for the parent, since that makes it easier
	// to introduce bugs if you forget it
	context(context_ptr parent):
		parent(parent)
	{
	}

	~context()
	{
	}
};

// Map symbol names to values.
// TODO: keep track of _where_ a symbol was defined?
struct scope {
	struct entry {
		function_ptr f;
		ast_node_ptr node;
		value_ptr val;
	};

	scope_ptr parent;
	std::map<std::string, entry> contents;

	scope(scope_ptr parent = nullptr):
		parent(parent)
	{
	}

	~scope()
	{
	}

	void define(function_ptr f, ast_node_ptr node, const std::string name, value_ptr val)
	{
		entry e = {
			.f = f,
			.node = node,
			.val = val,
		};

		contents[name] = e;
	}

	// Helper for defining builtin types
	// NOTE: builtin types are always global
	void define_builtin_type(const std::string name, value_type_ptr type)
	{
		auto type_value = std::make_shared<value>(nullptr, VALUE_GLOBAL, builtin_type_type);
		auto type_copy = new value_type_ptr(type);
		type_value->global.host_address = (void *) type_copy;
		define(nullptr, nullptr, name, type_value);
	}

	// Helper for defining builtin macros
	// NOTE: builtin macros are always global
	void define_builtin_macro(const std::string name, macro_ptr m)
	{
		auto macro_value = std::make_shared<value>(nullptr, VALUE_GLOBAL, builtin_type_macro);
		auto macro_copy = new macro_ptr(m);
		macro_value->global.host_address = (void *) macro_copy;
		define(nullptr, nullptr, name, macro_value);
	}

	void define_builtin_macro(const std::string name, value_ptr (*fn)(context_ptr, function_ptr, scope_ptr, ast_node_ptr))
	{
		return define_builtin_macro(name, std::make_shared<simple_macro>(fn));
	}

	value_ptr lookup(function_ptr f, ast_node_ptr node, const std::string name)
	{
		auto it = contents.find(name);
		if (it != contents.end()) {
			const auto &entry = it->second;

			// We can always access globals
			auto val = entry.val;
			if (val->storage_type == VALUE_GLOBAL || val->storage_type == VALUE_CONSTANT)
				return val;

			if (f != entry.f)
				throw compile_error(node, "cannot access local variable %s of different function", name.c_str());

			return val;
		}

		// Recursively search parent scopes
		if (parent)
			return parent->lookup(f, node, name);

		return nullptr;
	}
};

static bool is_parent_of(scope_ptr parent, scope_ptr child)
{
	while (child) {
		if (child == parent)
			return true;

		child = child->parent;
	}

	return false;
}

static void use_value(context_ptr c, ast_node_ptr node, value_ptr val)
{
#if 0
	printf("use: ");
	node->dump(stdout);
	printf("\n");

	printf("current context: \n");
	for (auto tmp = c; tmp; tmp = tmp->parent)
		printf(" - %p\n", tmp.get());

	printf("def context: \n");
	for (auto tmp = val->context; tmp; tmp = tmp->parent)
		printf(" - %p\n", tmp.get());
	printf("\n");
#endif

	assert(c);
	while (true) {
		c = c->parent;
		if (!c)
			break;

		if (c == val->context)
			// TODO: val->node
			throw compile_error(node, "cannot access value at compile time");
	}
}

#endif
