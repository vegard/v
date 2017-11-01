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

#ifndef V_AST_HH
#define V_AST_HH

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

#include <gmpxx.h>

enum ast_node_type {
	AST_UNKNOWN,

	/* Atoms */
	AST_LITERAL_INTEGER,
	AST_LITERAL_STRING,
	AST_SYMBOL_NAME,

	/* Nullary/unary outfix operators */
	AST_BRACKETS,
	AST_SQUARE_BRACKETS,
	AST_CURLY_BRACKETS,

	/* Binary infix operators */
	AST_MEMBER,
	AST_PAIR,
	AST_JUXTAPOSE,
	AST_COMMA,
	AST_SEMICOLON,
};

bool is_binop(ast_node_type t)
{
	switch (t) {
	case AST_MEMBER:
	case AST_PAIR:
	case AST_JUXTAPOSE:
	case AST_COMMA:
	case AST_SEMICOLON:
		return true;
	default:
		break;
	}

	return false;
}

struct ast_node;
typedef std::shared_ptr<ast_node> ast_node_ptr;

struct ast_node {
	ast_node_type type;

	// Position where it was defined in the source document
	unsigned int pos;
	unsigned int end;

	union {
		mpz_class literal_integer;
		std::string literal_string;
		std::string symbol_name;

		ast_node_ptr unop;

		struct {
			ast_node_ptr lhs;
			ast_node_ptr rhs;
		} binop;
	};

	ast_node():
		type(AST_UNKNOWN)
	{
	}

	ast_node(ast_node_type type, unsigned int pos, unsigned int end):
		type(type),
		pos(pos),
		end(end)
	{
	}

	~ast_node() {
		switch (type) {
		case AST_UNKNOWN:
			break;

		/* Atoms */
		case AST_LITERAL_INTEGER:
			literal_integer.~mpz_class();
			break;
		case AST_LITERAL_STRING:
			typedef std::string string;
			literal_string.~string();
			break;
		case AST_SYMBOL_NAME:
			typedef std::string string;
			symbol_name.~string();
			break;

		/* Unary operators */
		case AST_BRACKETS:
		case AST_SQUARE_BRACKETS:
		case AST_CURLY_BRACKETS:
			unop.~ast_node_ptr();
			break;

		/* Binary operators */
		case AST_MEMBER:
		case AST_PAIR:
		case AST_JUXTAPOSE:
		case AST_COMMA:
		case AST_SEMICOLON:
			binop.lhs.~ast_node_ptr();
			binop.rhs.~ast_node_ptr();
			break;
		}
	}
};

struct serializer {
	unsigned int max_depth;
	unsigned int indentation;
	bool line_breaks;

	serializer():
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

	void unop(std::ostream &os, const ast_node_ptr &node, unsigned int depth, const char *name)
	{
		if (node->unop) {
			indent(os, depth);
			os << "(" << name;
			line_break(os);
			serialize(os, node->unop, depth + 1);
			line_break(os);
			indent(os, depth);
			os << ")";
		} else {
			indent(os, depth);
			os << "(parens)";
		}
	}

	void binop(std::ostream &os, const ast_node_ptr &node, unsigned int depth, const char *name)
	{
		assert(node->binop.lhs);
		assert(node->binop.rhs);

		indent(os, depth);
		os << "(" << name;
		line_break(os);
		serialize(os, node->binop.lhs, depth + 1);
		line_break(os);
		serialize(os, node->binop.rhs, depth + 1);
		line_break(os, "");
		indent(os, depth);
		os << ")";
	}

	void serialize(std::ostream &os, const ast_node_ptr &node, unsigned int depth = 0)
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
			os << "(literal_integer " << node->literal_integer.get_str() << ")";
			break;
		case AST_LITERAL_STRING:
			indent(os, depth);
			os << "(literal_string \"" << node->literal_string << "\")";
			break;
		case AST_SYMBOL_NAME:
			indent(os, depth);
			os << "(symbol_name " << node->symbol_name << ")";
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
		case AST_PAIR:
			binop(os, node, depth, "pair");
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
std::string abbreviate(const ast_node_ptr &node)
{
	serializer s;
	s.max_depth = 2;
	s.indentation = 0;
	s.line_breaks = false;

	std::ostringstream ss;
	s.serialize(ss, node);
	return ss.str();
}

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

#endif
