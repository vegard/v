#include <cassert>
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include <gmpxx.h>

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

enum ast_node_type {
	AST_UNKNOWN,

	/* Atoms */
	AST_LITERAL_INTEGER,
	AST_LITERAL_STRING,
	AST_SYMBOL_NAME,

	/* Nullary/unary outfix operators */
	AST_BRACKETS,
	AST_SQUARE_BRACKETS,
	AST_ANGLE_BRACKETS,
	AST_CURLY_BRACKETS,

	/* Unary prefix operators */
	AST_AT,

	/* Binary infix operators */
	AST_MEMBER,
	AST_PAIR,
	AST_MULTIPLY,
	AST_DIVIDE,
	AST_ADD,
	AST_SUBTRACT,
	AST_COMMA,
	AST_ASSIGN,
	AST_DEFINE,
	AST_SEMICOLON,
};

bool is_binop(ast_node_type t)
{
	switch (t) {
	case AST_MEMBER:
	case AST_PAIR:
	case AST_MULTIPLY:
	case AST_DIVIDE:
	case AST_ADD:
	case AST_SUBTRACT:
	case AST_COMMA:
	case AST_ASSIGN:
	case AST_DEFINE:
	case AST_SEMICOLON:
		return true;
	default:
		break;
	}

	return false;
}

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

	return t;
}

bool left_associative(ast_node_type t)
{
	switch (t) {
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
		case AST_ANGLE_BRACKETS:
		case AST_CURLY_BRACKETS:
		case AST_AT:
			unop.~ast_node_ptr();
			break;

		/* Binary operators */
		case AST_MEMBER:
		case AST_PAIR:
		case AST_MULTIPLY:
		case AST_DIVIDE:
		case AST_ADD:
		case AST_SUBTRACT:
		case AST_COMMA:
		case AST_ASSIGN:
		case AST_DEFINE:
		case AST_SEMICOLON:
			binop.lhs.~ast_node_ptr();
			binop.rhs.~ast_node_ptr();
			break;
		}
	}

	void dump_unop(FILE *fp, unsigned int indent, const char *name)
	{
		if (unop) {
			fprintf(fp, "%*s(%s\n", indent, "", name);
			unop->dump(fp, indent + 4);
			fprintf(fp, "\n");
			fprintf(fp, "%*s)", indent, "");
		} else {
			fprintf(fp, "%*s(parens)", indent, "");
		}
	}

	void dump_binop(FILE *fp, unsigned int indent, const char *name)
	{
		assert(binop.lhs);
		assert(binop.rhs);

		fprintf(fp, "%*s(%s\n", indent, "", name);
		binop.lhs->dump(fp, indent + 4);
		fprintf(fp, "\n");
		binop.rhs->dump(fp, indent + 4);
		fprintf(fp, "\n");
		fprintf(fp, "%*s)", indent, "");
	}

	void dump(FILE *fp = stdout, unsigned int indent = 0)
	{
		switch (type) {
		case AST_UNKNOWN:
			fprintf(fp, "%*s(unknown)", indent, "");
			break;

		case AST_LITERAL_INTEGER:
			fprintf(fp, "%*s(literal_integer %s)", indent, "", literal_integer.get_str().c_str());
			break;
		case AST_LITERAL_STRING:
			fprintf(fp, "%*s(literal_string \"%s\")", indent, "", literal_string.c_str());
			break;
		case AST_SYMBOL_NAME:
			fprintf(fp, "%*s(symbol_name %s)", indent, "", symbol_name.c_str());
			break;

		case AST_BRACKETS:
			dump_unop(fp, indent, "brackets");
			break;
		case AST_SQUARE_BRACKETS:
			dump_unop(fp, indent, "square-brackets");
			break;
		case AST_ANGLE_BRACKETS:
			dump_unop(fp, indent, "angle-brackets");
			break;
		case AST_CURLY_BRACKETS:
			dump_unop(fp, indent, "curly-brackets");
			break;

		case AST_AT:
			dump_unop(fp, indent, "at");
			break;

		case AST_MEMBER:
			dump_binop(fp, indent, "member");
			break;
		case AST_PAIR:
			dump_binop(fp, indent, "pair");
			break;
		case AST_MULTIPLY:
			dump_binop(fp, indent, "multiply");
			break;
		case AST_DIVIDE:
			dump_binop(fp, indent, "divide");
			break;
		case AST_ADD:
			dump_binop(fp, indent, "add");
			break;
		case AST_SUBTRACT:
			dump_binop(fp, indent, "subtract");
			break;
		case AST_COMMA:
			dump_binop(fp, indent, "comma");
			break;
		case AST_ASSIGN:
			dump_binop(fp, indent, "assign");
			break;
		case AST_DEFINE:
			dump_binop(fp, indent, "define");
			break;
		case AST_SEMICOLON:
			dump_binop(fp, indent, "semicolon");
			break;
		}
	}
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

	ast_node_ptr parse_literal_integer(unsigned int &pos);
	ast_node_ptr parse_literal_string(unsigned int &pos);
	ast_node_ptr parse_symbol_name(unsigned int &pos);
	ast_node_ptr parse_atom(unsigned int &pos);

	template<ast_node_type type, unsigned int left_size, unsigned int right_size>
	ast_node_ptr parse_outfix(const char (&left)[left_size], const char (&right)[right_size], unsigned int &pos);
	template<ast_node_type type, unsigned int op_size>
	ast_node_ptr parse_unop_prefix(const char (&op)[op_size], unsigned int &pos);
	template<ast_node_type type, unsigned int op_size>
	ast_node_ptr parse_binop(const char (&op)[op_size], ast_node_ptr lhs, unsigned int &pos);

	ast_node_ptr parse_expr(unsigned int &pos);

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

	ast_node_ptr operand = parse_expr(i);
	if (!operand)
		return nullptr;

	auto result = std::make_shared<ast_node>(type, pos, i);
	new (&result->unop) ast_node_ptr(operand);

	skip_whitespace(i);

	pos = i;
	return result;
}

static bool should_shuffle_args(ast_node_type lhs_type, ast_node_type rhs_type)
{
	if (!is_binop(rhs_type))
		return false;
	if (precedence(lhs_type) < precedence(rhs_type))
		return true;
	if (precedence(lhs_type) != precedence(rhs_type))
		return false;
	return left_associative(lhs_type);
}

// NOTE: We expect the caller to have parsed the left hand side already
template<ast_node_type type, unsigned int op_size>
ast_node_ptr parser::parse_binop(const char (&op)[op_size], ast_node_ptr lhs, unsigned int &pos)
{
	unsigned int i = pos;

	assert(lhs);

	if (i + op_size - 1 >= len || strncmp(buf + i, op, op_size - 1))
		return nullptr;
	i += op_size - 1;

	skip_whitespace(i);

	// TODO: don't require the final operand in e.g. (a, b, c)
	ast_node_ptr rhs = parse_expr(i);
	if (!rhs) {
		//throw syntax_error("expected expression", pos, i);
		pos = i;
		return lhs;
	}

	// Jon Blow-style precedence handling
	// <https://twitter.com/jonathan_blow/status/747623921822760960>
	if (should_shuffle_args(type, rhs->type)) {
		// It would have been nicer to make this constant-time,
		// but I don't think it's possible.
		auto *pivot = &rhs;
		while (should_shuffle_args(type, (*pivot)->binop.lhs->type))
			pivot = &(*pivot)->binop.lhs;

		auto new_lhs = std::make_shared<ast_node>(type, pos, i);
		new (&new_lhs->binop.lhs) ast_node_ptr(lhs);
		new (&new_lhs->binop.rhs) ast_node_ptr((*pivot)->binop.lhs);

		(*pivot)->binop.lhs = new_lhs;

		skip_whitespace(i);

		pos = i;
		return rhs;
	}

	auto result = std::make_shared<ast_node>(type, pos, i);
	new (&result->binop.lhs) ast_node_ptr(lhs);
	new (&result->binop.rhs) ast_node_ptr(rhs);

	skip_whitespace(i);

	pos = i;
	return result;
}

ast_node_ptr parser::parse_expr(unsigned int &pos)
{
	//printf("parse_expr(%u, \"%.*s\")\n", pos, len - pos, buf + pos);

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

	// This must appear before ":" since that's a prefix
	if (!result)
		result = parse_binop<AST_DEFINE>(":=", lhs, i);

	if (!result)
		result = parse_binop<AST_MEMBER>(".", lhs, i);
	if (!result)
		result = parse_binop<AST_PAIR>(":", lhs, i);
	if (!result)
		result = parse_binop<AST_MULTIPLY>("*", lhs, i);
	if (!result)
		result = parse_binop<AST_DIVIDE>("/", lhs, i);
	if (!result)
		result = parse_binop<AST_ADD>("+", lhs, i);
	if (!result)
		result = parse_binop<AST_SUBTRACT>("-", lhs, i);
	if (!result)
		result = parse_binop<AST_COMMA>(",", lhs, i);
	if (!result)
		result = parse_binop<AST_ASSIGN>("=", lhs, i);
	if (!result)
		result = parse_binop<AST_SEMICOLON>(";", lhs, i);
	if (!result)
		result = lhs;

	pos = i;
	return result;

	// TODO: expression juxtaposition
	assert(false);
	result = std::make_shared<ast_node>();
	// TODO

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
