#ifndef V_FUNCTION_HH
#define V_FUNCTION_HH

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

// There are many design decisions to make here:
//  - How generic do we make labels/relocations?
//  - Who is responsible for linking labels? (The caller, the label
//    destructor, the function, etc.?)
//  - If we already know a label's address, do we output it immediately
//    instead of linking it at the end?

struct relocation {
	unsigned int addr;
	unsigned int offset;
};

struct label {
	unsigned int addr;
	std::vector<relocation> relocations;
};

struct function;
typedef std::shared_ptr<function> function_ptr;

struct function {
	std::shared_ptr<value> return_value;
	std::vector<uint8_t> bytes;

	unsigned int next_local_slot;

	function():
		// slot 0 is the return address
		next_local_slot(8)
	{
	}

	value_ptr alloc_local_value(value_type *type)
	{
		auto result = std::make_shared<value>(VALUE_LOCAL, type);

		// TODO: we could try to rearrange/pack values to avoid wasting stack space
		unsigned int offset = (next_local_slot + type->alignment - 1) & ~(type->alignment - 1);
		result->local.offset = offset;
		next_local_slot = next_local_slot + type->size;

		return result;
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

	void emit_prologue()
	{
		// TODO
	}

	void emit_return()
	{
		// retq
		emit_byte(0xc3);
	}

	void emit_epilogue()
	{
		// TODO
		emit_return();
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
		// Opcode
		bytes.push_back(0x8b);
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
		bytes.push_back(REX | REX_W | (REX_R * (dest >= 8)));
		// Opcode
		bytes.push_back(0xb8 | (dest & 7));
		emit_quad(source);
	}

	void emit_move(value_ptr source, machine_register dest)
	{
		switch (source->metatype) {
		case VALUE_GLOBAL:
			// TODO
			assert(false);
			break;
		case VALUE_LOCAL:
			emit_move_mreg_offset_to_reg(RSP, source->local.offset, dest);
			break;
		case VALUE_CONSTANT:
			emit_move_imm_to_reg(source->constant.u64, dest);
			break;
		}
	}

	void emit_move(machine_register source, value_ptr dest)
	{
		switch (dest->metatype) {
		case VALUE_GLOBAL:
			// TODO
			assert(false);
			break;
		case VALUE_LOCAL:
			emit_move_reg_to_mreg_offset(source, RSP, dest->local.offset);
			break;
		case VALUE_CONSTANT:
			// TODO
			assert(false);
			break;
		}
	}

	void emit_move(value_ptr source, value_ptr dest)
	{
		// TODO: check that the source is big enough to fit in
		// a register; if not, use more registers?
		// TODO: check for compatible types?
		emit_move(source, RAX);
		emit_move(RAX, dest);
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
		emit_move(lhs, RAX);
		emit_move(rhs, RBX);
		emit_cmp_reg_reg(RAX, RBX);
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

	void link_label(const label &l)
	{
		for (const relocation &r: l.relocations)
			overwrite_long(r.addr, l.addr - (r.addr + r.offset));
	}
};

static value_type builtin_type_function = {alignof(function_ptr), sizeof(function_ptr)};

#endif
