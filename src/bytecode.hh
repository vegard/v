//
//  V compiler
//  Copyright (C) 2019  Vegard Nossum <vegard.nossum@gmail.com>
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

#ifndef V_BYTECODE_HH
#define V_BYTECODE_HH

#include "function.hh"
#include "value.hh"

// Bytecode design:
//
// A bytecode function is an array of bytes.
// Bytecode instructions are variable length.
//
// Each function has a "constant pool" allowing access to 64-bit constants
// using (in most cases) an 8-bit index instead of the full/direct value.
//
// At run-time, each function also has a small set of "argument" registers
// which are used to transfer arguments in and out of function calls.
//
// At run-time, there exist a small number (probably <= 3) of operand
// registers. Instructions may create or consume operands. For example,
// the following C function:
//
// int add(int x, int y)
// {
//     return x + y;
// }
//
// could be translated to:
//
// LOAD_ARG // place 'x' in operands[0]
// LOAD_ARG // place 'y' in operands[1]
// ADD // place 'x + y' in operands[0]
// STORE_ARG // place 'x + y' in args[0]
// RETURN

enum bytecode_opcode {
	LOAD_CONSTANT,
	LOAD_CONSTANT2,
	LOAD_LOCAL,
	LOAD_LOCAL2,
	LOAD_LOCAL_ADDRESS,
	LOAD_LOCAL2_ADDRESS,
	LOAD_GLOBAL8,
	LOAD_GLOBAL16,
	LOAD_GLOBAL32,
	LOAD_GLOBAL64,
	LOAD_ARG,
	LOAD_ARG_ADDRESS,
	STORE_LOCAL,
	STORE_LOCAL2,
	STORE_GLOBAL8,
	STORE_GLOBAL16,
	STORE_GLOBAL32,
	STORE_GLOBAL64,
	STORE_ARG,

	ADD,
	SUB,
	MUL,
	DIV,

	NOT,
	AND,
	OR,
	XOR,

	EQ,
	NEQ,
	LT,
	LTE,
	GT,
	GTE,

	JUMP,
	JUMP_IF_ZERO,
	CALL,
	// no SYSCALL -- it's better if we have a uniform interface to the
	// host environment, in this case we can limit ourselves to calling
	// C functions (but using which ABI?)
	C_CALL,
	RETURN,
};

struct bytecode_label:
	label
{
	// constant pool index
	// This is all we need. When linking the label, we just update the
	// constant pool entry with the label's address and we're done.
	unsigned int constant_i;
};

struct bytecode_function:
	function
{
	std::vector<uint64_t> constants;

	// XXX: the double indirection is bad, we should collect bytes
	// ourselves directly and then move it into the object at the end
	object_ptr this_object;
	std::vector<uint8_t> &bytes;

	unsigned int indentation;

	unsigned int max_nr_args;
	unsigned int nr_locals;

	bytecode_function(context_ptr c, bool host, std::vector<value_type_ptr> args_types, value_type_ptr return_type):
		function(args_types, return_type),
		this_object(std::make_shared<object>()),
		bytes(this_object->bytes),
		indentation(0),
		max_nr_args(0),
		nr_locals(0)
	{
	}

	value_ptr alloc_local_value(context_ptr c, value_type_ptr type)
	{
		auto result = std::make_shared<value>(c, VALUE_LOCAL, type);

		// TODO: we could try to rearrange/pack values to avoid wasting stack space
		// TODO
		unsigned int offset = (nr_locals + type->size + type->alignment - 1) & ~(type->alignment - 1);
		assert(offset > 0);
		result->local.offset = offset;
		nr_locals += type->size;

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

	void emit(uint8_t v)
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

	void emit_prologue()
	{
	}

	void emit_epilogue()
	{
		emit(RETURN);
	}

	void emit_load_global(unsigned int size)
	{
		switch (size) {
		case 1:
			emit(LOAD_GLOBAL8);
			break;
		case 2:
			emit(LOAD_GLOBAL16);
			break;
		case 4:
			emit(LOAD_GLOBAL32);
			break;
		case 8:
			emit(LOAD_GLOBAL64);
			break;
		default:
			assert(false);
		}
	}

	void emit_load_offset(value_ptr value, unsigned int offset, unsigned int size)
	{
		switch (value->storage_type) {
		case VALUE_GLOBAL:
			{
				unsigned int index = constants.size();
				constants.push_back((uint64_t) value->global.host_address + offset);

				emit(LOAD_CONSTANT);
				emit(index);
				emit_load_global(size);
			}
			break;
		case VALUE_TARGET_GLOBAL:
			assert(false);
			break;
		case VALUE_LOCAL:
			assert(offset % 8 == 0);

			{
				unsigned int local_offset = value->local.offset + offset;
				if (local_offset < 256) {
					emit(LOAD_LOCAL);
					emit(local_offset);
				} else if (local_offset < 65536) {
					emit(LOAD_LOCAL2);
					emit(local_offset);
					emit(local_offset >> 8);
				} else {
					assert(false);
				}
			}
			break;
		case VALUE_LOCAL_POINTER:
			assert(offset % 8 == 0);

			{
				unsigned int local_offset = value->local.offset + offset;
				if (local_offset < 256) {
					emit(LOAD_LOCAL_ADDRESS);
					emit(local_offset);
				} else if (local_offset < 65536) {
					emit(LOAD_LOCAL2_ADDRESS);
					emit(local_offset);
					emit(local_offset >> 8);
				} else {
					assert(false);
				}
			}
			break;
		case VALUE_CONSTANT:
			// TODO
			assert(offset == 0);

			unsigned int index = constants.size();
			constants.push_back(value->constant.u64);

			emit(LOAD_CONSTANT);
			emit(index);
			break;
		}
	}

	void emit_load(value_ptr value)
	{
		assert(value->type->size <= 8);

		emit_load_offset(value, 0, value->type->size);
	}

	void emit_load(label_ptr super_label)
	{
		auto l = std::dynamic_pointer_cast<bytecode_label>(super_label);
		emit(LOAD_CONSTANT);
		emit(l->constant_i);
	}

	void emit_store_global(unsigned int size)
	{
		switch (size) {
		case 1:
			emit(STORE_GLOBAL8);
			break;
		case 2:
			emit(STORE_GLOBAL16);
			break;
		case 4:
			emit(STORE_GLOBAL32);
			break;
		case 8:
			emit(STORE_GLOBAL64);
			break;
		default:
			assert(false);
		}
	}

	void emit_store_offset(value_ptr value, unsigned int offset, unsigned int size)
	{
		switch (value->storage_type) {
		case VALUE_GLOBAL:
			{
				unsigned int index = constants.size();
				constants.push_back((uint64_t) value->global.host_address + offset);

				emit(LOAD_CONSTANT);
				emit(index);
				emit_store_global(size);
			}
			break;
		case VALUE_TARGET_GLOBAL:
			assert(false);
			break;
		case VALUE_LOCAL:
			assert(offset % 8 == 0);

			{
				unsigned int local_offset = value->local.offset + offset;
				if (local_offset < 256) {
					emit(STORE_LOCAL);
					emit(local_offset);
				} else if (local_offset < 65536) {
					emit(STORE_LOCAL2);
					emit(local_offset);
					emit(local_offset >> 8);
				} else {
					assert(false);
				}
			}
			break;
		case VALUE_LOCAL_POINTER:
			assert(offset % 8 == 0);
			assert(false);
			break;
		case VALUE_CONSTANT:
			assert(false);
			break;
		}
	}

	void emit_store(value_ptr value)
	{
		assert(value->type->size <= 8);

		emit_store_offset(value, 0, value->type->size);
	}

	void emit_move(value_ptr source, value_ptr dest)
	{
		// TODO: check for compatible types?
		assert(source->type->size == dest->type->size);

		// XXX: for now...
		assert(source->type->size % 8 == 0);

		// Poor man's memcpy
		for (unsigned int i = 0; i < source->type->size; i += 8) {
			emit_load_offset(source, i, 8);
			emit_store_offset(dest, i, 8);
		}
	}

	void emit_eq(value_ptr lhs, value_ptr rhs, value_ptr dest)
	{
		emit_load(lhs);
		emit_load(rhs);
		emit(EQ);
		emit_store(dest);
	}

	label_ptr new_label()
	{
		return std::make_shared<bytecode_label>();
	}

	void emit_label(label_ptr super_label)
	{
		auto l = std::dynamic_pointer_cast<bytecode_label>(super_label);
		l->constant_i = constants.size();
		constants.push_back(/* Dummy */ 0);
	}

	void link_label(label_ptr super_label)
	{
		auto l = std::dynamic_pointer_cast<bytecode_label>(super_label);
		constants[l->constant_i] = bytes.size();
	}

	void emit_jump(label_ptr super_target)
	{
		emit_load(super_target);
		emit(JUMP);
	}

	void emit_jump_if_zero(value_ptr value, label_ptr super_target)
	{
		emit_load(super_target);
		emit_load(value);
		emit(JUMP_IF_ZERO);
	}

	void emit_call(value_ptr fn, std::vector<value_ptr> args)
	{
		auto nr_args = args.size();
		if (nr_args > max_nr_args)
			max_nr_args = nr_args;

		for (auto arg: args) {
			emit_load(arg);
			emit(STORE_ARG);
		}

		emit_load(fn);
		emit(CALL);
	}

	void emit_c_call(value_ptr fn, std::vector<value_ptr> args)
	{
		// TODO
		emit(C_CALL);
	}

	void emit_add(value_ptr source1, value_ptr source2, value_ptr dest)
	{
		emit_load(source1);
		emit_load(source2);
		emit(ADD);
		emit_store(dest);
	}
};

void disassemble_bytecode(uint64_t *constants, uint8_t *bytecode, unsigned int size, const std::vector<comment> &comments)
{
        auto comments_it = comments.begin();
        auto comments_end = comments.end();

	unsigned int indentation = 0;

	unsigned int i = 0;
	while (i < size) {
		while (comments_it != comments_end) {
			const auto &c = *comments_it;
			if (c.offset > i)
				break;

			indentation = c.indentation;
			printf("\e[33m%4s//%*.s %s\n", "", 2 * indentation, "", c.text.c_str());
			++comments_it;
		}

		printf("\e[0m%3u: %*.s", i, 2 * indentation, "");

		switch (bytecode[i]) {
		case LOAD_CONSTANT:
			{
				unsigned int index = bytecode[++i];
				printf("LOAD_CONSTANT %lu (0x%lx)\n", constants[index], constants[index]);
			}
			break;
		case LOAD_LOCAL:
			{
				unsigned int index = bytecode[++i];
				printf("LOAD_LOCAL %lu\n", index);
			}
			break;
		case LOAD_GLOBAL32:
			printf("LOAD_GLOBAL32\n");
			break;
		case LOAD_GLOBAL64:
			printf("LOAD_GLOBAL64\n");
			break;
		case LOAD_ARG:
			{
				unsigned int index = bytecode[++i];
				printf("LOAD_ARG %lu\n", index);
			}
			break;
		case STORE_LOCAL:
			{
				unsigned int index = bytecode[++i];
				printf("STORE_LOCAL %lu\n", index);
			}
			break;
		case STORE_GLOBAL8:
			printf("STORE_GLOBAL8\n");
			break;
		case STORE_GLOBAL16:
			printf("STORE_GLOBAL16\n");
			break;
		case STORE_GLOBAL32:
			printf("STORE_GLOBAL32\n");
			break;
		case STORE_GLOBAL64:
			printf("STORE_GLOBAL64\n");
			break;
		case ADD:
			printf("ADD\n");
			break;
		case RETURN:
			printf("RETURN\n");
			break;
		default:
			printf("(unrecognised opcode %u)\n", bytecode[i]);
			break;
		}

		++i;
	}
}

void run(uint64_t *constants, uint8_t *bytecode, uint64_t *args, unsigned int nr_args)
{
	unsigned int ip = 0;

	uint64_t operands[3];
	unsigned int nr_operands = 0;

	// XXX!!!!!
	const unsigned int nr_locals = 1000;
	const unsigned int max_nr_args = 1000;

	uint64_t locals[nr_locals];
	uint64_t new_args[max_nr_args];
	unsigned int nr_new_args = 0;

	while (true) {
		switch (bytecode[ip++]) {

			// Operands

		case LOAD_CONSTANT:
			{
				unsigned int index = bytecode[ip++];
				operands[nr_operands++] = constants[index];
			}
			break;

		case LOAD_CONSTANT2:
			{
				unsigned int index = bytecode[ip++];
				index |= bytecode[ip++] << 8;
				operands[nr_operands++] = constants[index];
			}
			break;

		case LOAD_LOCAL:
			{
				unsigned int index = bytecode[ip++];
				operands[nr_operands++] = locals[index];
			}
			break;
		case LOAD_LOCAL2:
			{
				unsigned int index = bytecode[ip++];
				index |= bytecode[ip++] << 8;
				operands[nr_operands++] = locals[index];
			}
			break;

		case LOAD_LOCAL_ADDRESS:
			{
				unsigned int index = bytecode[ip++];
				operands[nr_operands++] = (uint64_t) &locals[index];
			}
			break;
		case LOAD_LOCAL2_ADDRESS:
			{
				unsigned int index = bytecode[ip++];
				index |= bytecode[ip++] << 8;
				operands[nr_operands++] = (uint64_t) &locals[index];
			}
			break;

		case LOAD_GLOBAL8:
			{
				operands[0] = *(uint8_t *) operands[0];
			}
			break;
		case LOAD_GLOBAL16:
			{
				assert((operands[0] & 1) == 0);
				operands[0] = *(uint16_t *) operands[0];
			}
			break;
		case LOAD_GLOBAL32:
			{
				assert((operands[0] & 3) == 0);
				operands[0] = *(uint32_t *) operands[0];
			}
			break;
		case LOAD_GLOBAL64:
			{
				assert((operands[0] & 7) == 0);
				operands[0] = *(uint64_t *) operands[0];
			}
			break;

		case LOAD_ARG:
			// TODO
			break;
		case LOAD_ARG_ADDRESS:
			// TODO
			break;

		case STORE_LOCAL:
			{
				unsigned int index = bytecode[ip++];
				locals[index] = operands[0];
			}
			break;

		case STORE_GLOBAL8:
			*(uint8_t *) operands[1] = operands[0];
			nr_operands = 0;
			break;
		case STORE_GLOBAL16:
			*(uint16_t *) operands[1] = operands[0];
			nr_operands = 0;
			break;
		case STORE_GLOBAL32:
			*(uint32_t *) operands[1] = operands[0];
			nr_operands = 0;
			break;
		case STORE_GLOBAL64:
			*(uint64_t *) operands[1] = operands[0];
			nr_operands = 0;
			break;

		case STORE_ARG:
			new_args[++nr_new_args] = operands[0];
			break;

			// Arithmetic

		case ADD:
			operands[0] += operands[1];
			nr_operands = 0;
			break;
		case SUB:
			operands[0] -= operands[1];
			nr_operands = 0;
			break;
		case MUL:
			operands[0] *= operands[1];
			nr_operands = 0;
			break;
		case DIV:
			operands[0] /= operands[1];
			nr_operands = 0;
			break;

			// Bitwise

		case NOT:
			operands[0] = ~operands[1];
			nr_operands = 0;
			break;
		case AND:
			operands[0] &= operands[1];
			nr_operands = 0;
			break;
		case OR:
			operands[0] |=  operands[1];
			nr_operands = 0;
			break;
		case XOR:
			operands[0] ^= operands[1];
			nr_operands = 0;
			break;

			// Relational

		case EQ:
			operands[0] = (operands[0] == operands[1]);
			nr_operands = 0;
			break;
		case NEQ:
			operands[0] = (operands[0] != operands[1]);
			nr_operands = 0;
			break;
		case LT:
			operands[0] = (operands[0] < operands[1]);
			nr_operands = 0;
			break;
		case LTE:
			operands[0] = (operands[0] <= operands[1]);
			nr_operands = 0;
			break;
		case GT:
			operands[0] = (operands[0] > operands[1]);
			nr_operands = 0;
			break;
		case GTE:
			operands[0] = (operands[0] >= operands[1]);
			nr_operands = 0;
			break;

			// Control flow

		case JUMP:
			ip = operands[0];
			nr_operands = 0;
			break;
		case JUMP_IF_ZERO:
			if (!operands[1])
				ip = operands[0];
			nr_operands = 0;
			break;
		case CALL:
			// XXX
			//((bytecode_function *) operands[0])->run(new_args, nr_new_args);
			nr_operands = 0;
			nr_new_args = 0;
			break;
		case C_CALL:
			// TODO
			if (nr_operands == 0)
				// XXX
				//((uint64_t (*)()) operands[0])->run();
				;
			else
				assert(false);

			nr_operands = 0;
			nr_new_args = 0;
			break;
		case RETURN:
			return;
		}
	}
}

#endif
