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
		type_value->global.host_address = (void *) &type;
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
