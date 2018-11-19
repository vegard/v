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
	AST_JUXTAPOSE,
	AST_COMMA,
	AST_SEMICOLON,
};

bool is_binop(ast_node_type t)
{
	switch (t) {
	case AST_MEMBER:
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

	// Position where it was defined in the source file
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
		case AST_JUXTAPOSE:
		case AST_COMMA:
		case AST_SEMICOLON:
			binop.lhs.~ast_node_ptr();
			binop.rhs.~ast_node_ptr();
			break;
		}
	}
};

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
