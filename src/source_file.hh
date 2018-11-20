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

#ifndef V_SOURCE_FILE_HH
#define V_SOURCE_FILE_HH

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

struct source_file;
typedef std::shared_ptr<source_file> source_file_ptr;

struct source_file {
	const char *name;

	const char *data;
	size_t data_size;

	std::unique_ptr<line_number_info> _line_numbers;

	ast_tree tree;

	source_file()
	{
	}

	source_file(const char *name, const char *data, size_t data_size):
		name(name),
		data(data),
		data_size(data_size)
	{
	}

	virtual ~source_file()
	{
	}

	line_number_info &line_numbers()
	{
		// Load line number data lazily
		if (!_line_numbers)
			_line_numbers = std::make_unique<line_number_info>(data, data_size);

		return *_line_numbers;
	}

	int parse()
	{
		unsigned int pos = 0;
		return parser(data, data_size, tree).parse_doc(pos);
	}
};

struct mmap_source_file: source_file {
	const char *filename;
	struct stat stbuf;
	void *mem;

	mmap_source_file(const char *filename):
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

	~mmap_source_file()
	{
		munmap(mem, stbuf.st_size);
	}
};

static std::string get_source_for(const source_file_ptr &source, const ast_node_ptr node)
{
	const line_number_info &line_numbers = source->line_numbers();
	auto pos = line_numbers.lookup(node->pos);
	auto end = line_numbers.lookup(node->end);

	if (pos.line == end.line)
		return std::string(source->data + node->pos, node->end - node->pos);

	if (pos.line_length - 1 >= pos.column + 1)
		return std::string(source->data + node->pos, pos.line_length - pos.column - 1) + "...";

	return std::string(source->data + node->pos, pos.line_length - pos.column);
}

static void print_message(const source_file_ptr &source, unsigned int pos_byte, unsigned int end_byte, std::string message)
{
	const line_number_info &line_numbers = source->line_numbers();
	auto pos = line_numbers.lookup(pos_byte);
	auto end = line_numbers.lookup(end_byte);

	printf("%s:%u:%u: %s\n", source->name, pos.line, pos.column, message.c_str());
	if (pos.line == end.line) {
		printf("%.*s", pos.line_length, source->data + pos.line_start);
		printf("%*s%s\n", pos.column, "", std::string(end.column - pos.column, '^').c_str());
	} else if (pos.line_length - 1 >= pos.column + 1) {
		// TODO: print following lines as well?
		printf("%.*s", pos.line_length, source->data + pos.line_start);
		printf("%*s%s\n", pos.column, "", std::string(pos.line_length - 1 - pos.column, '^').c_str());
	} else {
		printf("%.*s\n", pos.line_length, source->data + pos.line_start);
		printf("%*s%s\n", pos.column, "", std::string(pos.line_length - pos.column, '^').c_str());
	}
}

#endif
