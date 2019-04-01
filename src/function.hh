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

#ifndef V_FUNCTION_HH
#define V_FUNCTION_HH

extern "C" {
#include <elf.h>
}

#include <map>
#include <memory>

#include "value.hh"

// Instruction encoding

enum machine_register {
	RAX,
	RCX,
	RDX,
	RBX,
	RSP,
	RBP,
	RSI,
	RDI,

	R8,
	R9,
	R10,
	R11,
	R12,
	R13,
	R14,
	R15,
};

const unsigned int REX = 0x40;
const unsigned int REX_B = 0x01;
const unsigned int REX_X = 0x02;
const unsigned int REX_R = 0x04;
const unsigned int REX_W = 0x08;

// TODO: arch-specific
// TODO: abstract away ABI details

static const machine_register regs[] = {
	RDI, RSI, RDX, RCX, R8, R9,
};

struct args_allocator {
	const machine_register *it;

	args_allocator():
		it(std::cbegin(regs))
	{
	}

	machine_register next()
	{
		// TODO: use stack
		assert(it != std::cend(regs));
		return *(it++);
	}
};

// There are many design decisions to make here:
//  - How generic do we make labels/relocations?
//  - Who is responsible for linking labels? (The caller, the label
//    destructor, the function, etc.?)
//  - If we already know a label's address, do we output it immediately
//    instead of linking it at the end?

struct jump_relocation {
	unsigned int addr;
	unsigned int offset;
};

struct label {
	unsigned int addr;
	std::vector<jump_relocation> relocations;
};

struct function;
typedef std::shared_ptr<function> function_ptr;

struct function {
	bool host;
	std::vector<value_type_ptr> args_types;
	value_type_ptr return_type;

	std::vector<value_ptr> args_values;
	value_ptr return_value;

	// XXX: the double indirection is bad, we should collect bytes
	// ourselves directly and then move it into the object at the end
	object_ptr this_object;
	std::vector<uint8_t> &bytes;

	unsigned int indentation;

	// offset (into "bytes") where we need to write the final frame size
	// after we know how many locals we have.
	unsigned long frame_size_addr;

	unsigned int next_local_slot;

	explicit function(context_ptr c, bool host, std::vector<value_type_ptr> args_types, value_type_ptr return_type):
		host(host),
		args_types(args_types),
		return_type(return_type),
		this_object(std::make_shared<object>()),
		bytes(this_object->bytes),
		indentation(0),
		// slot 0 is the return address
		// slot 1 is the saved rbx
		next_local_slot(16)
	{
		// TODO: what does it mean to pass 'void' as argument or return type?

		for (auto arg_type: args_types) {
			assert(arg_type);

			if (arg_type->size == 0)
				args_values.push_back(builtin_value_void);
			else if (arg_type->size <= 8)
				args_values.push_back(alloc_local_value(c, arg_type));
			else
				args_values.push_back(alloc_local_pointer_value(c, arg_type));
		}

		assert(return_type);

		if (return_type->size == 0)
			return_value = builtin_value_void;
		else if (return_type->size <= 8)
			return_value = alloc_local_value(c, return_type);
		else
			return_value = alloc_local_pointer_value(c, return_type);
	}

	value_ptr alloc_local_value(context_ptr c, value_type_ptr type)
	{
		if (type->size == 0 || type->alignment == 0)
			return std::make_shared<value>(c, VALUE_CONSTANT, type);

		auto result = std::make_shared<value>(c, VALUE_LOCAL, type);

		// TODO: we could try to rearrange/pack values to avoid wasting stack space
		unsigned int offset = (next_local_slot + type->size + type->alignment - 1) & ~(type->alignment - 1);
		assert(offset > 0);
		result->local.offset = -offset;
		next_local_slot = next_local_slot + type->size;

		return result;
	}

	value_ptr alloc_local_pointer_value(context_ptr c, value_type_ptr type)
	{
		assert(type != builtin_type_void);
		assert(type->size > 0);
		assert(type->alignment > 0);

		auto result = std::make_shared<value>(c, VALUE_LOCAL_POINTER, type);

		unsigned int size = sizeof(void *);
		unsigned int alignment = alignof(void *);

		// TODO: we could try to rearrange/pack values to avoid wasting stack space
		unsigned int offset = (next_local_slot + size + alignment - 1) & ~(alignment - 1);
		assert(offset > 0);
		result->local.offset = -offset;
		next_local_slot = next_local_slot + size;

		return result;
	}

	// Helpers for indenting code + comments
	void enter()
	{
		++indentation;
	}

	void leave()
	{
		--indentation;
	}

	void comment(std::string s)
	{
		this_object->comments.push_back(::comment(bytes.size(), indentation, s));
	}

	void emit_byte(uint8_t v)
	{
		bytes.push_back(v);
	}

	void emit_long(uint32_t v)
	{
		bytes.push_back(v);
		bytes.push_back(v >> 8);
		bytes.push_back(v >> 16);
		bytes.push_back(v >> 24);
	}

	void emit_quad(uint64_t v)
	{
		bytes.push_back(v);
		bytes.push_back(v >> 8);
		bytes.push_back(v >> 16);
		bytes.push_back(v >> 24);
		bytes.push_back(v >> 32);
		bytes.push_back(v >> 40);
		bytes.push_back(v >> 48);
		bytes.push_back(v >> 56);
	}

	void emit_long_placeholder()
	{
		// emit something that we can recognise as a bogus value
		// if we forget to replace it
		emit_long(0x5a5a5a5a);
	}

	void overwrite_long(unsigned int addr, uint32_t v)
	{
		bytes[addr] = v;
		bytes[addr + 1] = v >> 8;
		bytes[addr + 2] = v >> 16;
		bytes[addr + 3] = v >> 24;
	}

	void emit_quad_placeholder()
	{
		// emit something that we can recognise as a bogus value
		// if we forget to replace it
		emit_quad(0x5b5b5b5b5b5b5b5b);
	}

	void emit_obj(unsigned int object_id)
	{
		// this emits a relocation for the address of the given object
		this_object->relocations.push_back(relocation(R_X86_64_64, bytes.size(), object_id));
		emit_quad_placeholder();
	}

	void emit_obj_PC32(unsigned int object_id, int addend)
	{
		// this emits a relocation for the address of the given object
		this_object->relocations.push_back(relocation(R_X86_64_PC32, bytes.size(), object_id, addend));
		emit_long_placeholder();
	}

	void emit_prologue()
	{
		comment("prologue");

		// pushq %rbp
		emit_byte(0x55);

		// movq %rsp, %rbp
		emit_byte(0x48);
		emit_byte(0x89);
		emit_byte(0xe5);

		// pushq %rbx
		// We save/restore %rbx because it's a callee saved register and
		// we use it to store temporary values.
		emit_byte(0x53);

		// subq $x, %rsp
		emit_byte(0x48);
		emit_byte(0x81);
		emit_byte(0xec);
		frame_size_addr = bytes.size();
		emit_long_placeholder();

		// save args in locals
		args_allocator regs;

		// large or non-POD return type passes a pointer in the
		// first argument
		if (return_value->type->size == 0) {
		} else if (return_value->type->size <= 8) {
		} else {
			auto reg = regs.next();
			comment("move large retval to local");
			emit_move_reg_to_mreg_offset(reg, RBP, return_value->local.offset);
		}

		for (auto arg_value: args_values) {
			if (arg_value->type->size == 0) {
			} else if (arg_value->type->size <= 8) {
				emit_move(regs.next(), arg_value, 0);
			} else {
				comment("move large arg to local");

				// argument is pointed-to
				auto reg = regs.next();
				assert(arg_value->type->size % 8 == 0);
				emit_move_reg_to_mreg_offset(reg, RBP, arg_value->local.offset);
			}
		}

		comment("end prologue");
	}

	void emit_epilogue()
	{
		comment("epilogue");

		overwrite_long(frame_size_addr, next_local_slot);

		if (return_value->type->size == 0) {
		} else if (return_value->type->size <= 8) {
			emit_move(return_value, 0, RAX);
		} else {
		}

		// addq $x, %rsp
		emit_byte(0x48);
		emit_byte(0x81);
		emit_byte(0xc4);
		emit_long(next_local_slot);

		// popq %rbx
		emit_byte(0x5b);

		// popq %rbp
		emit_byte(0x5d);

		// retq
		emit_byte(0xc3);
	}

	void emit_move_reg_to_reg(machine_register source, machine_register dest)
	{
		// REX.W (+ REX.B)
		bytes.push_back(REX | REX_W | (REX_R * (source >= 8)) | (REX_B * (dest >= 8)));
		// Opcode
		bytes.push_back(0x89);
		// Mod-Reg-R/M
		bytes.push_back(/* Mod */ 0xc0 | /* Reg */ ((source & 7) << 3) | /* R/M */ (dest & 7));
	}

	void emit_move_reg_to_mreg_offset(machine_register source, machine_register dest, unsigned int dest_offset)
	{
		// REX.W (+ REX.B)
		bytes.push_back(REX | REX_W | (REX_R * (source >= 8)) | (REX_B * (dest >= 8)));
		// Opcode
		bytes.push_back(0x89);
		// Mod-Reg-R/M
		bytes.push_back(/* Mod */ 0x80 | /* Reg */ ((source & 7) << 3) | /* R/M */ (dest & 7));
		// SIB
		if (dest == RSP)
			bytes.push_back(0x24);
		//bytes.push_back(/* Scale */ 0x00 | /* Index */ 0x20 | /* Base */ (0 & 7));
		emit_long(dest_offset);
	}

	void emit_move_mreg_offset_to_reg(machine_register source, unsigned int source_offset, machine_register dest)
	{
		// REX.W (+ REX.B)
		bytes.push_back(REX | REX_W | (REX_R * (dest >= 8)) | (REX_B * (source >= 8)));
		// Opcode: mov
		bytes.push_back(0x8b);
		// Mod-Reg-R/M
		bytes.push_back(/* Mod */ 0x80 | /* Reg */ ((dest & 7) << 3) | /* R/M */ (source & 7));
		// SIB
		if (source == RSP)
			bytes.push_back(0x24);
		//bytes.push_back(0x24);
		emit_long(source_offset);
	}

	void emit_move_reg_offset_to_reg(machine_register source, unsigned int source_offset, machine_register dest)
	{
		// REX.W (+ REX.B)
		bytes.push_back(REX | REX_W | (REX_R * (dest >= 8)) | (REX_B * (source >= 8)));
		// Opcode: lea
		bytes.push_back(0x8d);
		// Mod-Reg-R/M
		bytes.push_back(/* Mod */ 0x80 | /* Reg */ ((dest & 7) << 3) | /* R/M */ (source & 7));
		// SIB
		if (source == RSP)
			bytes.push_back(0x24);
		//bytes.push_back(0x24);
		emit_long(source_offset);
	}

	void emit_move_imm_to_reg(uint64_t source, machine_register dest)
	{
		// REX_W (+ REX.B)
		bytes.push_back(REX | REX_W | (REX_B * (dest >= 8)));
		// Opcode
		bytes.push_back(0xb8 | (dest & 7));
		emit_quad(source);
	}

	void emit_move_obj_to_reg(unsigned int object_id, machine_register dest)
	{
		// REX_W (+ REX.B)
		bytes.push_back(REX | REX_W | (REX_B * (dest >= 8)));
		// Opcode
		bytes.push_back(0xb8 | (dest & 7));
		emit_obj(object_id);
	}

	void emit_move(value_ptr source, unsigned int source_offset, machine_register dest)
	{
		switch (source->storage_type) {
		case VALUE_GLOBAL:
			// TODO
			assert(host);
			emit_move_imm_to_reg((uint64_t) source->global.host_address, RBX);
			emit_move_mreg_offset_to_reg(RBX, source_offset, dest);
			break;
		case VALUE_TARGET_GLOBAL:
			assert(!host);
			emit_move_obj_to_reg(source->target_global.object_id, RBX);
			emit_move_mreg_offset_to_reg(RBX, source_offset, dest);
			break;
		case VALUE_LOCAL:
			emit_move_mreg_offset_to_reg(RBP, source->local.offset + source_offset, dest);
			break;
		case VALUE_LOCAL_POINTER:
			emit_move_mreg_offset_to_reg(RBP, source->local.offset, RBX);
			emit_move_mreg_offset_to_reg(RBX, source_offset, dest);
			break;
		case VALUE_CONSTANT:
			emit_move_imm_to_reg(source->constant.u64 >> (8 * source_offset), dest);
			break;
		}
	}

	void emit_move(machine_register source, value_ptr dest, unsigned int dest_offset)
	{
		switch (dest->storage_type) {
		case VALUE_GLOBAL:
			// TODO
			assert(host);
			emit_move_imm_to_reg((uint64_t) dest->global.host_address, RBX);
			emit_move_reg_to_mreg_offset(source, RBX, dest_offset);
			break;
		case VALUE_TARGET_GLOBAL:
			assert(!host);
			emit_move_obj_to_reg(dest->target_global.object_id, RBX);
			emit_move_reg_to_mreg_offset(source, RBX, dest_offset);
			break;
		case VALUE_LOCAL:
			emit_move_reg_to_mreg_offset(source, RBP, dest->local.offset + dest_offset);
			break;
		case VALUE_LOCAL_POINTER:
			emit_move_mreg_offset_to_reg(RBP, dest->local.offset, RBX);
			emit_move_reg_to_mreg_offset(source, RBX, dest_offset);
			break;
		case VALUE_CONSTANT:
			// TODO
			assert(false);
			break;
		}
	}

	void emit_move(value_ptr source, value_ptr dest)
	{
		// TODO: check for compatible types?
		assert(source->type->size == dest->type->size);

		// Poor man's memcpy
		for (unsigned int i = 0; i < source->type->size; i += 8) {
			emit_move(source, i, RAX);
			emit_move(RAX, dest, i);
		}
	}

	void emit_move_address(machine_register source, value_ptr dest)
	{
		switch (dest->storage_type) {
		case VALUE_LOCAL_POINTER:
			emit_move_reg_to_mreg_offset(source, RBP, dest->local.offset);
			break;
		default:
			assert(false);
		}
	}

	void emit_move_address(value_ptr source, machine_register dest)
	{
		switch (source->storage_type) {
		case VALUE_GLOBAL:
			assert(host);
			emit_move_imm_to_reg((uint64_t) source->global.host_address, dest);
			break;
		case VALUE_TARGET_GLOBAL:
			assert(!host);
			emit_move_obj_to_reg(source->target_global.object_id, dest);
			break;
		case VALUE_LOCAL:
			emit_move_reg_offset_to_reg(RBP, source->local.offset, dest);
			break;
		case VALUE_LOCAL_POINTER:
			emit_move_mreg_offset_to_reg(RBP, source->local.offset, dest);
			break;
		default:
			assert(false);
		}
	}

	void emit_cmp_reg_reg(machine_register source1, machine_register source2)
	{
		// REX.W (+ REX.R / REX.B)
		bytes.push_back(REX | REX_W | (REX_R * (source2 >= 8) | (REX_B * (source1 >= 8))));
		// Opcode
		bytes.push_back(0x39);
		// Mod-Reg-R/M ?
		bytes.push_back(0xc0 | ((source2 & 7) << 3) | (source1 & 7));
	}

	void emit_compare(value_ptr lhs, value_ptr rhs)
	{
		assert(lhs->type->size == rhs->type->size);
		// TODO: handle other sizes?
		assert(lhs->type->size == 8);

		emit_move(lhs, 0, RAX);
		emit_move(rhs, 0, RBX);
		emit_cmp_reg_reg(RAX, RBX);
	}

	enum {
		// sete
		CMP_EQ = 0x94,
		// setne
		CMP_NEQ = 0x95,
		// setb
		CMP_LESS = 0x92,
		// setbe
		CMP_LESS_EQUAL = 0x96,
		// seta
		CMP_GREATER = 0x97,
		// setae
		CMP_GREATER_EQUAL= 0x93,
	};

	template<uint8_t opcode>
	void emit_eq(value_ptr source1, value_ptr source2, value_ptr dest)
	{
		emit_compare(source1, source2);

		// set[n]e %al
		emit_byte(0x0f);
		emit_byte(opcode);
		emit_byte(0xc0);

		// bools are size 8 for now, just to simplify things
		assert(dest->type->size == 8);

		// movzblq %al, %rax
		emit_byte(0x48);
		emit_byte(0x0f);
		emit_byte(0xb6);
		emit_byte(0xc0);

		emit_move(RAX, dest, 0);
	}

	void emit_label(label &lab)
	{
		lab.addr = bytes.size();
	}

	void emit_jump(label &lab)
	{
		emit_byte(0xe9);
		lab.relocations.push_back({ (unsigned int) bytes.size(), 4 });
		emit_long_placeholder();
	}

	void emit_jump_if_zero(label &lab)
	{
		emit_byte(0x0f);
		emit_byte(0x84);
		lab.relocations.push_back({ (unsigned int) bytes.size(), 4 });
		emit_long_placeholder();
	}

	void emit_jump_if_zero(value_ptr value, label &target)
	{
		emit_move(value, 0, RAX);

		// cmp $0x0, %rax
		emit_byte(0x48);
		emit_byte(0x83);
		emit_byte(0xf8);
		emit_byte(0x00);

		emit_jump_if_zero(target);
	}

	void emit_call(machine_register target)
	{
		// REX.B
		bytes.push_back(REX | (REX_B * (target >= 8)));
		// Opcode
		emit_byte(0xff);
		// Mod-Reg-R/M ?
		bytes.push_back(0xd0 | (target & 7));
	}

	void emit_call(value_ptr target)
	{
		switch (target->storage_type) {
		case VALUE_GLOBAL:
			// TODO: optimise
			assert(host);
			emit_move_imm_to_reg((uint64_t) target->global.host_address, RAX);
			emit_move_mreg_offset_to_reg(RAX, 0, RAX);
			emit_call(RAX);
			break;
		case VALUE_TARGET_GLOBAL:
			// TODO: optimise
			assert(!host);
			// Opcode
			emit_byte(0xe8);
			emit_obj_PC32(target->target_global.object_id, -4);
			break;
		case VALUE_LOCAL:
			// TODO: optimise
			emit_move_mreg_offset_to_reg(RBP, target->local.offset, RAX);
			emit_call(RAX);
			break;
		default:
			assert(false);
			break;
		}
	}

	void emit_call(value_ptr target, std::vector<value_ptr> args_values, value_ptr return_value)
	{
		// TODO: Try not to clobber the argument registers...
		args_allocator regs;

		assert(return_value);
		if (return_value->type->size == 0) {
		} else if (return_value->type->size <= 8) {
		} else {
			// TODO: handle other cases?
			assert(return_value->storage_type == VALUE_LOCAL);

			// Pass a pointer to our value
			emit_move_reg_offset_to_reg(RBP, return_value->local.offset, regs.next());
		}

		for (auto arg_value: args_values) {
			if (arg_value->type->size == 0) {
			} else if (arg_value->type->size <= 8) {
				emit_move(arg_value, 0, regs.next());
			} else {
				comment("move local to large arg");
				switch (arg_value->storage_type) {
				case VALUE_LOCAL:
					emit_move_reg_offset_to_reg(RBP, arg_value->local.offset, regs.next());
					break;
				case VALUE_LOCAL_POINTER:
					// TODO: Create a new temporary?
					emit_move_mreg_offset_to_reg(RBP, arg_value->local.offset, regs.next());
					break;
				default:
					// TODO
					assert(false);
					break;
				}
			}
		}

		emit_call(target);

		assert(return_value);
		if (return_value->type->size == 0) {
		} else if (return_value->type->size <= 8) {
			emit_move(RAX, return_value, 0);
		} else {
			// handled above
		}
	}

	void emit_add(value_ptr source1, value_ptr source2, value_ptr dest)
	{
		assert(source1->type->size == dest->type->size);
		assert(source2->type->size == dest->type->size);
		// TODO: handle other sizes?
		assert(dest->type->size == 8);

		comment("add");
		emit_move(source1, 0, RAX);
		emit_move(source2, 0, RBX);
		// hardcoded: addq %rbx, %rax
		emit_byte(0x48);
		emit_byte(0x01);
		emit_byte(0xd8);
		emit_move(RAX, dest, 0);
	}

	void emit_sub(value_ptr source1, value_ptr source2, value_ptr dest)
	{
		assert(source1->type->size == dest->type->size);
		assert(source2->type->size == dest->type->size);
		// TODO: handle other sizes?
		assert(dest->type->size == 8);

		comment("sub");
		emit_move(source1, 0, RAX);
		emit_move(source2, 0, RBX);
		// hardcoded: subq %rbx, %rax
		emit_byte(0x48);
		emit_byte(0x29);
		emit_byte(0xd8);
		emit_move(RAX, dest, 0);
	}

	void link_label(const label &l)
	{
		for (const jump_relocation &r: l.relocations)
			overwrite_long(r.addr, l.addr - (r.addr + r.offset));
	}
};

struct function_block {
	function_ptr f;

	function_block(const function_ptr &f, std::string name, std::string args = ""):
		f(f)
	{
		f->comment(format("$($) {", name, args));
		f->enter();
	}

	~function_block()
	{
		f->leave();
		f->comment("}");
	}
};

#define function_enter(f, args...) \
	function_block __function_enter(f, __FUNCTION__, ##args)

#endif
