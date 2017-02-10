extern "C" {
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <error.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
}

#include <functional>

#include "line_number_info.hh"
#include "parser.hh"

template<ast_node_type type>
struct traverse {
	struct iterator {
		ast_node_ptr node;

		iterator(ast_node_ptr node):
			node(node)
		{
		}

		ast_node_ptr operator*()
		{
			if (node->type == type)
				return node->binop.lhs;

			return node;
		}

		iterator &operator++()
		{
			if (node->type == type)
				node = node->binop.rhs;
			else
				node = nullptr;

			return *this;
		}

		bool operator!=(const iterator &other) const
		{
			return node != other.node;
		}
	};

	ast_node_ptr node;

	traverse(ast_node_ptr node):
		node(node)
	{
	}

	iterator begin()
	{
		return iterator(node);
	}

	iterator end()
	{
		return iterator(nullptr);
	}
};

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
		const char *filename = argv[i];

		int fd = open(filename, O_RDONLY);
		if (fd == -1)
			error(EXIT_FAILURE, errno, "%s: open()", filename);

		struct stat stbuf;
		if (fstat(fd, &stbuf) == -1)
			error(EXIT_FAILURE, errno, "%s: fstat()", filename);

		void *mem = mmap(nullptr, stbuf.st_size, PROT_READ, MAP_SHARED, fd, 0);
		if (mem == MAP_FAILED)
			error(EXIT_FAILURE, errno, "%s: mmap()", filename);

		close(fd);

		const char *doc = (const char *) mem;
		size_t doc_size = stbuf.st_size;

		parser p(doc, doc_size);
		ast_node_ptr node;

		try {
			unsigned int pos = 0;
			node = p.parse_doc(pos);
		} catch (const parse_error &e) {
			line_number_info line_numbers(doc, doc_size);
			auto pos = line_numbers.lookup(e.pos);
			auto end = line_numbers.lookup(e.end);

			printf("%s:%u:%u: %s\n", filename, pos.line, pos.column, e.what());
			if (pos.line == end.line) {
				printf("%.*s", pos.line_length, doc + pos.line_start);
				printf("%*s%s\n", pos.column, "", std::string(end.column - pos.column, '^').c_str());
			} else {
				// TODO: print following lines as well?
				printf("%.*s", pos.line_length, doc + pos.line_start);
				assert(pos.line_length - 1 >= pos.column + 1);
				printf("%*s%s\n", pos.column, "", std::string(pos.line_length - 1 - pos.column, '^').c_str());
			}

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

		munmap(mem, stbuf.st_size);
	}

	return EXIT_SUCCESS;
}
