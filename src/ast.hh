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
#include <vector>

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
typedef ast_node *ast_node_ptr;

struct ast_node {
	ast_node_type type;

	// Position where it was defined in the source file
	unsigned int pos;
	unsigned int end;

	union {
		// AST_SYMBOL_NAME for static strings only
		// if this is nullptr, then we use pos/end to get the name
		const char *symbol_name;

		int unop;

		struct {
			int lhs;
			int rhs;
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
		if (type == AST_SYMBOL_NAME)
			symbol_name = nullptr;
	}
};

struct ast_tree {
	std::vector<ast_node> nodes;

	ast_tree()
	{
	}

	template<typename... Args>
	int new_node(Args... args)
	{
		int result = nodes.size();
		nodes.push_back(ast_node(args...));
		return result;
	}

	ast_node *get(int node_index)
	{
		if (node_index == -1)
			return nullptr;

		return &nodes[node_index];
	}
};

template<ast_node_type type>
struct traverse {
	struct iterator {
		ast_tree &tree;
		ast_node_ptr node;

		iterator(ast_tree &tree, ast_node_ptr node):
			tree(tree),
			node(node)
		{
		}

		ast_node_ptr operator*()
		{
			if (node->type == type)
				return tree.get(node->binop.lhs);

			return node;
		}

		iterator &operator++()
		{
			if (node->type == type)
				node = tree.get(node->binop.rhs);
			else
				node = nullptr;

			return *this;
		}

		bool operator!=(const iterator &other) const
		{
			assert(&tree == &other.tree);
			return node != other.node;
		}
	};

	ast_tree &tree;
	ast_node_ptr node;

	traverse(ast_tree &tree, ast_node_ptr node):
		tree(tree),
		node(node)
	{
	}

	iterator begin()
	{
		return iterator(tree, node);
	}

	iterator end()
	{
		return iterator(tree, nullptr);
	}
};

#endif
