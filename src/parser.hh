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

#include <gmpxx.h>

#include "ast.hh"

/*
 * Nullary/unary (outfix) operators:
 *   (x)
 *   [x]
 *   {x}
 *
 * Unary prefix operators:
 *   @x
 *
 * Binary operators (highest precedence first):
 *   x.y
 *   x y      x := y
 *   x:y
 *   x..y
 *   x * y    x / y
 *   x + y    x - y
 *   x = y
 *   x, y
 *   x; y
 */

enum precedence {
	PREC_SEMICOLON,
	PREC_AT,
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

	parser(const char *buf, size_t len):
		buf(buf),
		len(len)
	{
	}

	void skip_whitespace(unsigned int &pos);
	void skip_comments(unsigned int &pos);
	void skip_whitespace_and_comments(unsigned int &pos);

	ast_node_ptr parse_literal_integer(unsigned int &pos);
	ast_node_ptr parse_literal_string(unsigned int &pos);
	ast_node_ptr parse_symbol_name(unsigned int &pos);
	ast_node_ptr parse_atom(unsigned int &pos);

	template<ast_node_type type, unsigned int left_size, unsigned int right_size>
	ast_node_ptr parse_outfix(const char (&left)[left_size], const char (&right)[right_size], unsigned int &pos);

	template<precedence prec, unsigned int op_size>
	ast_node_ptr parse_unop_prefix_as_call(const char (&op)[op_size], const char *symbol_name, unsigned int &pos);

	template<ast_node_type type, precedence prec, associativity assoc, bool allow_trailing, unsigned int op_size>
	ast_node_ptr parse_binop(const char (&op)[op_size], ast_node_ptr lhs, unsigned int &pos, unsigned int min_precedence);

	template<precedence prec, associativity assoc, bool allow_trailing, unsigned int op_size>
	ast_node_ptr parse_binop_as_call(const char (&op)[op_size], const char *symbol_name,
		ast_node_ptr lhs, unsigned int &pos, unsigned int min_precedence);

	ast_node_ptr parse_expr(unsigned int &pos, unsigned int min_precedence = 0);

	ast_node_ptr parse_doc(unsigned int &pos);
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

ast_node_ptr parser::parse_literal_integer(unsigned int &pos)
{
	unsigned int i = pos;
	unsigned int base = 10;

	// TODO: this rejects hex digits, but if we use isxdigit() it
	// will consume non-numbers

	if (i < len && (isdigit(buf[i]) || buf[i] == '-'))
		++i;
	while (i < len && isdigit(buf[i]))
		++i;
	if (i == pos)
		return nullptr;

	std::string str(buf + pos, i - pos);

	if (i < len) {
		if (buf[i] == 'b') {
			base = 2;
			++i;
		} else if (buf[i] == 'h') {
			base = 16;
			++i;
		} else if (buf[i] == 'o') {
			base = 8;
			++i;
		} else if (buf[i] == 'd') {
			base = 10;
			++i;
		}
	}

	auto result = std::make_shared<ast_node>(AST_LITERAL_INTEGER, pos, i);
	new (&result->literal_integer) mpz_class();
	if (result->literal_integer.set_str(str, base))
		// TODO: need to free data->value?
		throw parse_error("mpz_set_str() returned an error", pos, i);

	pos = i;
	return result;
}

ast_node_ptr parser::parse_literal_string(unsigned int &pos)
{
	unsigned int i = pos;
	std::vector<char> str;

	if (i == len || buf[i] != '\"')
		return nullptr;
	++i;

	while (i < len && buf[i] != '\"') {
		if (buf[i] == '\\') {
			++i;
			if (i == len)
				throw syntax_error("unterminated string literal", pos, i);
		}

		str.push_back(buf[i]);
		++i;
	}

	if (i == len || buf[i] != '\"')
		throw syntax_error("unterminated string literal", pos, i);
	++i;

	auto result = std::make_shared<ast_node>(AST_LITERAL_STRING, pos, i);
	new (&result->literal_string) std::string(&str[0], str.size());

	pos = i;
	return result;
}

ast_node_ptr parser::parse_symbol_name(unsigned int &pos)
{
	unsigned int i = pos;

	if (i < len && (isalpha(buf[i]) || buf[i] == '_'))
		++i;
	while (i < len && (isalnum(buf[i]) || buf[i] == '_'))
		++i;
	if (i == pos)
		return nullptr;

	auto result = std::make_shared<ast_node>(AST_SYMBOL_NAME, pos, i);
	auto &data = result->symbol_name;
	new (&data) std::string(buf + pos, i - pos);

	pos = i;
	return result;
}

ast_node_ptr parser::parse_atom(unsigned int &pos)
{
	ast_node_ptr ptr;

	if (!ptr)
		ptr = parse_literal_integer(pos);
	if (!ptr)
		ptr = parse_literal_string(pos);
	if (!ptr)
		ptr = parse_symbol_name(pos);

	if (ptr)
		skip_whitespace_and_comments(pos);
	return ptr;
}

template<ast_node_type type, unsigned int left_size, unsigned int right_size>
ast_node_ptr parser::parse_outfix(const char (&left)[left_size], const char (&right)[right_size], unsigned int &pos)
{
	unsigned int i = pos;

	if (i + left_size - 1 >= len || strncmp(buf + i, left, left_size - 1))
		return nullptr;
	i += left_size - 1;

	skip_whitespace_and_comments(i);

	ast_node_ptr operand = parse_expr(i);
	// operand can be nullptr when parsing e.g. "()"

	skip_whitespace_and_comments(i);

	if (i + right_size - 1 > len || strncmp(buf + i, right, right_size - 1))
		throw syntax_error("expected terminator", i, i + right_size - 1);
	i += right_size - 1;

	auto result = std::make_shared<ast_node>(type, pos, i);
	new (&result->unop) ast_node_ptr(operand);

	skip_whitespace_and_comments(i);

	pos = i;
	return result;
}

template<precedence prec, unsigned int op_size>
ast_node_ptr parser::parse_unop_prefix_as_call(const char (&op)[op_size], const char *symbol_name, unsigned int &pos)
{
	unsigned int i = pos;

	if (i + op_size - 1 >= len || strncmp(buf + i, op, op_size - 1))
		return nullptr;
	i += op_size - 1;

	skip_whitespace_and_comments(i);

	ast_node_ptr operand = parse_expr(i, prec);
	if (!operand)
		return nullptr;

	auto symbol_name_node = std::make_shared<ast_node>(AST_SYMBOL_NAME, pos, i);
	new (&symbol_name_node->symbol_name) std::string(symbol_name);

	auto result = std::make_shared<ast_node>(AST_JUXTAPOSE, pos, i);
	new (&result->binop.lhs) ast_node_ptr(symbol_name_node);
	new (&result->binop.rhs) ast_node_ptr(operand);

	skip_whitespace_and_comments(i);

	pos = i;
	return result;
}

// NOTE: We expect the caller to have parsed the left hand side already
template<ast_node_type type, precedence prec, associativity assoc, bool allow_trailing, unsigned int op_size>
ast_node_ptr parser::parse_binop(const char (&op)[op_size], ast_node_ptr lhs, unsigned int &pos, unsigned int min_precedence)
{
	assert(lhs);

	if (prec < min_precedence)
		return nullptr;

	unsigned int i = pos;

	if (i + op_size - 1 >= len || strncmp(buf + i, op, op_size - 1))
		return nullptr;
	i += op_size - 1;

	skip_whitespace_and_comments(i);

	ast_node_ptr rhs = parse_expr(i, prec + assoc);
	if (!rhs) {
		if (!allow_trailing)
			return nullptr;

		pos = i;
		return lhs;
	}

	auto result = std::make_shared<ast_node>(type, lhs->pos, i);
	new (&result->binop.lhs) ast_node_ptr(lhs);
	new (&result->binop.rhs) ast_node_ptr(rhs);

	skip_whitespace_and_comments(i);

	pos = i;
	return result;
}

// Helper wrapper for parsing a binary operator as a call to a built-in macro.
// This is kind of a transformation of the "true" AST which puts a bit more of
// the language into the parser (and maybe makes it a bit less elegant). We
// also have to create 2 more node objects than we would have otherwise. But
// putting it here simplifies anything that needs to traverse the AST later,
// since it can handle these operators in a uniform way (as opposed to
// handling separate AST types for each built-in operator).
template<precedence prec, associativity assoc, bool allow_trailing, unsigned int op_size>
ast_node_ptr parser::parse_binop_as_call(const char (&op)[op_size], const char *symbol_name,
	ast_node_ptr lhs, unsigned int &pos, unsigned int min_precedence)
{
	unsigned int i = pos;

	auto args = parse_binop<AST_JUXTAPOSE, prec, assoc, allow_trailing>(op, lhs, i, min_precedence);
	if (!args)
		return nullptr;

	auto symbol_name_node = std::make_shared<ast_node>(AST_SYMBOL_NAME, pos, i);
	new (&symbol_name_node->symbol_name) std::string(symbol_name);

	auto result = std::make_shared<ast_node>(AST_JUXTAPOSE, pos, i);
	new (&result->binop.lhs) ast_node_ptr(symbol_name_node);
	new (&result->binop.rhs) ast_node_ptr(args);

	skip_whitespace_and_comments(i);

	pos = i;
	return result;
}

ast_node_ptr parser::parse_expr(unsigned int &pos, unsigned int min_precedence)
{
	unsigned int i = pos;

	/* Outfix unary operators */
	ast_node_ptr lhs = nullptr;
	if (!lhs)
		lhs = parse_outfix<AST_BRACKETS>("(", ")", i);
	if (!lhs)
		lhs = parse_outfix<AST_SQUARE_BRACKETS>("[", "]", i);
	if (!lhs)
		lhs = parse_outfix<AST_CURLY_BRACKETS>("{", "}", i);

	/* Unary prefix operators */
	if (!lhs)
		lhs = parse_unop_prefix_as_call<PREC_AT>("@", "_eval", i);

	/* Infix binary operators (basically anything that starts with a literal) */
	if (!lhs)
		lhs = parse_atom(i);

	if (!lhs)
		return nullptr;

	ast_node_ptr result = nullptr;

	while (true) {
		/* We want comma and semicolon lists to behave like they
		 * typically do in lisp, scheme, etc. where you have the
		 * head of the list as the first operand and then the rest
		 * of it as the second operand; therefore they should right
		 * associative. The same goes for juxtaposition. */

		// This must appear before ":" since that's a prefix
		if (!result)
			result = parse_binop_as_call<PREC_JUXTAPOSE, ASSOC_RIGHT, false>(":=", "_define", lhs, i, min_precedence);

		if (!result)
			result = parse_binop<AST_MEMBER, PREC_MEMBER, ASSOC_LEFT, false>(".", lhs, i, min_precedence);
		if (!result)
			result = parse_binop_as_call<PREC_PAIR, ASSOC_LEFT, false>(":", "_declare", lhs, i, min_precedence);
		if (!result)
			result = parse_binop_as_call<PREC_MULTIPLY_DIVIDE, ASSOC_LEFT, false>("*", "_multiply", lhs, i, min_precedence);
		if (!result)
			result = parse_binop_as_call<PREC_MULTIPLY_DIVIDE, ASSOC_LEFT, false>("/", "_divide", lhs, i, min_precedence);
		if (!result)
			result = parse_binop_as_call<PREC_ADD_SUBTRACT, ASSOC_LEFT, false>("+", "_add", lhs, i, min_precedence);
		if (!result)
			result = parse_binop_as_call<PREC_ADD_SUBTRACT, ASSOC_LEFT, false>("-", "_subtract", lhs, i, min_precedence);
		if (!result)
			result = parse_binop<AST_COMMA, PREC_COMMA, ASSOC_RIGHT, true>(",", lhs, i, min_precedence);
		if (!result)
			result = parse_binop_as_call<PREC_EQUALITY, ASSOC_LEFT, false>("==", "_equals", lhs, i, min_precedence);
		if (!result)
			result = parse_binop_as_call<PREC_EQUALITY, ASSOC_LEFT, false>("!=", "_notequals", lhs, i, min_precedence);
		if (!result)
			result = parse_binop_as_call<PREC_EQUALITY, ASSOC_LEFT, false>("<", "_less", lhs, i, min_precedence);
		if (!result)
			result = parse_binop_as_call<PREC_EQUALITY, ASSOC_LEFT, false>("<=", "_less_equal", lhs, i, min_precedence);
		if (!result)
			result = parse_binop_as_call<PREC_EQUALITY, ASSOC_LEFT, false>(">", "_greater", lhs, i, min_precedence);
		if (!result)
			result = parse_binop_as_call<PREC_EQUALITY, ASSOC_LEFT, false>(">=", "_greater_equal", lhs, i, min_precedence);
		if (!result)
			result = parse_binop_as_call<PREC_ASSIGN, ASSOC_LEFT, false>("=", "_assign", lhs, i, min_precedence);
		if (!result)
			result = parse_binop<AST_SEMICOLON, PREC_SEMICOLON, ASSOC_RIGHT, true>(";", lhs, i, min_precedence);

		// This must appear last since it's a prefix of any other
		// operator.
		if (!result)
			result = parse_binop<AST_JUXTAPOSE, PREC_JUXTAPOSE, ASSOC_RIGHT, false>("", lhs, i, min_precedence);

		if (result) {
			lhs = result;
			result = nullptr;
		} else {
			result = lhs;
			break;
		}
	}

	pos = i;
	return result;
}

ast_node_ptr parser::parse_doc(unsigned int &pos)
{
	unsigned int i = pos;

	skip_whitespace_and_comments(i);

	auto result = parse_expr(i);
	if (!result)
		throw syntax_error("expected expression", i, i + 1);

	if (i != len)
		throw syntax_error("expected end-of-file", i, len - 1);

	pos = i;
	return result;
}

#endif
