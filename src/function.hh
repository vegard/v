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

#ifndef V_FUNCTION_HH
#define V_FUNCTION_HH

extern "C" {
#include <elf.h>
}

#include <map>
#include <memory>

#include "format.hh"
#include "value.hh"

struct label {
	virtual ~label()
	{
	}
};

typedef std::shared_ptr<label> label_ptr;

struct function;
typedef std::shared_ptr<function> function_ptr;

struct function
{
	enum compare_op {
		CMP_EQ,
		CMP_NEQ,
		CMP_LESS,
		CMP_LESS_EQUAL,
		CMP_GREATER,
		CMP_GREATER_EQUAL,
	};

	object_ptr this_object;
	std::vector<function_comment> comments;
	unsigned int indentation;

	std::vector<value_type_ptr> args_types;
	value_type_ptr return_type;

	std::vector<value_ptr> args_values;
	value_ptr return_value;

	function(std::vector<value_type_ptr> args_types, value_type_ptr return_type):
		this_object(std::make_shared<object>()),
		indentation(0),
		args_types(args_types),
		return_type(return_type)
	{
	}

	virtual ~function()
	{
	}

	virtual value_ptr alloc_local_value(scope_ptr scope, context_ptr c, value_type_ptr type) = 0;

	void not_implemented(const char *file, unsigned int line, const char *func)
	{
		fprintf(stderr, "%s:%u: not implemented: %s\n",
			file, line, func);
	}

	#define NOT_IMPLEMENTED not_implemented(__FILE__, __LINE__, __func__)

	void comment(std::string s)
	{
		comments.push_back(function_comment(this_object->bytes.size(), indentation, s));
	}


	void enter()
	{
		++indentation;
		comments.push_back(function_comment(this_object->bytes.size(), indentation, ""));
	}

	void leave()
	{
		--indentation;
		comments.push_back(function_comment(this_object->bytes.size(), indentation, ""));
	}

	virtual void emit_prologue()
	{
	}

	virtual void emit_epilogue()
	{
	}

	virtual void emit_move(value_ptr source, value_ptr dest) = 0;
	virtual void emit_compare(compare_op op, value_ptr source1, value_ptr source2, value_ptr dest) = 0;

	virtual label_ptr new_label() = 0;
	virtual void emit_label(label_ptr) = 0;
	virtual void link_label(label_ptr) = 0;

	virtual void emit_jump_if_zero(value_ptr value, label_ptr target) = 0;
	virtual void emit_jump(label_ptr target) = 0;

	virtual void emit_call(value_ptr target, std::vector<value_ptr> args_values, value_ptr return_value) = 0;
	virtual void emit_c_call(value_ptr target, std::vector<value_ptr> args_values, value_ptr return_value)
	{
		assert(false);
	}

	virtual void emit_add(value_ptr source1, value_ptr source2, value_ptr dest) = 0;
	virtual void emit_sub(value_ptr source1, value_ptr source2, value_ptr dest) = 0;

	virtual void run()
	{
		abort();
	}
};

struct function_block {
	function *f;

	function_block(function *f, std::string name, std::string args = ""):
		f(f)
	{
		f->comment(format("$($) {", name, args));
		f->enter();
	}

	function_block(const function_ptr &f, std::string name, std::string args = ""):
		f(f.get())
	{
		f->comment(format("$($) {", name, args));
		f->enter();
	}

	~function_block()
	{
		f->leave();
		f->comment("}");
	}
};

// This takes a weak reference on 'f', so you need to make sure it cannot be
// destroyed before the end of the scope if you use this.
#define function_enter(f, args...) \
	function_block __function_enter(f, __FUNCTION__, ##args)

#endif
