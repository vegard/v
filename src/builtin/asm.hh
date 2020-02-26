//
//  V compiler
//  Copyright (C) 2018  Vegard Nossum <vegard.nossum@gmail.com>
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

#ifndef V_BUILTIN_ASM_HH
#define V_BUILTIN_ASM_HH

#include "../ast.hh"
#include "../compile.hh"
#include "../function.hh"
#include "../scope.hh"
#include "../value.hh"
#include "../builtin/str.hh"
#include "../x86_64.hh"

// TODO: This is specific to the x86_64 backend.
// TODO: We should probably make the backend itself define the 'asm' macro
// so that it can have access to exactly the right values and call exactly
// the right functions to emit the code that it needs to

static auto builtin_type_asm_register = std::make_shared<value_type>(value_type {
	.alignment = alignof(machine_register),
	.size = sizeof(machine_register),
});

struct asm_assign_input_macro: macro {
	value_ptr invoke(ast_node_ptr node)
	{
		// TODO: we have to be *really* careful not to clobber
		// the already-assigned registers when compiling the
		// RHS below. One solution is to ensure that all the
		// values are VALUE_LOCALs since (at least for x86)
		// these can be moved to a register without using any
		// other register.

		if (node->type != AST_JUXTAPOSE)
			state->error(node, "expected juxtaposition");

		auto src_node = state->source->tree.get(node->binop.rhs);
		auto src_value = compile(src_node);

		auto dest_node = state->source->tree.get(node->binop.lhs);
		auto dest_value = eval(dest_node);
		if (dest_value->type != builtin_type_asm_register)
			state->error(dest_node, "expected register");
		if (dest_value->storage_type != VALUE_GLOBAL)
			state->error(dest_node, "expected compile-time constant");

		auto f = std::dynamic_pointer_cast<x86_64_function>(state->function);
		if (!f)
			state->error(node, "x86_64 inline asm used in non-x86_64 function");

		// TODO: check that size matches register
		assert(src_value->type->size == 8);

		machine_register dest_reg = *(machine_register *) dest_value->global.host_address;

		// TODO: we probably need to handle %rsp/%rbp specially
		assert(dest_reg != RBP);
		assert(dest_reg != RSP);

		f->emit_move(src_value, 0, dest_reg);
		return builtin_value_void;
	}
};

struct asm_assign_output_macro: macro {
	value_ptr invoke(ast_node_ptr node)
	{
		// TODO
		return builtin_value_void;
	}
};

struct asm_mov_macro: macro {
	value_ptr invoke(ast_node_ptr node)
	{
		if (node->type != AST_BRACKETS)
			state->error(node, "expected (reg, reg)");

		auto unop = state->source->tree.get(node->unop);
		if (unop->type != AST_COMMA)
			state->error(node, "expected (reg, reg)");

		node = unop;

		auto src_node = state->source->tree.get(node->binop.lhs);
		auto src_value = eval(src_node);
		if (src_value->type != builtin_type_asm_register)
			state->error(src_node, "expected register");
		if (src_value->storage_type != VALUE_GLOBAL)
			state->error(src_node, "expected compile-time constant");

		auto dest_node = state->source->tree.get(node->binop.rhs);
		auto dest_value = eval(dest_node);
		if (dest_value->type != builtin_type_asm_register)
			state->error(dest_node, "expected register");
		if (dest_value->storage_type != VALUE_GLOBAL)
			state->error(dest_node, "expected compile-time constant");

		auto f = std::dynamic_pointer_cast<x86_64_function>(state->function);
		if (!f)
			state->error(node, "x86_64 inline asm used in non-x86_64 function");

		f->emit_move_reg_to_reg(*(machine_register *) src_value->global.host_address,
			*(machine_register *) dest_value->global.host_address);
		return builtin_value_void;
	}
};

struct asm_syscall_macro: macro {
	value_ptr invoke(ast_node_ptr node)
	{
		if (node->type != AST_BRACKETS || state->source->tree.get(node->unop))
			state->error(node, "expected ()");

		auto f = std::dynamic_pointer_cast<x86_64_function>(state->function);
		if (!f)
			state->error(node, "x86_64 inline asm used in non-x86_64 function");

		f->emit_byte(0x0f);
		f->emit_byte(0x05);
		return builtin_value_void;
	}
};

static value_ptr builtin_macro_asm(ast_node_ptr node)
{
	if (node->type != AST_JUXTAPOSE)
		state->error(node, "expected juxtaposition");

	auto inputs_node = state->source->tree.get(node->binop.lhs);
	node = state->source->tree.get(node->binop.rhs);

	if (node->type != AST_JUXTAPOSE)
		state->error(node, "expected juxtaposition");

	auto outputs_node = state->source->tree.get(node->binop.lhs);
	auto asm_node = state->source->tree.get(node->binop.rhs);

	auto new_scope = std::make_shared<scope>(state->scope);

	// registers
	new_scope->define_builtin_constant("rax", builtin_type_asm_register, RAX);
	new_scope->define_builtin_constant("rcx", builtin_type_asm_register, RCX);
	new_scope->define_builtin_constant("rdx", builtin_type_asm_register, RDX);
	new_scope->define_builtin_constant("rbx", builtin_type_asm_register, RBX);
	new_scope->define_builtin_constant("rsp", builtin_type_asm_register, RSP);
	new_scope->define_builtin_constant("rbp", builtin_type_asm_register, RBP);
	new_scope->define_builtin_constant("rsi", builtin_type_asm_register, RSI);
	new_scope->define_builtin_constant("rdi", builtin_type_asm_register, RDI);
	new_scope->define_builtin_constant("r8", builtin_type_asm_register, R8);
	new_scope->define_builtin_constant("r9", builtin_type_asm_register, R9);
	new_scope->define_builtin_constant("r10", builtin_type_asm_register, R10);
	new_scope->define_builtin_constant("r11", builtin_type_asm_register, R11);
	new_scope->define_builtin_constant("r12", builtin_type_asm_register, R12);
	new_scope->define_builtin_constant("r13", builtin_type_asm_register, R13);
	new_scope->define_builtin_constant("r14", builtin_type_asm_register, R14);
	new_scope->define_builtin_constant("r15", builtin_type_asm_register, R15);

	auto inputs_scope = std::make_shared<scope>(new_scope);
	inputs_scope->define_builtin_macro("_assign", std::make_shared<asm_assign_input_macro>());
	(use_scope(inputs_scope), compile(inputs_node));

	auto outputs_scope = std::make_shared<scope>(new_scope);
	outputs_scope->define_builtin_macro("_assign", std::make_shared<asm_assign_output_macro>());
	(use_scope(outputs_scope), compile(outputs_node));

	// TODO: this is architecture-specific for now
	auto asm_scope = std::make_shared<scope>(new_scope);

	// instructions
	asm_scope->define_builtin_macro("mov", std::make_shared<asm_mov_macro>());
	asm_scope->define_builtin_macro("syscall", std::make_shared<asm_syscall_macro>());

	(use_scope(asm_scope), compile(asm_node));

	return builtin_value_void;
}

#endif
