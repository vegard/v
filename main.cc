extern "C" {
#include <sys/mman.h>
#include <sys/types.h>
}

#include <cstdio>

#include "ast.hh"
#include "builtin.hh"
#include "compile.hh"
#include "document.hh"
#include "function.hh"
#include "scope.hh"
#include "value.hh"

static function_ptr compile_metaprogram(ast_node_ptr root)
{
	auto global_scope = std::make_shared<scope>();

	// Types
	global_scope->define_builtin_type("uint64", &builtin_type_uint64);

	// Operators
	global_scope->define_builtin_macro("_define", builtin_macro_define);
	global_scope->define_builtin_macro("_assign", builtin_macro_assign);
	global_scope->define_builtin_macro("_equals", builtin_macro_equals);

	// Keywords
	global_scope->define_builtin_macro("debug", builtin_macro_debug);
	global_scope->define_builtin_macro("if", builtin_macro_if);
	global_scope->define_builtin_macro("fun", builtin_macro_fun);

	auto f = std::make_shared<function>();
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

			if (do_run) {
				size_t length = (f->bytes.size() + 4095) & ~4095;
				void *mem = mmap(NULL, length,
					PROT_READ | PROT_WRITE | PROT_EXEC,
					MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
				if (mem == MAP_FAILED)
					error(EXIT_FAILURE, errno, "%s: mmap()", filename);

				memcpy(mem, &f->bytes[0], f->bytes.size());

				// Flush instruction cache so we know we'll
				// execute what we compiled and not some
				// garbage that happened to be in the cache.
				__builtin___clear_cache((char *) mem, (char *) mem + length);

				auto fn = (void (*)()) mem;
				fn();

				munmap(mem, length);
			}
		}
	}

	return EXIT_SUCCESS;
}
