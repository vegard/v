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

unsigned int precedence(ast_node_type t)
{
	switch (t) {
	case AST_DIVIDE:
		t = AST_MULTIPLY;
		break;
	case AST_SUBTRACT:
		t = AST_ADD;
		break;
	default:
		break;
	}

	// Higher numerical values means stronger associativity
	return -t;
}

bool left_associative(ast_node_type t)
{
	switch (t) {
	case AST_JUXTAPOSE:
		return false;
	case AST_COMMA:
	case AST_SEMICOLON:
		/* We want comma and semicolon lists to behave like they
		 * typically do in lisp, scheme, etc. where you have the
		 * head of the list as the first operand and then the rest
		 * of it as the second operand. */
		return false;
	default:
		break;
	}

	return true;
}

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

	ast_node_ptr parse_literal_integer(unsigned int &pos);
	ast_node_ptr parse_literal_string(unsigned int &pos);
	ast_node_ptr parse_symbol_name(unsigned int &pos);
	ast_node_ptr parse_atom(unsigned int &pos);

	template<ast_node_type type, unsigned int left_size, unsigned int right_size>
	ast_node_ptr parse_outfix(const char (&left)[left_size], const char (&right)[right_size], unsigned int &pos);
	template<ast_node_type type, unsigned int op_size>
	ast_node_ptr parse_unop_prefix(const char (&op)[op_size], unsigned int &pos);
	template<ast_node_type type, unsigned int op_size>
	ast_node_ptr parse_binop(const char (&op)[op_size], ast_node_ptr lhs, unsigned int &pos, unsigned int min_precedence);

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

ast_node_ptr parser::parse_literal_integer(unsigned int &pos)
{
	unsigned int i = pos;
	unsigned int base = 10;

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
		skip_whitespace(pos);
	return ptr;
}

template<ast_node_type type, unsigned int left_size, unsigned int right_size>
ast_node_ptr parser::parse_outfix(const char (&left)[left_size], const char (&right)[right_size], unsigned int &pos)
{
	unsigned int i = pos;

	if (i + left_size - 1 >= len || strncmp(buf + i, left, left_size - 1))
		return nullptr;
	i += left_size - 1;

	skip_whitespace(i);

	ast_node_ptr operand = parse_expr(i);
	// operand can be nullptr when parsing e.g. "()"

	skip_whitespace(i);

	if (i + right_size - 1 > len || strncmp(buf + i, right, right_size - 1))
		throw syntax_error("expected terminator", i, i + right_size - 1);
	i += right_size - 1;

	auto result = std::make_shared<ast_node>(type, pos, i);
	new (&result->unop) ast_node_ptr(operand);

	skip_whitespace(i);

	pos = i;
	return result;
}

template<ast_node_type type, unsigned int op_size>
ast_node_ptr parser::parse_unop_prefix(const char (&op)[op_size], unsigned int &pos)
{
	unsigned int i = pos;

	if (i + op_size - 1 >= len || strncmp(buf + i, op, op_size - 1))
		return nullptr;
	i += op_size - 1;

	// TODO: decide whether to allow whitespace between a unary operator and its operand
	//skip_whitespace(i);

	ast_node_ptr operand = parse_expr(i, precedence(type));
	if (!operand)
		return nullptr;

	auto result = std::make_shared<ast_node>(type, pos, i);
	new (&result->unop) ast_node_ptr(operand);

	skip_whitespace(i);

	pos = i;
	return result;
}

// NOTE: We expect the caller to have parsed the left hand side already
template<ast_node_type type, unsigned int op_size>
ast_node_ptr parser::parse_binop(const char (&op)[op_size], ast_node_ptr lhs, unsigned int &pos, unsigned int min_precedence)
{
	assert(lhs);

	if (precedence(type) < min_precedence)
		return nullptr;

	unsigned int i = pos;

	if (i + op_size - 1 >= len || strncmp(buf + i, op, op_size - 1))
		return nullptr;
	i += op_size - 1;

	skip_whitespace(i);

	ast_node_ptr rhs = parse_expr(i, precedence(type) + left_associative(type));
	if (!rhs) {
		pos = i;
		return lhs;
	}

	auto result = std::make_shared<ast_node>(type, pos, i);
	new (&result->binop.lhs) ast_node_ptr(lhs);
	new (&result->binop.rhs) ast_node_ptr(rhs);

	skip_whitespace(i);

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
		lhs = parse_outfix<AST_ANGLE_BRACKETS>("<", ">", i);
	if (!lhs)
		lhs = parse_outfix<AST_CURLY_BRACKETS>("{", "}", i);

	/* Unary prefix operators */
	if (!lhs)
		lhs = parse_unop_prefix<AST_AT>("@", i);

	/* Infix binary operators (basically anything that starts with a literal) */
	if (!lhs)
		lhs = parse_atom(i);

	if (!lhs)
		return nullptr;

	ast_node_ptr result = nullptr;

	while (true) {
		// This must appear before ":" since that's a prefix
		if (!result)
			result = parse_binop<AST_DEFINE>(":=", lhs, i, min_precedence);

		if (!result)
			result = parse_binop<AST_MEMBER>(".", lhs, i, min_precedence);
		if (!result)
			result = parse_binop<AST_PAIR>(":", lhs, i, min_precedence);
		if (!result)
			result = parse_binop<AST_MULTIPLY>("*", lhs, i, min_precedence);
		if (!result)
			result = parse_binop<AST_DIVIDE>("/", lhs, i, min_precedence);
		if (!result)
			result = parse_binop<AST_ADD>("+", lhs, i, min_precedence);
		if (!result)
			result = parse_binop<AST_SUBTRACT>("-", lhs, i, min_precedence);
		if (!result)
			result = parse_binop<AST_COMMA>(",", lhs, i, min_precedence);
		if (!result)
			result = parse_binop<AST_ASSIGN>("=", lhs, i, min_precedence);
		if (!result)
			result = parse_binop<AST_SEMICOLON>(";", lhs, i, min_precedence);

		// This must appear last since it's a prefix of any other
		// operator.
		if (!result) {
			result = parse_binop<AST_JUXTAPOSE>("", lhs, i, min_precedence);
			// Special case: parse_binop() typically returns the lhs if
			// it didn't consume any characters, but for a "" operator it
			// means we didn't even consume the operator itself, and so
			// we should stop the search here (otherwise we would enter
			// an infinite loop).
			if (result == lhs)
				break;
		}

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

	auto result = parse_expr(i);
	if (!result)
		throw syntax_error("expected expression", i, i + 1);

	if (i != len)
		throw syntax_error("expected end-of-file", i, len - 1);

	pos = i;
	return result;
}

#if 0
#include "line_number_info.hh"

int main(int argc, char *argv[])
{
	//const char *doc = "";
	//const char *doc = "-123h";
	//const char *doc = "\"foo\\sdf\"";
	//const char *doc = "@_foobar_1234";
	//const char *doc = "(1,2,3)";
	//const char *doc = "(1,2,3,\n";
	//const char *doc = "(123.bar)";
	//const char *doc = "(((1;2)))\n";
	//const char *doc = "(();())\n";
	//const char *doc = "((foo.\"bar\",baz:122);1);2\n";
	//const char *doc = "@(1:2:3)\n";
	//const char *doc = "(1)\n";
	//const char *doc = "[1]\n";
	//const char *doc = "[(1)]\n";
	const char *doc = "(1, 2, 3);\n(3; 4; 5);\n";

	try {
		unsigned int pos = 0;
		parser p(doc, strlen(doc));
		auto node = p.parse_expr(pos);
		if (node) {
			node->dump();
			printf("\n");
		}
	} catch (const parse_error &e) {
		line_number_info line_numbers(doc, strlen(doc));
		auto pos = line_numbers.lookup(e.pos);
		auto end = line_numbers.lookup(e.end);

		printf("%u:%u: %s\n", pos.line, pos.column, e.what());
		if (pos.line == end.line) {
			printf("%.*s", pos.line_length, doc + pos.line_start);
			printf("%*s%s\n", pos.column, "", std::string(end.column - pos.column, '^').c_str());
		} else {
			// TODO: print following lines as well?
			printf("%.*s", pos.line_length, doc + pos.line_start);
			assert(pos.line_length - 1 >= pos.column + 1);
			printf("%*s%s\n", pos.column, "", std::string(pos.line_length - 1 - pos.column, '^').c_str());
		}
	}

	return 0;
}
#endif

#endif
