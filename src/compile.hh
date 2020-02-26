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

#ifndef V_COMPILE_HH
#define V_COMPILE_HH

#include <gmpxx.h>

#include "ast.hh"
#include "ast_serializer.hh"
#include "bytecode.hh"
#include "compile_error.hh"
#include "format.hh"
#include "function.hh"
#include "globals.hh"
#include "scope.hh"
#include "source_file.hh"
#include "value.hh"
#include "x86_64.hh"

typedef std::shared_ptr<std::vector<object_ptr>> objects_ptr;

// This is a badly named; for the future I'd like to rename 'context' to
// something else and then rename this to 'compile_context'
struct compile_state {
	// XXX: Implicit assumption: when objects == nullptr, we're compiling for the host
	objects_ptr objects;
	source_file_ptr source;
	context_ptr context;
	function_ptr function;
	scope_ptr scope;

	compile_state(source_file_ptr &source, context_ptr &context, function_ptr function, scope_ptr &scope):
		source(source),
		context(context),
		function(function),
		scope(scope)
	{
	}

	unsigned int new_object(object_ptr object) const
	{
		assert(objects);
		unsigned int object_id = objects->size();
		objects->push_back(object);
		return object_id;
	}

#if 0
	compile_state set_objects(objects_ptr objects) const
	{
		auto ret = *this;
		ret.objects = objects;
		return ret;
	}

	compile_state set_source(source_file_ptr &new_source, scope_ptr &new_scope) const
	{
		auto ret = *this;
		ret.source = new_source;
		ret.scope = new_scope;
		return ret;
	}

	compile_state set_context(context_ptr &new_context) const
	{
		auto ret = *this;
		ret.context = new_context;
		return ret;
	}

	compile_state set_scope(scope_ptr &new_scope) const
	{
		auto ret = *this;
		ret.scope = new_scope;
		return ret;
	}

	compile_state set_function(context_ptr &new_context, function_ptr new_function) const
	{
		auto ret = *this;
		ret.context = new_context;
		ret.function = new_function;
		return ret;
	}

	compile_state set_function(function_ptr new_function, scope_ptr &new_scope) const
	{
		auto ret = *this;
		ret.function = new_function;
		ret.scope = new_scope;
		return ret;
	}
#endif

	template<typename... Args>
	void __attribute__((noreturn)) error(const ast_node_ptr node, const char *fmt, Args... args) const
	{
		throw compile_error(source, node, fmt, args...);
	}

	template<typename... Args>
	void expect(const ast_node_ptr node, bool cond, const char *fmt, Args... args) const
	{
		if (!cond)
			error(node, fmt, args...);
	}

	void expect_type(const ast_node_ptr node, ast_node_type type) const
	{
		// TODO: stringify the expected and actual types
		expect(node, node->type == type, "got AST node type $, expected $", node->type, type);
	}

	void expect_type(const ast_node_ptr node, value_ptr value, value_type_ptr type) const
	{
		// TODO: can we even stringify these types?
		expect(node, value->type == type, "unexpected type");
	}

	value_ptr lookup(const ast_node_ptr node, const std::string name) const
	{
		scope::entry e;
		if (!scope->lookup(name, e))
			return nullptr;

		// We can always access globals
		auto val = e.val;
		if (val->storage_type == VALUE_GLOBAL || val->storage_type == VALUE_TARGET_GLOBAL || val->storage_type == VALUE_CONSTANT)
			return val;

		if (e.f != function)
			error(node, "cannot access local variable of different function");

		return val;
	}

	void use_value(const ast_node_ptr &node, value_ptr val) const
	{
		if (!can_use_value(context, val))
			error(node, "cannot access value at compile time");
	}

	ast_node_ptr get_node(int index) const
	{
		return source->tree.get(index);
	}

	mpz_class get_literal_integer(ast_node_ptr node) const
	{
		assert(node->type == AST_LITERAL_INTEGER);

		// TODO: parse it correctly (sign, base, etc.)...
		mpz_class literal_integer;
		literal_integer.set_str(std::string(&source->data[node->pos], node->end - node->pos), 10);
		return literal_integer;
	}

	std::string get_literal_string(ast_node_ptr node) const
	{
		assert(node->type == AST_LITERAL_STRING);

		return source->tree.strings[node->string_index];
	}

	std::string get_symbol_name(ast_node_ptr node) const
	{
		assert(node->type == AST_SYMBOL_NAME);

		if (node->symbol_name)
			return node->symbol_name;

		return std::string(&source->data[node->pos], node->end - node->pos);
	}
};

__thread compile_state *state;

struct use_function {
	function_ptr old_function;
	scope_ptr old_scope;

	explicit use_function(function_ptr function):
		old_function(state->function),
		old_scope(state->scope)
	{
		state->function = function;
	}

	use_function(function_ptr function, scope_ptr scope):
		old_function(state->function),
		old_scope(state->scope)
	{
		state->function = function;
		state->scope = scope;
	}

	~use_function()
	{
		state->function = old_function;
		state->scope = old_scope;
	}
};

struct use_scope {
	scope_ptr old_scope;

	explicit use_scope(scope_ptr scope):
		old_scope(state->scope)
	{
		state->scope = scope;
	}

	~use_scope()
	{
		state->scope = old_scope;
	}
};

struct use_objects {
	objects_ptr old_objects;
	scope_ptr old_scope;

	use_objects(objects_ptr objects, scope_ptr scope):
		old_objects(state->objects),
		old_scope(state->scope)
	{
		state->objects = objects;
		state->scope = scope;
	}

	~use_objects()
	{
		state->objects = old_objects;
		state->scope = old_scope;
	}
};

struct use_source {
	source_file_ptr old_source;
	scope_ptr old_scope;

	use_source(source_file_ptr source, scope_ptr scope):
		old_source(state->source),
		old_scope(state->scope)
	{
		state->source = source;
		state->scope = scope;
	}

	~use_source()
	{
		state->source = old_source;
		state->scope = old_scope;
	}
};

struct use_context {
	context_ptr old_context;

	explicit use_context(context_ptr context):
		old_context(state->context)
	{
		state->context = context;
	}

	~use_context()
	{
		state->context = old_context;
	}
};

static value_ptr compile(ast_node_ptr node);

static void run(std::shared_ptr<bytecode_function> f)
{
	run_bytecode(f->constants.data(), f->bytes.data(), nullptr, 0);
}

static value_ptr eval(ast_node_ptr node)
{
	if (global_trace_eval)
		printf("\e[32m[trace-eval] %s\e[0m\n", serialize(state->source, node).c_str());

	auto new_c = std::make_shared<context>(state->context);
	use_context _asdf(new_c);

	auto new_f = std::make_shared<bytecode_function>(state->scope, new_c, true, std::vector<value_type_ptr>(), builtin_type_void);

	value_ptr ret;
	{
		use_function _asdf(new_f);

		new_f->emit_prologue();

		auto v = compile(node);

		if (v->storage_type == VALUE_LOCAL || v->storage_type == VALUE_LOCAL_POINTER) {
			// Make sure we copy the value out to a new global in case the
			// returned value is a local (which cannot be accessed outside
			// "new_f" itself).
			ret = state->scope->make_value(new_c, VALUE_GLOBAL, v->type);
			auto global = new uint8_t[v->type->size];
			ret->global.host_address = (void *) global;
			new_f->emit_move(v, ret);
		} else {
			// We can return it directly
			ret = v;
		}

		new_f->emit_epilogue();
	}

	if (global_disassemble) {
		printf("eval:\n");
		disassemble_bytecode(new_f->constants.data(), new_f->bytes.data(), new_f->bytes.size(), new_f->comments);
		printf("\n");
	}

	run(new_f);

	return ret;
}

static value_ptr compile_brackets(ast_node_ptr node)
{
	function_enter(state->function, get_source_for(state->source, node));

	return compile(state->source->tree.get(node->unop));
}

static value_ptr compile_curly_brackets(ast_node_ptr node)
{
	function_enter(state->function, get_source_for(state->source, node));

	auto ret = state->scope->make_value();

	// Curly brackets create a new scope parented to the old one
	auto new_scope = std::make_shared<scope>(state->scope);
	auto v = (use_scope(new_scope), compile(state->source->tree.get(node->unop)));
	*ret = *v;
	return ret;
}

static value_ptr compile_member(ast_node_ptr node)
{
	function_enter(state->function, get_source_for(state->source, node));

	assert(node->type == AST_MEMBER);

	auto lhs = compile(state->source->tree.get(node->binop.lhs));
	auto lhs_type = lhs->type;

	auto rhs_node = state->source->tree.get(node->binop.rhs);
	if (rhs_node->type != AST_SYMBOL_NAME)
		// TODO: say which AST node type we got instead of a symbol name
		state->error(node, "member name must be a symbol");

	auto symbol_name = state->get_symbol_name(rhs_node);
	auto it = lhs_type->members.find(symbol_name);
	if (it == lhs_type->members.end())
		state->error(node, "unknown member: $", symbol_name);

	return it->second->invoke(lhs, rhs_node);
}

static value_ptr _compile_juxtapose(ast_node_ptr lhs_node, value_ptr lhs, ast_node_ptr rhs_node)
{
	auto lhs_type = lhs->type;

	if (lhs_type == builtin_type_macro) {
		// macros are evaluated directly
		auto new_c = std::make_shared<context>(state->context);
		(use_context(new_c), state->use_value(lhs_node, lhs));
		assert(lhs->storage_type == VALUE_GLOBAL);

		auto m = *(macro_ptr *) lhs->global.host_address;
		return m->invoke(rhs_node);
	} else if (lhs_type == builtin_type_type) {
		auto new_c = std::make_shared<context>(state->context);
		(use_context(new_c), state->use_value(lhs_node, lhs));
		assert(lhs->storage_type == VALUE_GLOBAL);

		// call type's constructor
		auto type = *(value_type_ptr *) lhs->global.host_address;
		if (!type->constructor)
			state->error(lhs_node, "type doesn't have a constructor");

		// TODO: functions as constructors
		return type->constructor(type, rhs_node);
	}

	auto it = lhs_type->members.find("_call");
	if (it != lhs_type->members.end())
		return it->second->invoke(lhs, rhs_node);

	state->error(lhs_node, "type is not callable");
}

static value_ptr compile_juxtapose(ast_node_ptr node)
{
	function_enter(state->function, get_source_for(state->source, node));

	assert(node->type == AST_JUXTAPOSE);

	auto lhs_node = state->source->tree.get(node->binop.lhs);
	auto rhs_node = state->source->tree.get(node->binop.rhs);
	auto lhs = compile(lhs_node);
	return _compile_juxtapose(lhs_node, lhs, rhs_node);
}

static value_ptr compile_symbol_name(ast_node_ptr node)
{
	auto symbol_name = state->get_symbol_name(node);
	auto ret = state->lookup(node, symbol_name);
	if (!ret)
		state->error(node, "could not resolve symbol: $", symbol_name);

	return ret;
}

static value_ptr compile_semicolon(ast_node_ptr node)
{
	// TODO: should we return the result of compiling LHS or void?
	compile(state->source->tree.get(node->binop.lhs));
	return compile(state->source->tree.get(node->binop.rhs));
}

static value_ptr builtin_type_u64_constructor(value_type_ptr, ast_node_ptr);
static value_ptr builtin_type_str_constructor(value_type_ptr, ast_node_ptr);

static value_ptr compile(ast_node_ptr node)
{
	if (!node)
		return &builtin_value_void;

	switch (node->type) {
	case AST_LITERAL_INTEGER:
		// TODO: evaluate as int rather than u64
		return builtin_type_u64_constructor(nullptr, node);
	case AST_LITERAL_STRING:
		return builtin_type_str_constructor(nullptr, node);
	case AST_SYMBOL_NAME:
		return compile_symbol_name(node);

	case AST_BRACKETS:
		return compile_brackets(node);
	case AST_CURLY_BRACKETS:
		return compile_curly_brackets(node);

	case AST_MEMBER:
		return compile_member(node);
	case AST_JUXTAPOSE:
		return compile_juxtapose(node);
	case AST_SEMICOLON:
		return compile_semicolon(node);

	default:
		state->error(node, "unrecognised expression");
	}

	assert(false);
	return nullptr;
}

#endif
