#ifndef V_BUILTIN_HH
#define V_BUILTIN_HH

#include "ast.hh"
#include "compile.hh"
#include "function.hh"
#include "scope.hh"
#include "value.hh"

static void run(function_ptr f)
{
	size_t length = (f->bytes.size() + 4095) & ~4095;
	void *mem = mmap(NULL, length,
		PROT_READ | PROT_WRITE | PROT_EXEC,
		MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
	if (mem == MAP_FAILED)
		throw std::runtime_error(format("mmap() failed: %s", strerror(errno)));

	memcpy(mem, &f->bytes[0], f->bytes.size());

	// Flush instruction cache so we know we'll
	// execute what we compiled and not some
	// garbage that happened to be in the cache.
	__builtin___clear_cache((char *) mem, (char *) mem + length);

	// TODO: ABI
	auto ret = f->return_value;
	if (!ret || ret->type->size == 0) {
		// No return value
		auto fn = (void (*)()) mem;
		fn();
	} else if (ret->type->size > sizeof(unsigned long)) {
		// TODO
		// If the return value is bigger than a long, we need to pass
		// a pointer to it as the first argument.
		//auto fn = (void (*)(void *)) mem;
		//fn();
		assert(false);
	} else {
		// The return value fits in a long
		auto fn = (long (*)()) mem;
		fn();
	}

	munmap(mem, length);
}

static value_ptr builtin_macro_eval(function &f, scope_ptr s, ast_node_ptr node)
{
	auto new_f = std::make_shared<function>();
	new_f->emit_prologue();
	auto v = compile(*new_f, s, node);
	new_f->emit_epilogue();

	run(new_f);

	// TODO
	return v;
}

static value_ptr builtin_macro_define(function &f, scope_ptr s, ast_node_ptr node)
{
	auto lhs = node->binop.lhs;
	if (lhs->type != AST_SYMBOL_NAME)
		throw compile_error(node, "definition of non-symbol");

	// Create new local
	auto rhs = compile(f, s, node->binop.rhs);
	auto val = f.alloc_local_value(rhs->type);
	s->define(node, lhs->symbol_name, val);
	f.emit_move(rhs, val);
	return val;
}

static value_ptr builtin_macro_assign(function &f, scope_ptr s, ast_node_ptr node)
{
	auto rhs = compile(f, s, node->binop.rhs);
	auto lhs = compile(f, s, node->binop.lhs);
	f.emit_move(rhs, lhs);
	return lhs;
}

static value_ptr builtin_macro_equals(function &f, scope_ptr s, ast_node_ptr node)
{
	auto lhs = compile(f, s, node->binop.lhs);
	auto rhs = compile(f, s, node->binop.rhs);
	if (lhs->type != rhs->type)
		throw compile_error(node, "cannot compare values of different types");

	f.emit_compare(lhs, rhs);

	// TODO: actually set the value
	return std::make_shared<value>(VALUE_LOCAL, &builtin_type_boolean);
}

static value_ptr builtin_macro_debug(function &f, scope_ptr s, ast_node_ptr node)
{
	// TODO: need context so we can print line numbers and stuff too
	node->dump();
	printf("\n");

	return std::make_shared<value>(VALUE_CONSTANT, &builtin_type_void);
}

static value_ptr builtin_macro_if(function &f, scope_ptr s, ast_node_ptr node)
{
	// Extract condition, true block, and false block (if any) from AST
	//
	// Input: "if a b else c";
	// Parse tree:
	// (juxtapose
	//     (symbol_name if)
	//     (juxtapose <-- node
	//         (symbol_name a) <-- node->binop.lhs AKA condition_node
	//         (juxtapose      <-- node->binop.rhs AKA rhs
	//             (symbol_name b) <-- rhs->binop.lhs AKA true_node
	//             (juxtapose      <-- rhs->binop.rhs AKA rhs
	//                 (symbol_name else) <-- rhs->binop.lhs AKA else_node
	//                 (symbol_name b)    <-- rhs->binop.rhs AKA false_node
	//             )
	//         )
	//     )
	// )

	if (node->type != AST_JUXTAPOSE)
		throw compile_error(node, "expected 'if <expression> <expression>'");

	ast_node_ptr condition_node = node->binop.lhs;
	ast_node_ptr true_node;
	ast_node_ptr false_node;

	auto rhs = node->binop.rhs;
	if (rhs->type == AST_JUXTAPOSE) {
		true_node = rhs->binop.lhs;

		rhs = rhs->binop.rhs;
		if (rhs->type != AST_JUXTAPOSE)
			throw compile_error(rhs, "expected 'else <expression>'");

		auto else_node = rhs->binop.lhs;
		if (else_node->type != AST_SYMBOL_NAME || else_node->symbol_name != "else")
			throw compile_error(else_node, "expected 'else'");

		false_node = rhs->binop.rhs;
	} else {
		true_node = rhs;
	}

	// Got all the bits that we need, now try to compile it.

	value_ptr return_value;

	// "if" condition
	auto condition_value = compile(f, s, condition_node);
	if (condition_value->type != &builtin_type_boolean)
		throw compile_error(condition_node, "'if' condition must be boolean");

	label false_label;
	f.emit_jump_if_zero(false_label);

	// "if" block
	auto true_value = compile(f, s, true_node);
	if (true_value->type != &builtin_type_void) {
		return_value = f.alloc_local_value(true_value->type);
		f.emit_move(true_value, return_value);
	}

	label end_label;
	f.emit_jump(end_label);

	// "else" block
	f.emit_label(false_label);
	if (false_node) {
		auto false_value = compile(f, s, false_node);
		if (false_value->type != true_value->type)
			throw compile_error(false_node, "'else' block must return the same type as 'if' block");
		if (false_value->type != &builtin_type_void)
			f.emit_move(false_value, return_value);
	} else {
		if (true_value->type != &builtin_type_void)
			throw compile_error(node, "expected 'else' since 'if' block has return value");
	}

	// next statement
	f.emit_label(end_label);

	// finalize
	f.link_label(false_label);
	f.link_label(end_label);

	// TODO: just return nullptr instead?
	if (!return_value) {
		return_value = std::make_shared<value>(VALUE_CONSTANT, &builtin_type_void);
		//new (&return_value->constant.node) ast_node_ptr;
	}
	return return_value;
}

static value_ptr builtin_macro_fun(function &f, scope_ptr s, ast_node_ptr node)
{
	// TODO
	assert(false);
	return nullptr;
}

#endif
