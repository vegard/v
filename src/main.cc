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

extern "C" {
#include <sys/mman.h>
#include <sys/types.h>
}

#include <cstdio>

#include "ast.hh"
#include "builtin.hh"
#include "builtin_types.hh"
#include "compile.hh"
#include "document.hh"
#include "function.hh"
#include "scope.hh"
#include "value.hh"

static void _print(uint64_t x)
{
	printf("%lu\n", x);
}

static value_ptr builtin_macro_print(function &f, scope_ptr s, ast_node_ptr node)
{
	auto print_fn = std::make_shared<value>(VALUE_GLOBAL, builtin_type_u64);
	auto global = new void *;
	*global = (void *) &_print;
	print_fn->global.host_address = (void *) global;

	auto arg = compile(f, s, node);
	f.emit_move(arg, RDI);
	f.emit_call(print_fn);

	return std::make_shared<value>(VALUE_CONSTANT, builtin_type_void);
}

static function_ptr compile_metaprogram(ast_node_ptr root)
{
	auto global_scope = std::make_shared<scope>();

	// Types
	global_scope->define_builtin_type("u64", builtin_type_u64);

	// Operators
	global_scope->define_builtin_macro("_eval", builtin_macro_eval);
	global_scope->define_builtin_macro("_define", builtin_macro_define);
	global_scope->define_builtin_macro("_assign", builtin_macro_assign);
	global_scope->define_builtin_macro("_equals", builtin_macro_equals);
	global_scope->define_builtin_macro("_add", builtin_macro_add);

	// Keywords
	global_scope->define_builtin_macro("debug", builtin_macro_debug);
	global_scope->define_builtin_macro("if", builtin_macro_if);
	global_scope->define_builtin_macro("fun", builtin_macro_fun);

	global_scope->define_builtin_macro("print", builtin_macro_print);

	auto f = std::make_shared<function>(true);
	f->emit_prologue();
	compile(*f, global_scope, root);
	f->emit_epilogue();

	return f;
}

int main(int argc, char *argv[])
{
	bool do_dump_ast = false;
	bool do_compile = true;
	bool do_dump_binary = false;
	bool do_run = true;
	std::vector<const char *> filenames;

	for (int i = 1; i < argc; ++i) {
		if (argv[i][0] == '-') {
			if (!strcmp(argv[i], "--dump-ast"))
				do_dump_ast = true;
			else if (!strcmp(argv[i], "--no-compile"))
				do_compile = false;
			else if (!strcmp(argv[i], "--dump-binary"))
				do_dump_binary = true;
			else if (!strcmp(argv[i], "--no-run"))
				do_run = false;
			else
				error(EXIT_FAILURE, 0, "Unrecognised option: %s", argv[i]);
		} else {
			filenames.push_back(argv[i]);
		}
	}

	for (const char *filename: filenames) {
		file_document doc(filename);
		ast_node_ptr node;

		try {
			node = doc.parse();
		} catch (const parse_error &e) {
			print_message(doc, e.pos, e.end, e.what());
			return EXIT_FAILURE;
		}

		assert(node);

		if (do_dump_ast) {
			node->dump();
			printf("\n");
		}

		if (do_compile) {
			function_ptr f;

			try {
				f = compile_metaprogram(node);
			} catch (const compile_error &e) {
				print_message(doc, e.pos, e.end, e.what());
				return EXIT_FAILURE;
			}

			if (do_dump_binary) {
				std::string output_filename = std::string(filename) + ".bin";

				FILE *fp = fopen(output_filename.c_str(), "wb+");
				if (!fp)
					error(EXIT_FAILURE, errno, "%s: fopen()", output_filename.c_str());
				if (fwrite(&f->bytes[0], f->bytes.size(), 1, fp) != 1)
					error(EXIT_FAILURE, errno, "%s: fwrite()", output_filename.c_str());
				fclose(fp);
			}

			if (do_run)
				run(f);
		}
	}

	return EXIT_SUCCESS;
}
