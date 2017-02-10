#include <cstdio>

#include "ast.hh"
#include "document.hh"
#include "parser.hh"

static void eval(const ast_node_ptr node)
{
	assert(node);

	switch (node->type) {
	case AST_JUXTAPOSE:
		{
			auto lhs = node->binop.lhs;
			auto rhs = node->binop.rhs;

			if (rhs->type == AST_BRACKETS) {
				/* Looks like a function call */

				/* Very special case for the "debug" symbol. */
				if (lhs->type == AST_SYMBOL_NAME && lhs->symbol_name == "debug") {
					for (ast_node_ptr arg: traverse<AST_COMMA>(rhs->unop)) {
						printf("%u: ", arg->pos);
						arg->dump();
						printf("\n");
					}
				}
			}
		}
		break;
	default:
		fprintf(stderr, "Unknown node type: ");
		node->dump(stderr);
		fprintf(stderr, "\n");
		assert(false);
	}
}

bool strstarts(const char *str, const char *prefix)
{
	return !strncmp(str, prefix, strlen(prefix));
}

int main(int argc, char *argv[])
{
	bool do_dump_ast = false;
	bool do_eval = true;
	std::vector<const char *> filenames;

	for (int i = 1; i < argc; ++i) {
		if (strstarts(argv[i], "--dump-ast"))
			do_dump_ast = true;
		else if (strstarts(argv[i], "--no-eval"))
			do_eval = false;
		else
			filenames.push_back(argv[i]);
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

		if (do_eval) {
			for (ast_node_ptr stmt: traverse<AST_SEMICOLON>(node))
				eval(stmt);
		}
	}

	return EXIT_SUCCESS;
}
