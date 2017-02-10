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
#include <memory>

#include "ast.hh"
#include "line_number_info.hh"
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

struct document {
	const char *name;

	const char *data;
	size_t data_size;

	std::unique_ptr<line_number_info> _line_numbers;

	document()
	{
	}

	document(const char *name, const char *data, size_t data_size):
		name(name),
		data(data),
		data_size(data_size)
	{
	}

	virtual ~document()
	{
	}

	line_number_info &line_numbers()
	{
		// Load line number data lazily
		if (!_line_numbers)
			_line_numbers = std::make_unique<line_number_info>(data, data_size);

		return *_line_numbers;
	}

	ast_node_ptr parse()
	{
		unsigned int pos = 0;
		return parser(data, data_size).parse_doc(pos);
	}
};

struct file_document: document {
	const char *filename;
	struct stat stbuf;
	void *mem;

	file_document(const char *filename):
		filename(filename),
		mem(nullptr)
	{
		// TODO: throw exceptions instead of exiting

		int fd = open(filename, O_RDONLY);
		if (fd == -1)
			error(EXIT_FAILURE, errno, "%s: open()", filename);

		if (fstat(fd, &stbuf) == -1)
			error(EXIT_FAILURE, errno, "%s: fstat()", filename);

		mem = mmap(nullptr, stbuf.st_size, PROT_READ, MAP_SHARED, fd, 0);
		if (mem == MAP_FAILED)
			error(EXIT_FAILURE, errno, "%s: mmap()", filename);

		close(fd);

		// initialize parent
		name = filename;
		data = (const char *) mem;
		data_size = stbuf.st_size;
	}

	~file_document()
	{
		munmap(mem, stbuf.st_size);
	}
};

static void print_message(document &doc, unsigned int pos_byte, unsigned int end_byte, std::string message)
{
	line_number_info &line_numbers = doc.line_numbers();
	auto pos = line_numbers.lookup(pos_byte);
	auto end = line_numbers.lookup(end_byte);

	printf("%s:%u:%u: %s\n", doc.name, pos.line, pos.column, message.c_str());
	if (pos.line == end.line) {
		printf("%.*s", pos.line_length, doc.data + pos.line_start);
		printf("%*s%s\n", pos.column, "", std::string(end.column - pos.column, '^').c_str());
	} else {
		// TODO: print following lines as well?
		printf("%.*s", pos.line_length, doc.data + pos.line_start);
		assert(pos.line_length - 1 >= pos.column + 1);
		printf("%*s%s\n", pos.column, "", std::string(pos.line_length - 1 - pos.column, '^').c_str());
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
