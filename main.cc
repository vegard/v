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

int main(int argc, char *argv[])
{
	for (int i = 1; i < argc; ++i) {
		file_document doc(argv[i]);
		ast_node_ptr node;

		try {
			node = doc.parse();
		} catch (const parse_error &e) {
			print_message(doc, e.pos, e.end, e.what());
			return EXIT_FAILURE;
		}

		if (node) {
			node->dump();
			printf("\n");
		}

		if (false) {
			for (ast_node_ptr stmt: traverse<AST_SEMICOLON>(node))
				eval(stmt);
		}
	}

	return EXIT_SUCCESS;
}
