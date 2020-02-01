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
		source_file_ptr source;
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

	void define(function_ptr f, source_file_ptr source, ast_node_ptr node, const std::string name, value_ptr val)
	{
		entry e = {
			.f = f,
			.source = source,
			.node = node,
			.val = val,
		};

		if (f) {
			switch (val->storage_type) {
			case VALUE_GLOBAL:
				f->comment(format("define global var $", name));
				break;
			case VALUE_TARGET_GLOBAL:
				f->comment(format("define target global var $", name));
				break;
			case VALUE_LOCAL:
				f->comment(format("define local var $", name));
				break;
			case VALUE_LOCAL_POINTER:
				f->comment(format("define local pointer var $", name));
				break;
			case VALUE_CONSTANT:
				f->comment(format("define constant var $", name));
				break;
			}
		}

		contents[name] = e;
	}

	// Helper for defining builtin types
	// NOTE: builtin types are always global
	void define_builtin_type(const std::string name, value_type_ptr type)
	{
		auto type_value = std::make_shared<value>(nullptr, VALUE_GLOBAL, builtin_type_type);
		auto type_copy = new value_type_ptr(type);
		type_value->global.host_address = (void *) type_copy;
		define(nullptr, nullptr, nullptr, name, type_value);
	}

	// Helper for defining builtin macros
	// NOTE: builtin macros are always global
	void define_builtin_macro(const std::string name, macro_ptr m)
	{
		auto macro_value = std::make_shared<value>(nullptr, VALUE_GLOBAL, builtin_type_macro);
		auto macro_copy = new macro_ptr(m);
		macro_value->global.host_address = (void *) macro_copy;
		define(nullptr, nullptr, nullptr, name, macro_value);
	}

	void define_builtin_macro(const std::string name, value_ptr (*fn)(const compile_state &, ast_node_ptr))
	{
		return define_builtin_macro(name, std::make_shared<simple_macro>(fn));
	}

	void define_builtin_namespace(const std::string name, value_ptr val)
	{
		define(nullptr, nullptr, nullptr, name, val);
	}

	template<typename t>
	void define_builtin_constant(const std::string name, value_type_ptr type, const t &constant_value)
	{
		auto type_value = std::make_shared<value>(nullptr, VALUE_GLOBAL, type);
		auto copy = new t(constant_value);
		type_value->global.host_address = (void *) copy;
		define(nullptr, nullptr, nullptr, name, type_value);
	}

	bool lookup(const std::string name, entry &result)
	{
		auto it = contents.find(name);
		if (it != contents.end()) {
			result = it->second;
			return true;
		}

		// Recursively search parent scopes
		if (parent)
			return parent->lookup(name, result);

		return false;
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

static bool can_use_value(context_ptr c, value_ptr val)
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
			return false;
	}

	return true;
}

#endif
