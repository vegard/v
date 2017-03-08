#ifndef V_BUILTIN_HH
#define V_BUILTIN_HH

#include <set>

#include "ast.hh"
#include "compile.hh"
#include "function.hh"
#include "scope.hh"
#include "value.hh"

static value_ptr builtin_macro_eval(function &f, scope_ptr s, ast_node_ptr node)
{
	return eval(f, s, node);
}

static value_ptr builtin_macro_define(function &f, scope_ptr s, ast_node_ptr node)
{
	auto lhs = node->binop.lhs;
	if (lhs->type != AST_SYMBOL_NAME)
		throw compile_error(node, "definition of non-symbol");

	auto rhs = compile(f, s, node->binop.rhs);
	value_ptr val;
	if (f.target_jit) {
		// For functions that are run at compile-time, we allocate
		// a new global value. The _name_ is still scoped as usual,
		// though.
		val = std::make_shared<value>(VALUE_GLOBAL, rhs->type);
		auto global = new uint8_t[rhs->type->size];
		val->global.host_address = (void *) global;
	} else {
		// Create new local
		val = f.alloc_local_value(rhs->type);
	}
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
	return std::make_shared<value>(VALUE_LOCAL, builtin_type_boolean);
}

static value_ptr builtin_macro_debug(function &f, scope_ptr s, ast_node_ptr node)
{
	// TODO: need context so we can print line numbers and stuff too
	node->dump();
	printf("\n");

	return std::make_shared<value>(VALUE_CONSTANT, builtin_type_void);
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
	if (condition_value->type != builtin_type_boolean)
		throw compile_error(condition_node, "'if' condition must be boolean");

	label false_label;
	f.emit_jump_if_zero(false_label);

	// "if" block
	auto true_value = compile(f, s, true_node);
	if (true_value->type != builtin_type_void) {
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
		if (false_value->type != builtin_type_void)
			f.emit_move(false_value, return_value);
	} else {
		if (true_value->type != builtin_type_void)
			throw compile_error(node, "expected 'else' since 'if' block has return value");
	}

	// next statement
	f.emit_label(end_label);

	// finalize
	f.link_label(false_label);
	f.link_label(end_label);

	// TODO: just return nullptr instead?
	if (!return_value) {
		return_value = std::make_shared<value>(VALUE_CONSTANT, builtin_type_void);
		//new (&return_value->constant.node) ast_node_ptr;
	}
	return return_value;
}

static value_ptr _call_fun(function &f, scope_ptr s, value_ptr fn, ast_node_ptr node)
{
	if (node->type != AST_BRACKETS)
		throw compile_error(node, "expected parantheses");

	// TODO: save/restore caller save registers
	auto type = fn->type;

	// TODO: abstract away ABI details
	machine_register args_regs[] = {
		RDI, RSI, RDX, RCX, R8, R9,
	};

	std::vector<std::pair<ast_node_ptr, value_ptr>> args;
	for (auto arg_node: traverse<AST_COMMA>(node->unop))
		args.push_back(std::make_pair(arg_node, compile(f, s, arg_node)));

	if (args.size() != type->argument_types.size())
		throw compile_error(node, "expected %u arguments; got %u", type->argument_types.size(), args.size());

	for (unsigned int i = 0; i < args.size(); ++i) {
		auto arg_node = args[i].first;
		auto arg_value = args[i].second;

		if (type->argument_types[i] != arg_value->type)
			throw compile_error(arg_node, "wrong argument type");

		f.emit_move(arg_value, args_regs[i]);
	}

	f.emit_call(fn);

	value_ptr ret_value = f.alloc_local_value(type->return_type);
	f.emit_move(RAX, ret_value);
	return ret_value;
}

static value_ptr builtin_macro_fun(function &f, scope_ptr s, ast_node_ptr node)
{
	// Extract parameters and code block from AST

	if (node->type != AST_JUXTAPOSE)
		throw compile_error(node, "expected 'fun (<expression>) <expression>'");

	auto brackets_node = node->binop.lhs;
	if (brackets_node->type != AST_BRACKETS)
		throw compile_error(brackets_node, "expected (<expression>...)");

	auto code_node = node->binop.rhs;
	auto args_node = brackets_node->unop;

	// TODO: abstract away ABI details
	machine_register args_regs[] = {
		RDI, RSI, RDX, RCX, R8, R9,
	};
	unsigned int current_arg = 0;

	auto new_f = std::make_shared<function>(false);
	new_f->emit_prologue();

	auto new_scope = std::make_shared<scope>(s);

	std::vector<value_type_ptr> argument_types;
	for (auto arg_node: traverse<AST_COMMA>(args_node)) {
		if (arg_node->type != AST_PAIR)
			throw compile_error(arg_node, "expected <name>: <type> pair");

		auto name_node = arg_node->binop.lhs;
		if (name_node->type != AST_SYMBOL_NAME)
			throw compile_error(arg_node, "argument name must be a symbol name");

		// Find the type by evaluating the <type> expression
		auto type_node = arg_node->binop.rhs;
		value_ptr arg_type_value = builtin_macro_eval(f, s, type_node);
		if (arg_type_value->storage_type != VALUE_GLOBAL)
			throw compile_error(type_node, "argument type must be known at compile time");
		if (arg_type_value->type != builtin_type_type)
			throw compile_error(type_node, "argument type must be an instance of a type");
		auto arg_type = *(value_type_ptr *) arg_type_value->global.host_address;

		argument_types.push_back(arg_type);

		// Define arguments as local values
		value_ptr arg_value = new_f->alloc_local_value(arg_type);
		new_scope->define(node, name_node->literal_string, arg_value);

		// TODO: use multiple regs or pass on stack
		if (arg_type->size > sizeof(unsigned long))
			throw compile_error(arg_node, "argument too big to fit in register");

		// TODO: pass args on stack if there are too many to fit in registers
		if (current_arg >= sizeof(args_regs) / sizeof(*args_regs))
			throw compile_error(arg_node, "too many arguments");

		new_f->emit_move(args_regs[current_arg++], arg_value);
	}

	// v is the return value of the compiled expression
	auto v = compile(*new_f, new_scope, code_node);
	auto v_type = v->type;

	// TODO: use multiple regs or pass on stack
	if (v_type->size > sizeof(unsigned long))
		throw compile_error(code_node, "return value too big to fit in register");

	if (v_type->size)
		new_f->emit_move(v, RAX);

	new_f->emit_epilogue();

	// Now that we know the function's return type, we can finalize
	// the signature and either find or create a type to represent
	// the function signature.
	//.push_back(v_type);

	// We memoise function types so that two functions with the same
	// signature always get the same type
	static std::map<std::pair<std::vector<value_type_ptr>, value_type_ptr>, value_type_ptr> signature_cache;

	// TODO: This is a *bit* ugly
	auto signature = std::make_pair(argument_types, v_type);

	value_type_ptr ret_type;
	auto it = signature_cache.find(signature);
	if (it == signature_cache.end()) {
		// Create new type for this signature
		ret_type = std::make_shared<value_type>();
		ret_type->alignment = 8;
		ret_type->size = 8;
		ret_type->constructor = nullptr;
		ret_type->argument_types = argument_types;
		ret_type->return_type = v_type;
		ret_type->call = _call_fun;

		signature_cache[signature] = ret_type;
	} else {
		ret_type = it->second;
	}

	// We need this to keep the new function from getting freed when this
	// function returns.
	// TODO: Another solution?
	static std::set<function_ptr> functions;
	functions.insert(new_f);

	auto ret = std::make_shared<value>(VALUE_GLOBAL, ret_type);
	void *mem = map(new_f);

	auto global = new void *;
	*global = mem;
	ret->global.host_address = (void *) global;

	disassemble((const uint8_t *) mem, new_f->bytes.size(), (uint64_t) mem, new_f->comments);
	return ret;
}

static value_ptr builtin_macro_add(function &f, scope_ptr s, ast_node_ptr node)
{
	// So this is probably a result of something (x + y), which got
	// parsed as (juxtapose _add (juxtapose x y)).
	// Compiling the "juxtapose" decided we're a macro, and "node" here
	// refers to the (brackets ...) part.
	//
	// What we'd like to do is to evaluate 'x' to figure out what type
	// it is. Once we know its type, we can call that type's ->add()
	// operator.
	//
	// In general, we should be careful about "type only" evaluations
	// because it's more expensive to first evaluate the type and then
	// evaluate the type AND value than to just evaluate the type and
	// the value at the same time.
	//
	// However, this allows operators to be macros, which is a very
	// powerful feature.

	assert(node->type == AST_JUXTAPOSE);

	// TODO: only evaluate the type so we don't evaluate the value twice
	auto lhs = compile(f, s, node->binop.lhs);
	auto lhs_type = lhs->type;

	if (!lhs_type->add)
		throw compile_error(node, "type doesn't support +");

	return lhs_type->add(f, s, lhs, node);
}

#endif
