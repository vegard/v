#ifndef V_AST_HH
#define V_AST_HH

#include <cassert>
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <stdexcept>
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
	AST_ANGLE_BRACKETS,
	AST_CURLY_BRACKETS,

	/* Unary prefix operators */
	AST_AT,

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
		case AST_ANGLE_BRACKETS:
		case AST_CURLY_BRACKETS:
		case AST_AT:
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
		case AST_JUXTAPOSE:
			dump_binop(fp, indent, "juxtapose");
			break;
		case AST_COMMA:
			dump_binop(fp, indent, "comma");
			break;
		case AST_SEMICOLON:
			dump_binop(fp, indent, "semicolon");
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
