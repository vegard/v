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

#ifndef V_AST_SERIALIZER_HH
#define V_AST_SERIALIZER_HH

#include <cassert>
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <iterator>
#include <memory>
#include <stdexcept>
#include <sstream>
#include <string>
#include <vector>

#include "ast.hh"
#include "source_file.hh"

struct ast_serializer {
	source_file_ptr source;
	unsigned int max_depth;
	unsigned int indentation;
	bool line_breaks;

	ast_serializer(source_file_ptr source):
		source(source),
		max_depth(0),
		indentation(4),
		line_breaks(true)
	{
	}

	void line_break(std::ostream &os, const char *space = " ")
	{
		if (line_breaks)
			os << "\n";
		else
			os << space;
	}

	void indent(std::ostream &os, unsigned int depth)
	{
		std::fill_n(std::ostream_iterator<char>(os), depth * indentation, ' ');
	}

	void unop(std::ostream &os, const ast_node_ptr node, unsigned int depth, const char *name)
	{
		auto child = source->tree.get(node->unop);

		if (child) {
			indent(os, depth);
			os << "(" << name;
			line_break(os);
			serialize(os, child, depth + 1);
			line_break(os);
			indent(os, depth);
			os << ")";
		} else {
			indent(os, depth);
			os << "(" << name << ")";
		}
	}

	void binop(std::ostream &os, const ast_node_ptr node, unsigned int depth, const char *name)
	{
		indent(os, depth);
		os << "(" << name;
		line_break(os);
		serialize(os, source->tree.get(node->binop.lhs), depth + 1);
		line_break(os);
		serialize(os, source->tree.get(node->binop.rhs), depth + 1);
		line_break(os, "");
		indent(os, depth);
		os << ")";
	}

	void serialize(std::ostream &os, const ast_node_ptr node, unsigned int depth = 0)
	{
		if (max_depth && depth >= max_depth) {
			os << "...";
			return;
		}

		switch (node->type) {
		case AST_UNKNOWN:
			os << "(unknown)";
			break;

		case AST_LITERAL_INTEGER:
			indent(os, depth);
			os << "(literal_integer " << std::string(&source->data[node->pos], node->end - node->pos) << ")";
			break;
		case AST_LITERAL_STRING:
			indent(os, depth);
			os << "(literal_string " << std::string(&source->data[node->pos], node->end - node->pos) << ")";
			break;
		case AST_SYMBOL_NAME:
			indent(os, depth);
			if (node->symbol_name)
				os << "(symbol_name " << node->symbol_name << ")";
			else
				os << "(symbol_name " << std::string(&source->data[node->pos], node->end - node->pos) << ")";
			break;

		case AST_BRACKETS:
			unop(os, node, depth, "brackets");
			break;
		case AST_SQUARE_BRACKETS:
			unop(os, node, depth, "square-brackets");
			break;
		case AST_CURLY_BRACKETS:
			unop(os, node, depth, "curly-brackets");
			break;

		case AST_MEMBER:
			binop(os, node, depth, "member");
			break;
		case AST_JUXTAPOSE:
			binop(os, node, depth, "juxtapose");
			break;
		case AST_COMMA:
			binop(os, node, depth, "comma");
			break;
		case AST_SEMICOLON:
			binop(os, node, depth, "semicolon");
			break;
		}
	}
};

// Create a "one-line" abbreviation of the serialized AST node, useful
// for debugging where you just want to show a part of the tree (e.g. the
// node and its children, but not grandchildren).
std::string abbreviate(const source_file_ptr source, const ast_node_ptr node)
{
	ast_serializer s(source);
	s.max_depth = 2;
	s.indentation = 0;
	s.line_breaks = false;

	std::ostringstream ss;
	s.serialize(ss, node);
	return ss.str();
}

#endif
