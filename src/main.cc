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
#include "ast_serializer.hh"
#include "builtin.hh"
#include "builtin/asm.hh"
#include "builtin/assign.hh"
#include "builtin/constant.hh"
#include "builtin/debug.hh"
#include "builtin/declare.hh"
#include "builtin/define.hh"
#include "builtin/doc.hh"
#include "builtin/elf.hh"
#include "builtin/equals.hh"
#include "builtin/eval.hh"
#include "builtin/fun.hh"
#include "builtin/if.hh"
#include "builtin/import.hh"
#include "builtin/macro.hh"
#include "builtin/operators.hh"
#include "builtin/quote.hh"
#include "builtin/struct.hh"
#include "builtin/u64.hh"
#include "builtin/str.hh"
#include "builtin/use.hh"
#include "builtin/value.hh"
#include "builtin/while.hh"
#include "compile.hh"
#include "source_file.hh"
#include "function.hh"
#include "globals.hh"
#include "macro.hh"
#include "namespace.hh"
#include "scope.hh"
#include "value.hh"

static void _print_u64(uint64_t *args)
{
	auto x = args[0];
	printf("%lu\n", x);
}

static void _print_str(uint64_t *args)
{
	auto s = *(std::string *) args[0];
	printf("%s\n", s.c_str());
}

static value_ptr builtin_macro_print(ast_node_ptr node)
{
	// TODO: save registers
	auto arg = compile(node);
	if (arg->type == builtin_type_u64) {
		auto print_fn = state->scope->make_value(state->context, VALUE_CONSTANT, builtin_type_u64);
		print_fn->constant.u64 = (uint64_t) &_print_u64;

		use_value(node, arg);
		state->function->emit_c_call(print_fn, { arg }, &builtin_value_void);
	} else if (arg->type == builtin_type_str) {
		auto print_fn = state->scope->make_value(state->context, VALUE_CONSTANT, builtin_type_u64);
		print_fn->constant.u64 = (uint64_t) &_print_str;

		// TODO: I think this only works by pure coincidence,
		// the problem is we're passing (part of) a std::string as char *
		use_value(node, arg);
		state->function->emit_c_call(print_fn, { arg }, &builtin_value_void);
	} else {
		error(node, "expected value of type u64");
	}

	return &builtin_value_void;
}

static auto builtin_value_namespace_lang = new value(nullptr, VALUE_CONSTANT,
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

static scope_ptr make_toplevel_scope()
{
	auto global_scope = std::make_shared<scope>();

	// Namespaces
	global_scope->define_builtin_namespace("lang", builtin_value_namespace_lang);

	// Types
	global_scope->define_builtin_type("str", builtin_type_str);
	global_scope->define_builtin_type("u64", builtin_type_u64);

	// Operators
	global_scope->define_builtin_macro("_eval", builtin_macro_eval);
	global_scope->define_builtin_macro("_declare", builtin_macro_declare);
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
	global_scope->define_builtin_macro("asm", builtin_macro_asm);
	global_scope->define_builtin_macro("constant", builtin_macro_constant);
	global_scope->define_builtin_macro("debug", builtin_macro_debug);
	global_scope->define_builtin_macro("doc", builtin_macro_doc);
	global_scope->define_builtin_macro("elf", builtin_macro_elf);
	global_scope->define_builtin_macro("if", builtin_macro_if);
	global_scope->define_builtin_macro("import", builtin_macro_import);
	global_scope->define_builtin_macro("while", builtin_macro_while);
	global_scope->define_builtin_macro("fun", builtin_macro_fun);
	global_scope->define_builtin_macro("quote", builtin_macro_quote);
	global_scope->define_builtin_macro("struct", builtin_macro_struct);
	global_scope->define_builtin_macro("use", builtin_macro_use);

	global_scope->define_builtin_macro("print", builtin_macro_print);

	return global_scope;
}

static std::shared_ptr<bytecode_function> compile_metaprogram(scope_ptr scope, source_file_ptr source, ast_node_ptr root)
{
	auto c = std::make_shared<context>(nullptr);
	auto f = std::make_shared<bytecode_function>(scope, c, true, std::vector<value_type_ptr>(), builtin_type_void);
	compile_state new_state(source, c, f, scope);
	state = &new_state;

	f->emit_prologue();
	compile(root);
	f->emit_epilogue();

	state = nullptr;
	return f;
}

static bool do_dump_ast = false;
static bool do_compile = true;
static bool do_run = true;

static bool compile_and_run(source_file_ptr source)
{
	auto scope = make_toplevel_scope();

	try {
		auto node = source->parse();
		assert(node != -1);

		std::shared_ptr<bytecode_function> f;

		if (do_compile)
			f = compile_metaprogram(scope, source, source->tree.get(node));

		if (do_dump_ast)
			printf("%s\n", serialize(source, source->tree.get(node)).c_str());

		if (global_disassemble) {
			printf("metaprogram:\n");
			disassemble_bytecode(f->constants.data(), f->bytes.data(), f->bytes.size(), f->comments);
			printf("\n");
		}

		if (do_compile && do_run)
			run(f);
	} catch (const parse_error &e) {
		print_message(source, e.pos, e.end, e.what());
		return true;
	} catch (const compile_error &e) {
		print_message(e.source, e.pos, e.end, e.what());
		return true;
	}

	return false;
}

static void repl()
{
	auto scope = make_toplevel_scope();

	// TODO: use std::cin or something that doesn't limit line length
	static char line[1024];
	while (true) {
		fprintf(stdout, ">>> ");
		fflush(stdout);

		if (!fgets(line, sizeof(line), stdin))
			break;

		auto source = std::make_shared<source_file>("<stdin>", line, strlen(line));
		try {
			auto node = source->parse();
			assert(node != -1);

			auto f = compile_metaprogram(scope, source, source->tree.get(node));
			run(f);
		} catch (const parse_error &e) {
			print_message(source, e.pos, e.end, e.what());
		} catch (const compile_error &e) {
			print_message(e.source, e.pos, e.end, e.what());
		}
	}

	fprintf(stdout, "\n");
	fflush(stdout);
}

int main(int argc, char *argv[])
{
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
			else if (!strcmp(argv[i], "-Xtrace-eval"))
				global_trace_eval = true;
			else if (!strcmp(argv[i], "-Xtrace-bytecode"))
				global_trace_bytecode = true;
			else
				error(EXIT_FAILURE, 0, "Unrecognised option: %s", argv[i]);
		} else {
			filenames.push_back(argv[i]);
		}
	}

	if (filenames.empty()) {
		repl();
	} else {
		for (const char *filename: filenames) {
			auto source = std::make_shared<mmap_source_file>(filename);
			if (compile_and_run(source))
				return EXIT_FAILURE;
		}
	}

	return EXIT_SUCCESS;
}
