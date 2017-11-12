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

extern "C" {
#include <sys/mman.h>
#include <sys/types.h>
}

#include <cstdio>

#include "ast.hh"
#include "builtin.hh"
#include "builtin/assign.hh"
#include "builtin/scope.hh"
#include "builtin/compile.hh"
#include "builtin/compile_state.hh"
#include "builtin/debug.hh"
#include "builtin/define.hh"
#include "builtin/equals.hh"
#include "builtin/eval.hh"
#include "builtin/fun.hh"
#include "builtin/if.hh"
#include "builtin/macro.hh"
#include "builtin/operators.hh"
#include "builtin/quote.hh"
#include "builtin/struct.hh"
#include "builtin/u64.hh"
#include "builtin/value.hh"
#include "builtin/while.hh"
#include "compile.hh"
#include "source_file.hh"
#include "function.hh"
#include "globals.hh"
#include "macro.hh"
#include "scope.hh"
#include "value.hh"

static void _print(uint64_t x)
{
	printf("%lu\n", x);
}

static value_ptr builtin_macro_print(const compile_state &state, ast_node_ptr node)
{
	auto print_fn = std::make_shared<value>(state.context, VALUE_GLOBAL, builtin_type_u64);
	auto global = new void *;
	*global = (void *) &_print;
	print_fn->global.host_address = (void *) global;

	// TODO: save registers
	auto arg = compile(state, node);
	assert(arg->type == builtin_type_u64);

	state.use_value(node, arg);
	state.function->emit_move(arg, 0, RDI);
	state.function->emit_call(print_fn);

	return builtin_value_void;
}

struct namespace_member: member {
	value_ptr val;

	namespace_member(value_type_ptr type)
	{
		val = std::make_shared<value>(nullptr, VALUE_GLOBAL, builtin_type_type);
		val->global.host_address = (void *) new value_type_ptr(type);
	}

	value_ptr invoke(const compile_state &state, value_ptr v, ast_node_ptr node)
	{
		return val;
	}
};

static auto builtin_value_namespace_lang = std::make_shared<value>(nullptr, VALUE_CONSTANT,
	std::make_shared<value_type>(value_type {
		.alignment = 0,
		.size = 0,
		.constructor = nullptr,
		.argument_types = std::vector<value_type_ptr>(),
		.return_type = nullptr,
		.members = std::map<std::string, member_ptr>({
			{"macro", std::make_shared<namespace_member>(builtin_type_macro)},
			{"scope", std::make_shared<namespace_member>(builtin_type_scope)},
			{"value", std::make_shared<namespace_member>(builtin_type_value)},
		}),
	})
);

static function_ptr compile_metaprogram(ast_node_ptr root)
{
	auto global_scope = std::make_shared<scope>();

	// Namespaces
	global_scope->define_builtin_namespace("lang", builtin_value_namespace_lang);

	// Types
	global_scope->define_builtin_type("u64", builtin_type_u64);

	// Operators
	global_scope->define_builtin_macro("_eval", builtin_macro_eval);
	global_scope->define_builtin_macro("_define", builtin_macro_define);
	global_scope->define_builtin_macro("_assign", builtin_macro_assign);
	global_scope->define_builtin_macro("_equals", builtin_macro_equals);
	global_scope->define_builtin_macro("_notequals", builtin_macro_notequals);
	global_scope->define_builtin_macro("_add", builtin_macro_add);
	global_scope->define_builtin_macro("_subtract", builtin_macro_subtract);
	global_scope->define_builtin_macro("_less", builtin_macro_less);
	global_scope->define_builtin_macro("_less_equal", builtin_macro_less_equal);
	global_scope->define_builtin_macro("_greater", builtin_macro_greater);
	global_scope->define_builtin_macro("_greater_equal", builtin_macro_greater_equal);

	// Keywords
	global_scope->define_builtin_macro("debug", builtin_macro_debug);
	global_scope->define_builtin_macro("if", builtin_macro_if);
	global_scope->define_builtin_macro("while", builtin_macro_while);
	global_scope->define_builtin_macro("fun", builtin_macro_fun);
	global_scope->define_builtin_macro("quote", builtin_macro_quote);
	global_scope->define_builtin_macro("struct", builtin_macro_struct);

	global_scope->define_builtin_macro("print", builtin_macro_print);

	// Functions
	// XXX: move eval and compile to 'lang' namespace
	global_scope->define_builtin_macro("eval", builtin_function_eval);
	global_scope->define_builtin_macro("compile", builtin_function_compile);

	auto c = std::make_shared<context>(nullptr);
	auto f = std::make_shared<function>(true);
	f->emit_prologue();
	compile(compile_state(c, f, global_scope), root);
	f->emit_epilogue();

	return f;
}

int main(int argc, char *argv[])
{
	bool do_dump_ast = false;
	bool do_compile = true;
	bool do_run = true;
	std::vector<const char *> filenames;

	for (int i = 1; i < argc; ++i) {
		if (argv[i][0] == '-') {
			if (!strcmp(argv[i], "--dump-ast"))
				do_dump_ast = true;
			else if (!strcmp(argv[i], "--no-compile"))
				do_compile = false;
			else if (!strcmp(argv[i], "--no-run"))
				do_run = false;
			else if (!strcmp(argv[i], "--disassemble"))
				global_disassemble = true;
			else
				error(EXIT_FAILURE, 0, "Unrecognised option: %s", argv[i]);
		} else {
			filenames.push_back(argv[i]);
		}
	}

	for (const char *filename: filenames) {
		mmap_source_file source(filename);
		ast_node_ptr node;

		try {
			node = source.parse();
		} catch (const parse_error &e) {
			print_message(source, e.pos, e.end, e.what());
			return EXIT_FAILURE;
		}

		assert(node);

		if (do_dump_ast) {
			serializer().serialize(std::cout, node);
			std::cout << std::endl;
		}

		if (do_compile) {
			function_ptr f;

			try {
				f = compile_metaprogram(node);
			} catch (const compile_error &e) {
				print_message(source, e.pos, e.end, e.what());
				return EXIT_FAILURE;
			}

			if (do_run)
				run(f);
		}
	}

	return EXIT_SUCCESS;
}
