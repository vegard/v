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

#ifndef V_PARSER_HH
#define V_PARSER_HH

#include <cassert>
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include "ast.hh"

/*
 * Nullary/unary (outfix) operators:
 *   (x)
 *   [x]
 *   <x>
 *   {x}
 *   (C++-style multiline comments)
 *
 * Unary prefix operators:
 *   @x
 *
 * Binary operators (highest precedence first):
 *   x.y
 *   x y
 *   x:y
 *   x..y
 *   x * y    x / y
 *   x + y    x - y
 *   x = y
 *   x := y
 *   x, y
 *   x; y
 */

enum precedence {
	PREC_SEMICOLON,
	PREC_AT,
	PREC_DEFINE,
	PREC_ASSIGN,
	PREC_COMMA,
	PREC_EQUALITY,
	PREC_ADD_SUBTRACT,
	PREC_MULTIPLY_DIVIDE,
	PREC_PAIR,
	PREC_JUXTAPOSE,
	PREC_MEMBER,
	PREC_OUTFIX,
	PREC_LITERAL,
};

enum associativity {
	ASSOC_RIGHT,
	ASSOC_LEFT,
};

struct parse_error: std::runtime_error {
	unsigned int pos;
	unsigned int end;

	parse_error(const char *message, unsigned int pos, unsigned int end):
		std::runtime_error(message),
		pos(pos),
		end(end)
	{
		assert(end >= pos);
	}
};

struct syntax_error: parse_error {
	syntax_error(const char *message, unsigned int pos, unsigned int end):
		parse_error(message, pos, end)
	{
	}
};

struct parser {
	const char *buf;
	unsigned int len;
	ast_tree &tree;

	parser(const char *buf, size_t len, ast_tree &tree):
		buf(buf),
		len(len),
		tree(tree)
	{
	}

	void skip_whitespace(unsigned int &pos);
	void skip_comments(unsigned int &pos);
	void skip_whitespace_and_comments(unsigned int &pos);

	int parse_literal_integer(unsigned int &pos);
	int parse_literal_string(unsigned int &pos);
	int parse_symbol_name(unsigned int &pos);
	int parse_atom(unsigned int &pos);

	template<ast_node_type type, unsigned int left_size, unsigned int right_size>
	int parse_outfix(const char (&left)[left_size], const char (&right)[right_size], unsigned int &pos);

	template<precedence prec, unsigned int op_size>
	int parse_unop_prefix_as_call(const char (&op)[op_size], const char *symbol_name, unsigned int &pos);

	template<ast_node_type type, precedence prec, associativity assoc, bool allow_trailing, unsigned int op_size>
	int parse_binop(const char (&op)[op_size], int lhs, unsigned int &pos, unsigned int min_precedence);

	template<precedence prec, associativity assoc, bool allow_trailing, unsigned int op_size>
	int parse_binop_as_call(const char (&op)[op_size], const char *symbol_name,
		int lhs, unsigned int &pos, unsigned int min_precedence);

	int parse_expr(unsigned int &pos, unsigned int min_precedence = 0);

	int parse_doc(unsigned int &pos);
};

void parser::skip_whitespace(unsigned int &pos)
{
	unsigned int i = pos;

	while (i < len && isspace(buf[i]))
		++i;

	pos = i;
}

void parser::skip_comments(unsigned int &pos)
{
	unsigned int i = pos;

	if (i < len && buf[i] == '#') {
		++i;

		while (i < len && buf[i] != '\n')
			++i;
		if (i < len && buf[i] == '\n')
			++i;
	}

	pos = i;
}

void parser::skip_whitespace_and_comments(unsigned int &pos)
{
	unsigned int i = pos;

	while (true) {
		skip_whitespace(i);
		skip_comments(i);

		if (i == len)
			break;

		// If we didn't skip anything, we should stop
		if (i == pos)
			break;

		pos = i;
	}

	pos = i;
}

int parser::parse_literal_integer(unsigned int &pos)
{
	unsigned int i = pos;

	// TODO: this rejects hex digits, but if we use isxdigit() it
	// will consume non-numbers

	if (i < len && (isdigit(buf[i]) || buf[i] == '-'))
		++i;
	while (i < len && isdigit(buf[i]))
		++i;
	if (i == pos)
		return -1;

	std::string str(buf + pos, i - pos);

	if (i < len) {
		if (buf[i] == 'b') {
			++i;
		} else if (buf[i] == 'h') {
			++i;
		} else if (buf[i] == 'o') {
			++i;
		} else if (buf[i] == 'd') {
			++i;
		}
	}

	auto node_index = tree.new_node(AST_LITERAL_INTEGER, pos, i);

	pos = i;
	return node_index;
}

int parser::parse_literal_string(unsigned int &pos)
{
	unsigned int i = pos;

	if (i == len || buf[i] != '\"')
		return -1;
	++i;

	while (i < len && buf[i] != '\"') {
		if (buf[i] == '\\') {
			++i;
			if (i == len)
				throw syntax_error("unterminated string literal", pos, i);
		}

		++i;
	}

	if (i == len || buf[i] != '\"')
		throw syntax_error("unterminated string literal", pos, i);
	++i;

	auto node_index = tree.new_node(AST_LITERAL_STRING, pos, i);

	pos = i;
	return node_index;
}

int parser::parse_symbol_name(unsigned int &pos)
{
	unsigned int i = pos;

	if (i < len && (isalpha(buf[i]) || buf[i] == '_'))
		++i;
	while (i < len && (isalnum(buf[i]) || buf[i] == '_'))
		++i;
	if (i == pos)
		return -1;

	auto node_index = tree.new_node(AST_SYMBOL_NAME, pos, i);

	pos = i;
	return node_index;
}

int parser::parse_atom(unsigned int &pos)
{
	int ptr = -1;

	if (ptr == -1)
		ptr = parse_literal_integer(pos);
	if (ptr == -1)
		ptr = parse_literal_string(pos);
	if (ptr == -1)
		ptr = parse_symbol_name(pos);

	if (ptr != -1)
		skip_whitespace_and_comments(pos);

	return ptr;
}

template<ast_node_type type, unsigned int left_size, unsigned int right_size>
int parser::parse_outfix(const char (&left)[left_size], const char (&right)[right_size], unsigned int &pos)
{
	unsigned int i = pos;

	if (i + left_size - 1 >= len || strncmp(buf + i, left, left_size - 1))
		return -1;
	i += left_size - 1;

	skip_whitespace_and_comments(i);

	int operand = parse_expr(i);
	// operand can be -1 when parsing e.g. "()"

	skip_whitespace_and_comments(i);

	if (i + right_size - 1 > len || strncmp(buf + i, right, right_size - 1))
		throw syntax_error("expected terminator", i, i + right_size - 1);
	i += right_size - 1;

	auto node_index = tree.new_node(type, pos, i);
	auto node = tree.get(node_index);
	node->unop = operand;

	skip_whitespace_and_comments(i);

	pos = i;
	return node_index;
}

template<precedence prec, unsigned int op_size>
int parser::parse_unop_prefix_as_call(const char (&op)[op_size], const char *symbol_name, unsigned int &pos)
{
	unsigned int i = pos;

	if (i + op_size - 1 >= len || strncmp(buf + i, op, op_size - 1))
		return -1;
	i += op_size - 1;

	skip_whitespace_and_comments(i);

	auto operand = parse_expr(i, prec);
	if (operand == -1)
		return -1;

	auto symbol_name_node_index = tree.new_node(AST_SYMBOL_NAME, pos, i);
	auto symbol_name_node = tree.get(symbol_name_node_index);
	symbol_name_node->symbol_name = symbol_name;

	auto node_index = tree.new_node(AST_JUXTAPOSE, pos, i);
	auto node = tree.get(node_index);
	node->binop.lhs = symbol_name_node_index;
	node->binop.rhs = operand;

	skip_whitespace_and_comments(i);

	pos = i;
	return node_index;
}

// NOTE: We expect the caller to have parsed the left hand side already
template<ast_node_type type, precedence prec, associativity assoc, bool allow_trailing, unsigned int op_size>
int parser::parse_binop(const char (&op)[op_size], int lhs, unsigned int &pos, unsigned int min_precedence)
{
	assert(lhs != -1);

	if (prec < min_precedence)
		return -1;

	unsigned int i = pos;

	if (i + op_size - 1 >= len || strncmp(buf + i, op, op_size - 1))
		return -1;
	i += op_size - 1;

	skip_whitespace_and_comments(i);

	int rhs = parse_expr(i, prec + assoc);
	if (rhs == -1) {
		if (!allow_trailing)
			return -1;

		pos = i;
		return lhs;
	}

	auto node_index = tree.new_node(type, tree.get(lhs)->pos, i);
	auto node = tree.get(node_index);
	node->binop.lhs = lhs;
	node->binop.rhs = rhs;

	skip_whitespace_and_comments(i);

	pos = i;
	return node_index;
}

// Helper wrapper for parsing a binary operator as a call to a built-in macro.
// This is kind of a transformation of the "true" AST which puts a bit more of
// the language into the parser (and maybe makes it a bit less elegant). We
// also have to create 2 more node objects than we would have otherwise. But
// putting it here simplifies anything that needs to traverse the AST later,
// since it can handle these operators in a uniform way (as opposed to
// handling separate AST types for each built-in operator).
template<precedence prec, associativity assoc, bool allow_trailing, unsigned int op_size>
int parser::parse_binop_as_call(const char (&op)[op_size], const char *symbol_name,
	int lhs, unsigned int &pos, unsigned int min_precedence)
{
	unsigned int i = pos;

	auto args = parse_binop<AST_JUXTAPOSE, prec, assoc, allow_trailing>(op, lhs, i, min_precedence);
	if (args == -1)
		return -1;

	auto symbol_name_node_index = tree.new_node(AST_SYMBOL_NAME, pos, i);
	auto symbol_name_node = tree.get(symbol_name_node_index);
	symbol_name_node->symbol_name = symbol_name;

	auto node_index = tree.new_node(AST_JUXTAPOSE, pos, i);
	auto node = tree.get(node_index);
	node->binop.lhs = symbol_name_node_index;
	node->binop.rhs = args;

	skip_whitespace_and_comments(i);

	pos = i;
	return node_index;
}

int parser::parse_expr(unsigned int &pos, unsigned int min_precedence)
{
	unsigned int i = pos;

	/* Outfix unary operators */
	int lhs = -1;
	if (lhs == -1)
		lhs = parse_outfix<AST_BRACKETS>("(", ")", i);
	if (lhs == -1)
		lhs = parse_outfix<AST_SQUARE_BRACKETS>("[", "]", i);
	if (lhs == -1)
		lhs = parse_outfix<AST_CURLY_BRACKETS>("{", "}", i);

	/* Unary prefix operators */
	if (lhs == -1)
		lhs = parse_unop_prefix_as_call<PREC_AT>("@", "_eval", i);

	/* Infix binary operators (basically anything that starts with a literal) */
	if (lhs == -1)
		lhs = parse_atom(i);

	if (lhs == -1)
		return -1;

	int result = -1;

	while (true) {
		/* We want comma and semicolon lists to behave like they
		 * typically do in lisp, scheme, etc. where you have the
		 * head of the list as the first operand and then the rest
		 * of it as the second operand; therefore they should right
		 * associative. The same goes for juxtaposition. */

		// This must appear before ":" since that's a prefix
		if (result == -1)
			result = parse_binop_as_call<PREC_DEFINE, ASSOC_LEFT, false>(":=", "_define", lhs, i, min_precedence);

		if (result == -1)
			result = parse_binop<AST_MEMBER, PREC_MEMBER, ASSOC_LEFT, false>(".", lhs, i, min_precedence);
		if (result == -1)
			result = parse_binop_as_call<PREC_PAIR, ASSOC_LEFT, false>(":", "_declare", lhs, i, min_precedence);
		if (result == -1)
			result = parse_binop_as_call<PREC_MULTIPLY_DIVIDE, ASSOC_LEFT, false>("*", "_multiply", lhs, i, min_precedence);
		if (result == -1)
			result = parse_binop_as_call<PREC_MULTIPLY_DIVIDE, ASSOC_LEFT, false>("/", "_divide", lhs, i, min_precedence);
		if (result == -1)
			result = parse_binop_as_call<PREC_ADD_SUBTRACT, ASSOC_LEFT, false>("+", "_add", lhs, i, min_precedence);
		if (result == -1)
			result = parse_binop_as_call<PREC_ADD_SUBTRACT, ASSOC_LEFT, false>("-", "_subtract", lhs, i, min_precedence);
		if (result == -1)
			result = parse_binop<AST_COMMA, PREC_COMMA, ASSOC_RIGHT, true>(",", lhs, i, min_precedence);
		if (result == -1)
			result = parse_binop_as_call<PREC_EQUALITY, ASSOC_LEFT, false>("==", "_equals", lhs, i, min_precedence);
		if (result == -1)
			result = parse_binop_as_call<PREC_EQUALITY, ASSOC_LEFT, false>("!=", "_notequals", lhs, i, min_precedence);
		if (result == -1)
			result = parse_binop_as_call<PREC_EQUALITY, ASSOC_LEFT, false>("<", "_less", lhs, i, min_precedence);
		if (result == -1)
			result = parse_binop_as_call<PREC_EQUALITY, ASSOC_LEFT, false>("<=", "_less_equal", lhs, i, min_precedence);
		if (result == -1)
			result = parse_binop_as_call<PREC_EQUALITY, ASSOC_LEFT, false>(">", "_greater", lhs, i, min_precedence);
		if (result == -1)
			result = parse_binop_as_call<PREC_EQUALITY, ASSOC_LEFT, false>(">=", "_greater_equal", lhs, i, min_precedence);
		if (result == -1)
			result = parse_binop_as_call<PREC_ASSIGN, ASSOC_LEFT, false>("=", "_assign", lhs, i, min_precedence);
		if (result == -1)
			result = parse_binop<AST_SEMICOLON, PREC_SEMICOLON, ASSOC_RIGHT, true>(";", lhs, i, min_precedence);

		// This must appear last since it's a prefix of any other
		// operator.
		if (result == -1)
			result = parse_binop<AST_JUXTAPOSE, PREC_JUXTAPOSE, ASSOC_RIGHT, false>("", lhs, i, min_precedence);

		if (result == -1) {
			result = lhs;
			break;
		}

		lhs = result;
		result = -1;
	}

	pos = i;
	return result;
}

int parser::parse_doc(unsigned int &pos)
{
	unsigned int i = pos;

	skip_whitespace_and_comments(i);

	auto result = parse_expr(i);
	if (result == -1)
		throw syntax_error("expected expression", i, i + 1);

	if (i != len)
		throw syntax_error("expected end-of-file", i, len - 1);

	pos = i;
	return result;
}

#endif
