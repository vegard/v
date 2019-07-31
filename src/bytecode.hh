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

#include <stdarg.h>

#include "function.hh"
#include "globals.hh"
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

#define _DEFINE_ENUM_NAME(name) name,
#define _DEFINE_ENUM_STR(name) #name,
#define DEFINE_ENUM(name) \
	enum name { \
		name(_DEFINE_ENUM_NAME) \
	}; \
	\
	const char *name##_names[] = { \
		name(_DEFINE_ENUM_STR) \
	}; \
	\
	const unsigned int nr_##name##s = sizeof(name##_names) / sizeof(*name##_names);

#define bytecode_opcode(X) \
	X(LOAD_CONSTANT) \
	X(LOAD_CONSTANT2) \
	X(LOAD_LOCAL) \
	X(LOAD_LOCAL2) \
	X(LOAD_LOCAL_ADDRESS) \
	X(LOAD_LOCAL2_ADDRESS) \
	X(LOAD_GLOBAL8) \
	X(LOAD_GLOBAL16) \
	X(LOAD_GLOBAL32) \
	X(LOAD_GLOBAL64) \
	X(LOAD_ARG) \
	X(LOAD_ARG_ADDRESS) \
	X(LOAD_RET) \
	\
	X(STORE_LOCAL) \
	X(STORE_LOCAL2) \
	X(STORE_GLOBAL8) \
	X(STORE_GLOBAL16) \
	X(STORE_GLOBAL32) \
	X(STORE_GLOBAL64) \
	X(STORE_ARG) \
	\
	X(ADD) \
	X(SUB) \
	X(MUL) \
	X(DIV) \
	\
	X(NOT) \
	X(AND) \
	X(OR) \
	X(XOR) \
	\
	X(EQ) \
	X(NEQ) \
	X(LT) \
	X(LTE) \
	X(GT) \
	X(GTE) \
	\
	X(JUMP) \
	X(JUMP_IF_ZERO) \
	X(CALL) \
	X(C_CALL) \
	X(RETURN)

DEFINE_ENUM(bytecode_opcode)

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

	// Ok, so this is how return values work. We have one "return_value"
	// (in the parent class) which is the actual return value that all
	// the users (fun/return/etc. macros) deal with directly.
	//
	// We also have this "local_return_value" which is what we actually
	// use to return a value to the caller (it is passed as the first
	// argument).
	//
	// When we return, we copy "return_value" into "local_return_value".
	value_ptr local_return_value;

	bytecode_function(context_ptr c, bool host, std::vector<value_type_ptr> args_types, value_type_ptr return_type):
		function(args_types, return_type),
		this_object(std::make_shared<object>()),
		bytes(this_object->bytes),
		indentation(0),
		max_nr_args(0),
		nr_locals(0)
	{
		// XXX: bytecode ABI..?

		for (auto arg_type: args_types) {
			assert(arg_type);

			// TODO: assert(arg_type->size > 0) ?
			if (arg_type->size == 0)
				args_values.push_back(builtin_value_void);
			else if (arg_type->size <= 8)
				args_values.push_back(alloc_local_value(c, arg_type));
			else
				args_values.push_back(alloc_local_pointer_value(c, arg_type));
		}

		assert(return_type);

		if (return_type->size == 0) {
			return_value = builtin_value_void;
			local_return_value = builtin_value_void;
		} else {
			return_value = alloc_local_value(c, return_type);
			local_return_value = alloc_local_pointer_value(c, return_type);
		}
	}

	value_ptr alloc_local_value(context_ptr c, value_type_ptr type)
	{
		auto result = std::make_shared<value>(c, VALUE_LOCAL, type);

		// TODO: alignment!
		unsigned int size = (type->size + 7) & ~7;
		unsigned int alignment = type->alignment;

		result->local.offset = nr_locals;
		nr_locals += size / 8;

		return result;
	}

	value_ptr alloc_local_pointer_value(context_ptr c, value_type_ptr type)
	{
		auto result = std::make_shared<value>(c, VALUE_LOCAL_POINTER, type);

		// TODO: alignment!
		unsigned int size = (sizeof(uint64_t) + 7) & ~7;
		unsigned int alignment = alignof(uint64_t);

		result->local.offset = nr_locals;
		nr_locals += size / 8;

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
		comment("emit_prologue() {");
		enter();

		unsigned int arg = 0;

		if (return_type->size != 0) {
			emit(LOAD_ARG);
			emit(arg++);
			emit_store_address(local_return_value);
		}

		for (auto arg_value: args_values) {
			if (arg_value->type->size != 0) {
				emit(LOAD_ARG);
				emit(arg++);

				if (arg_value->type->size <= 8)
					emit_store(arg_value);
				else
					emit_store_address(arg_value);
			}
		}

		leave();
		comment("}");
	}

	void emit_epilogue()
	{
		comment("emit_epilogue() {");
		enter();

		emit_move(return_value, local_return_value);
		emit(RETURN);

		leave();
		comment("}");
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
				unsigned int local_offset = value->local.offset + offset / 8;
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
			// TODO
			assert(false);
			break;
		case VALUE_CONSTANT:
			// TODO
			assert(offset == 0);

			{
				unsigned int index = constants.size();
				constants.push_back(value->constant.u64);

				emit(LOAD_CONSTANT);
				emit(index);
			}
			break;
		default:
			assert(false);
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

	void emit_load_address(value_ptr value, unsigned int offset)
	{
		switch (value->storage_type) {
		case VALUE_GLOBAL:
			{
				unsigned int index = constants.size();
				constants.push_back((uint64_t) value->global.host_address + offset);

				emit(LOAD_CONSTANT);
				emit(index);
			}
			break;
		case VALUE_LOCAL:
			{
				unsigned int local_offset = value->local.offset + offset / 8;
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
		case VALUE_LOCAL_POINTER:
			assert(offset % 8 == 0);

			{
				unsigned int local_offset = value->local.offset + offset / 8;
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
		default:
			assert(false);
		}
	}

	void emit_load_address(value_ptr value)
	{
		emit_load_address(value, 0);
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
				unsigned int local_offset = value->local.offset + offset / 8;
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
			assert(size == 8);

			{
				unsigned int local_offset = value->local.offset;
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

			// XXX: not so happy about this... pretty inefficient
			{
				unsigned int index = constants.size();
				constants.push_back((uint64_t) offset);

				emit(LOAD_CONSTANT);
				emit(index);
			}

			emit(ADD);

			emit_store_global(size);
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

	void emit_store_address(value_ptr value)
	{
		switch (value->storage_type) {
		case VALUE_LOCAL_POINTER:
			{
				unsigned int local_offset = value->local.offset;
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
		default:
			assert(false);
		}

	}

	void emit_move(value_ptr source, value_ptr dest)
	{
		// TODO: check for compatible types?
		assert(source->type->size == dest->type->size);

		// XXX: for now...
		assert(source->type->size % 8 == 0);

		// Poor man's memcpy
		// TODO: instead of doing this word by word, we can do a lot better
		// (e.g. loading constant + global addresses only once)
		for (unsigned int i = 0; i < source->type->size; i += 8) {
			emit_load_offset(source, i, 8);
			emit_store_offset(dest, i, 8);
		}
	}

	void emit_eq(uint8_t opcode, value_ptr source1, value_ptr source2, value_ptr dest)
	{
		emit_load(source1);
		emit_load(source2);

		switch (opcode) {
		case CMP_EQ:
			emit(EQ);
			break;
		case CMP_NEQ:
			emit(NEQ);
			break;
		case CMP_LESS:
			emit(LT);
			break;
		case CMP_LESS_EQUAL:
			emit(LTE);
			break;
		case CMP_GREATER:
			emit(GT);
			break;
		case CMP_GREATER_EQUAL:
			emit(GTE);
			break;
		default:
			assert(false);
		}

		emit_store(dest);
	}

	label_ptr new_label()
	{
		auto l = std::make_shared<bytecode_label>();
		l->constant_i = constants.size();
		constants.push_back(/* Dummy */ 0);
		return l;
	}

	void emit_label(label_ptr super_label)
	{
		auto l = std::dynamic_pointer_cast<bytecode_label>(super_label);
		constants[l->constant_i] = bytes.size();
	}

	void link_label(label_ptr super_label)
	{
		// This function intentionally left blank!
		//
		// (Instructions already refer to a constant pool index which
		// is updated in emit_label().)
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

	void emit_call(value_ptr fn, std::vector<value_ptr> args, value_ptr return_value)
	{
		comment("emit_call() {");
		enter();

		auto nr_args = args.size();
		if (nr_args > max_nr_args)
			max_nr_args = nr_args;

		// We pass a pointer to the return value as the first
		// argument
		if (return_value->type->size > 0) {
			emit_load_address(return_value);
			emit(STORE_ARG);
		}

		for (auto arg: args) {
			if (arg->type->size <= 8)
				emit_load(arg);
			else
				emit_load_address(arg);

			emit(STORE_ARG);
		}

		emit_load(fn);
		emit(CALL);

		leave();
		comment("}");
	}

	void emit_c_call(value_ptr fn, std::vector<value_ptr> args, value_ptr return_value)
	{
		auto nr_args = args.size();
		if (nr_args > max_nr_args)
			max_nr_args = nr_args;

		// We pass a pointer to the return value as the first
		// argument
		if (return_value->type->size > 0) {
			emit_load_address(return_value);
			emit(STORE_ARG);
		}

		for (auto arg: args) {
			if (arg->type->size <= 8)
				emit_load(arg);
			else
				emit_load_address(arg);

			emit(STORE_ARG);
		}

		emit_load(fn);
		emit(C_CALL);
	}

	void emit_add(value_ptr source1, value_ptr source2, value_ptr dest)
	{
		emit_load(source1);
		emit_load(source2);
		emit(ADD);
		emit_store(dest);
	}

	void emit_sub(value_ptr source1, value_ptr source2, value_ptr dest)
	{
		emit_load(source1);
		emit_load(source2);
		emit(SUB);
		emit_store(dest);
	}
};

struct jit_function {
	std::unique_ptr<uint64_t[]> constants;
	std::unique_ptr<uint8_t[]> bytecode;

	jit_function(std::shared_ptr<bytecode_function> f):
		constants(new uint64_t[f->constants.size()]),
		bytecode(new uint8_t[f->bytes.size()])
	{
		//printf("making jit function with bytecode at addr %p constants %p\n", &bytecode[0], &constants[0]);
		memcpy(&constants[0], &f->constants[0], sizeof(f->constants[0]) * f->constants.size());
		memcpy(&bytecode[0], &f->bytes[0], f->bytes.size());
	}
};

void disassemble_bytecode(uint64_t *constants, uint8_t *bytecode, unsigned int size, const std::vector<comment> &comments, unsigned int ip = 0)
{
        auto comments_it = comments.begin();
        auto comments_end = comments.end();

	unsigned int indentation = 0;

	unsigned int i = ip;
	if (i < size) do {
		while (comments_it != comments_end) {
			const auto &c = *comments_it;
			if (c.offset > i)
				break;

			indentation = c.indentation;
			printf("\e[33m%4s//%*.s %s\n", "", 2 * indentation, "", c.text.c_str());
			++comments_it;
		}

		if (i >= size)
			break;

		printf("\e[0m%4u: %*.s %s", i, 2 * indentation, "", bytecode_opcode_names[bytecode[i]]);

		uint8_t opcode = bytecode[i];
		if (opcode >= sizeof(bytecode_opcode_names) / sizeof(*bytecode_opcode_names)) {
			printf("(unrecognised opcode %u)\n", bytecode[i]);
		} else {
			switch (opcode) {
			case LOAD_CONSTANT:
				{
					unsigned int index = bytecode[++i];
					printf(" %lu (0x%lx)\n", constants[index], constants[index]);
				}
				break;

			case LOAD_LOCAL:
			case LOAD_LOCAL_ADDRESS:
			case LOAD_ARG:
			case STORE_LOCAL:
				{
					unsigned int index = bytecode[++i];
					printf(" %lu\n", index);
				}
				break;

			default:
				printf("\n");
				break;
			}
		}

		++i;
	} while (true);

	printf("\e[0m");
}

static void trace_bytecode(const char *fmt, ...)
{
	printf("\e[33m[trace-bytecode] ");

	va_list ap;
	va_start(ap, fmt);
	vprintf(fmt, ap);
	va_end(ap);

	printf("\e[0m");
}

template<bool debug>
void run_bytecode(uint64_t *constants, uint8_t *bytecode,
	uint64_t *args, unsigned int nr_args)
{
	unsigned int ip = 0;

	uint64_t operands[5];
	unsigned int nr_operands = 0;

	// XXX!!!!!
	const unsigned int nr_locals = 1000;
	const unsigned int max_nr_args = 1000;

	uint64_t locals[nr_locals];
	uint64_t new_args[max_nr_args];
	unsigned int nr_new_args = 0;

	unsigned int ret_index = 0;

	if (debug)
		trace_bytecode("running bytecode at addr %p with constants at addr %p\n", bytecode, constants);

	while (true) {
		if (debug) {
			trace_bytecode("");
			std::vector<comment> comments;
			disassemble_bytecode(constants, bytecode, ip + 1, comments, ip);
			fflush(stdout);
		}

		switch (bytecode[ip++]) {

			// Operands

		case LOAD_CONSTANT:
			{
				unsigned int index = bytecode[ip++];
				if (debug)
					trace_bytecode("constant %u = 0x%lx\n", index, constants[index]);
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
				if (debug)
					trace_bytecode("local %u = 0x%lx\n", index, locals[index]);
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
				operands[nr_operands - 1] = *(uint8_t *) operands[nr_operands - 1];
			}
			break;
		case LOAD_GLOBAL16:
			{
				assert((operands[nr_operands - 1] & 1) == 0);
				operands[nr_operands - 1] = *(uint16_t *) operands[nr_operands - 1];
			}
			break;
		case LOAD_GLOBAL32:
			{
				assert((operands[nr_operands - 1] & 3) == 0);
				operands[nr_operands - 1] = *(uint32_t *) operands[nr_operands - 1];
			}
			break;
		case LOAD_GLOBAL64:
			{
				assert((operands[nr_operands - 1] & 7) == 0);
				operands[nr_operands - 1] = *(uint64_t *) operands[nr_operands - 1];
				if (debug)
					trace_bytecode("op[%u] = 0x%llx\n", nr_operands - 1, operands[nr_operands - 1]);
			}
			break;

		case LOAD_ARG:
			{
				unsigned int index = bytecode[ip++];
				if (debug)
					trace_bytecode("arg %u = 0x%lx\n", index, args[index]);
				operands[nr_operands++] = args[index];
			}
			break;
		case LOAD_ARG_ADDRESS:
			// TODO
			assert(false);
			break;

		case STORE_LOCAL:
			{
				unsigned int index = bytecode[ip++];
				if (debug)
					trace_bytecode("local %u = 0x%lx\n", index, operands[nr_operands - 1]);
				locals[index] = operands[nr_operands - 1];
			}
			nr_operands -= 1;
			break;

		// XXX: rename to just STORE?
		case STORE_GLOBAL8:
			*(uint8_t *) operands[nr_operands - 1] = operands[nr_operands - 2];
			nr_operands -= 2;
			break;
		case STORE_GLOBAL16:
			*(uint16_t *) operands[nr_operands - 1] = operands[nr_operands - 2];
			nr_operands -= 2;
			break;
		case STORE_GLOBAL32:
			*(uint32_t *) operands[nr_operands - 1] = operands[nr_operands - 2];
			nr_operands -= 2;
			break;
		case STORE_GLOBAL64:
			assert(nr_operands >= 2);
			if (debug)
				trace_bytecode("*%p = 0x%lx\n", operands[nr_operands - 1], operands[nr_operands - 2]);
			*(uint64_t *) operands[nr_operands - 1] = operands[nr_operands - 2];
			nr_operands -= 2;
			break;

		case STORE_ARG:
			assert(nr_operands >= 1);
			if (debug)
				trace_bytecode("arg %u = 0x%lx\n", nr_new_args, operands[nr_operands - 1]);
			new_args[nr_new_args++] = operands[nr_operands - 1];
			nr_operands -= 1;
			break;

			// Arithmetic

		case ADD:
			if (debug) {
				trace_bytecode("%lx + %lx = %lx\n",
					operands[nr_operands - 2],
					operands[nr_operands - 1],
					operands[nr_operands - 2] + operands[nr_operands - 1]);
			}
			operands[nr_operands - 2] += operands[nr_operands - 1];
			nr_operands -= 1;
			break;
		case SUB:
			operands[nr_operands - 2] -= operands[nr_operands - 1];
			nr_operands -= 1;
			break;
		case MUL:
			operands[nr_operands - 2] *= operands[nr_operands - 1];
			nr_operands -= 1;
			break;
		case DIV:
			operands[nr_operands - 2] /= operands[nr_operands - 1];
			nr_operands -= 1;
			break;

			// Bitwise

		case NOT:
			operands[nr_operands - 2] = ~operands[nr_operands - 1];
			nr_operands -= 1;
			break;
		case AND:
			operands[nr_operands - 2] &= operands[nr_operands - 1];
			nr_operands -= 1;
			break;
		case OR:
			operands[nr_operands - 2] |= operands[nr_operands - 1];
			nr_operands -= 1;
			break;
		case XOR:
			operands[nr_operands - 2] ^= operands[nr_operands - 1];
			nr_operands -= 1;
			break;

			// Relational

		case EQ:
			operands[nr_operands - 2] = (operands[nr_operands - 2] == operands[nr_operands - 1]);
			nr_operands -= 1;
			break;
		case NEQ:
			operands[nr_operands - 2] = (operands[nr_operands - 2] != operands[nr_operands - 1]);
			nr_operands -= 1;
			break;
		case LT:
			operands[nr_operands - 2] = (operands[nr_operands - 2] < operands[nr_operands - 1]);
			nr_operands -= 1;
			break;
		case LTE:
			operands[nr_operands - 2] = (operands[nr_operands - 2] <= operands[nr_operands - 1]);
			nr_operands -= 1;
			break;
		case GT:
			operands[nr_operands - 2] = (operands[nr_operands - 2] > operands[nr_operands - 1]);
			nr_operands -= 1;
			break;
		case GTE:
			operands[nr_operands - 2] = (operands[nr_operands - 2] >= operands[nr_operands - 1]);
			nr_operands -= 1;
			break;

			// Control flow

		case JUMP:
			assert(nr_operands == 1);
			ip = operands[0];
			nr_operands = 0;
			break;
		case JUMP_IF_ZERO:
			assert(nr_operands == 2);
			if (!operands[1])
				ip = operands[0];
			nr_operands = 0;
			break;
		case CALL:
			assert(nr_operands == 1);
			{
				auto fn = (jit_function *) operands[0];
				run_bytecode<debug>(&fn->constants[0], &fn->bytecode[0], new_args, nr_new_args);
			}

			nr_operands = 0;
			nr_new_args = 0;
			break;
		case C_CALL:
			assert(nr_operands == 1);

			{
				auto fn = (void (*)(uint64_t *)) operands[0];
				fn(new_args);
			}

			nr_operands = 0;
			nr_new_args = 0;
			break;
		case RETURN:
			assert(nr_operands == 0);
			return;
		}
	}
}

void run_bytecode(uint64_t *constants, uint8_t *bytecode,
	uint64_t *args, unsigned int nr_args)
{
	// Do the check here and rely on the compiler to constant propagate
	// and inline so the fast path doesn't need to check this variable
	// more than once per eval().
	if (global_trace_bytecode)
		run_bytecode<true>(constants, bytecode, args, nr_args);
	else
		run_bytecode<false>(constants, bytecode, args, nr_args);
}

#endif
