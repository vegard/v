extern "C" {
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <error.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
}

#include "line_number_info.hh"
#include "parser.hh"

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

		try {
			unsigned int pos = 0;
			auto node = p.parse_doc(pos);
			if (node) {
				node->dump();
				printf("\n");
			}
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

		munmap(mem, stbuf.st_size);
	}

	return EXIT_SUCCESS;
}
