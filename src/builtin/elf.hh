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

#ifndef V_BUILTIN_ELF_HH
#define V_BUILTIN_ELF_HH

extern "C" {
#include <elf.h>
}

#include <array>
#include <set>
#include <vector>

#include "../ast.hh"
#include "../compile.hh"
#include "../function.hh"
#include "../scope.hh"
#include "../value.hh"
#include "../builtin/str.hh"

struct elf_data {
	value_ptr entry_point;
	std::map<std::string, value_ptr> exports;

	elf_data():
		entry_point(builtin_value_void)
	{
	}
};

struct entry_macro: macro {
	scope_ptr s;
	elf_data &elf;

	entry_macro(scope_ptr s, elf_data &elf):
		s(s),
		elf(elf)
	{
	}

	value_ptr invoke(const compile_state &state, ast_node_ptr node)
	{
		if (!is_parent_of(this->s, state.scope))
			state.error(node, "'entry' used outside defining scope");

		auto entry_value = eval(state, node);
		if (entry_value->storage_type != VALUE_TARGET_GLOBAL)
			state.error(node, "entry point must be a compile-time target constant");

		// TODO: check here that entry_point is not void and callable with no args

		elf.entry_point = entry_value;
		return builtin_value_void;
	}
};

struct define_macro: macro {
	scope_ptr s;
	elf_data &elf;
	bool do_export;

	define_macro(scope_ptr s, elf_data &elf, bool do_export):
		s(s),
		elf(elf),
		do_export(do_export)
	{
	}

	value_ptr invoke(const compile_state &state, ast_node_ptr node)
	{
		if (node->type != AST_JUXTAPOSE)
			state.error(node, "expected juxtaposition");

		auto lhs = state.get_node(node->binop.lhs);
		if (lhs->type != AST_SYMBOL_NAME)
			state.error(node, "definition of non-symbol");

		auto symbol_name = state.get_symbol_name(lhs);

		// TODO: create new value?
		auto rhs = compile(state.set_scope(s), state.get_node(node->binop.rhs));
		s->define(state.function, node, symbol_name, rhs);

		if (do_export)
			elf.exports[symbol_name] = rhs;
		return builtin_value_void;
	}
};

struct export_macro: macro {
	scope_ptr s;
	elf_data &elf;

	export_macro(scope_ptr s, elf_data &elf):
		s(s),
		elf(elf)
	{
	}

	value_ptr invoke(const compile_state &state, ast_node_ptr node)
	{
		if (!is_parent_of(this->s, state.scope))
			state.error(node, "'export' used outside defining scope");

		// TODO: we really need to implement read vs. write scopes so
		// that when the user defines something it still becomes visible
		// in the parent scope
		auto new_scope = std::make_shared<scope>(state.scope);
		new_scope->define_builtin_macro("_define", std::make_shared<define_macro>(s, elf, true));

		return eval(state.set_scope(new_scope), node);
	}
};

struct elf_writer {
	struct element {
		size_t offset;
		uint8_t *data;
		size_t size;
	};

	size_t offset;
	std::vector<element> elements;

	elf_writer():
		offset(0)
	{
	}

	template<typename t>
	t &append()
	{
		// TODO: we allocate as uint8_t[] because we will free
		// everything using that as well. Hopefully the alignment
		// of t will agree with our current offset...
		size_t size = sizeof(t);
		uint8_t *data = new uint8_t[size];
		elements.push_back(element { offset, data, size });
		offset += size;
		return *(t *) data;
	}

	void align(size_t alignment)
	{
		size_t size = alignment - (offset & (alignment - 1));
		uint8_t *data = new uint8_t[size];
		memset(data, 0, size);
		elements.push_back(element { offset, data, size });
		offset += size;
	}
};

static value_ptr builtin_macro_elf(const compile_state &state, ast_node_ptr node)
{
	if (node->type != AST_JUXTAPOSE)
		state.error(node, "expected 'elf filename:<expression> <expression>'");

	auto filename_node = state.get_node(node->binop.lhs);
	auto filename_value = eval(state, filename_node);
	if (filename_value->storage_type != VALUE_GLOBAL)
		state.error(filename_node, "output filename must be known at compile time");
	if (filename_value->type != builtin_type_str)
		state.error(filename_node, "output filename must be a string");
	auto filename = *(std::string *) filename_value->global.host_address;

	elf_data elf;
	auto objects = std::make_shared<std::vector<object_ptr>>();

	auto new_scope = std::make_shared<scope>(state.scope);
	new_scope->define_builtin_macro("_define", std::make_shared<define_macro>(new_scope, elf, false));
	new_scope->define_builtin_macro("entry", std::make_shared<entry_macro>(new_scope, elf));
	new_scope->define_builtin_macro("export", std::make_shared<export_macro>(new_scope, elf));

	auto new_state = state.set_objects(objects).set_scope(new_scope);

	auto expr_node = state.get_node(node->binop.rhs);
	auto expr_value = eval(new_state, expr_node);

	elf_writer w;

	auto &ehdr = w.append<Elf64_Ehdr>();
	ehdr = {};
	ehdr.e_ident[EI_MAG0] = ELFMAG0;
	ehdr.e_ident[EI_MAG1] = ELFMAG1;
	ehdr.e_ident[EI_MAG2] = ELFMAG2;
	ehdr.e_ident[EI_MAG3] = ELFMAG3;
	ehdr.e_ident[EI_CLASS] = ELFCLASS64;
	ehdr.e_ident[EI_DATA] = ELFDATA2LSB;
	ehdr.e_ident[EI_VERSION] = EV_CURRENT;
	ehdr.e_ident[EI_OSABI] = ELFOSABI_SYSV;
	ehdr.e_ident[EI_ABIVERSION] = 0;
	ehdr.e_ident[EI_PAD] = 0;
	ehdr.e_type = ET_EXEC;
	ehdr.e_machine = EM_X86_64;
	ehdr.e_version = EV_CURRENT;
	ehdr.e_ehsize = sizeof(Elf64_Ehdr);
	ehdr.e_phentsize = sizeof(Elf64_Phdr);
	ehdr.e_shentsize = sizeof(Elf64_Shdr);

	// TODO: do we need a PT_PHDR Phdr?

	ehdr.e_phoff = w.offset;

	// TODO: just one segment for everything for now
	auto &phdr = w.append<Elf64_Phdr>();
	phdr = {};
	phdr.p_type = PT_LOAD;
	phdr.p_flags = PF_X | PF_W | PF_R;
	phdr.p_vaddr = 0x400000;
	phdr.p_paddr = phdr.p_vaddr;
	phdr.p_align = 1;
	++ehdr.e_phnum;

	// XXX: the low order bits of this offset must match with the
	// virtual address. We should pad with zero to the nearest
	// page boundary to avoid loading parts of the ELF header.
	w.align(4096);

	phdr.p_offset = w.offset;

	// TODO: traverse entry point + exports
	for (const auto it: elf.exports) {
		//printf("export: %s\n", it.first.c_str());
	}

	unsigned int entry_object_id;

	// TODO: is this check sufficient?
	if (elf.entry_point != builtin_value_void) {
		assert(elf.entry_point->storage_type == VALUE_TARGET_GLOBAL);

		// We cannot set the entry point _directly_ at the function
		// given by the user, as the stack doesn't contain a return
		// value for us to return to. So we instead generate a new
		// function with a call to the entry point given by the user
		// and finish off with a syscall to terminate the process.

		auto new_f = std::make_shared<function>(false);

		new_f->emit_call(elf.entry_point);
		new_f->emit_move_reg_to_reg(RAX, RDI);
		new_f->emit_move_imm_to_reg(/* SYS_exit_group */ 231, RAX);

		// syscall
		new_f->emit_byte(0x0f);
		new_f->emit_byte(0x05);

		entry_object_id = new_state.new_object(new_f->this_object);
	}

	printf("%lu objects!\n", objects->size());

	std::vector<size_t> offsets;

	size_t offset = 0;
	for (const auto obj: *objects) {
		printf(" - object size %lu\n", obj->bytes.size());
		//std::map<size_t, std::vector<std::pair<unsigned int, std::string>>> comments;
		//disassemble((const uint8_t *) obj->bytes, obj->size, 0, comments);
		offsets.push_back(offset);
		offset += obj->bytes.size();
	}

	if (elf.entry_point != builtin_value_void)
		ehdr.e_entry = phdr.p_vaddr + offsets[entry_object_id];

	phdr.p_filesz = offset;
	phdr.p_memsz = offset;

	// TODO: error handling, temporaries, etc.
	int fd = open(filename.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0755);
	if (fd == -1)
		state.error(filename_node, "couldn't open '$' for writing: $", filename.c_str(), strerror(errno));

	for (auto x: w.elements) {
		write(fd, x.data, x.size);
	}

	for (unsigned int object_id = 0; object_id < objects->size(); ++object_id) {
		const auto obj = (*objects)[object_id];

		// apply relocations
		for (const auto &reloc: obj->relocations) {
			switch (reloc.type) {
			case R_X86_64_64:
				{
					uint64_t target = phdr.p_vaddr + offsets[reloc.object];
					for (unsigned int i = 0; i < 8; ++i)
						obj->bytes[reloc.offset + i] = target >> (8 * i);
				}
				break;
			case R_X86_64_PC32:
				{
					uint64_t S = phdr.p_vaddr + offsets[reloc.object];
					uint64_t A = reloc.addend;
					uint64_t P = phdr.p_vaddr + offsets[object_id] + reloc.offset;

					uint64_t value = S + A - P;
					for (unsigned int i = 0; i < 4; ++i)
						obj->bytes[reloc.offset + i] = value >> (8 * i);
				}
				break;
			default:
				assert(false);
			}
		}

		disassemble(&obj->bytes[0], obj->bytes.size(), phdr.p_vaddr + offsets[object_id], obj->comments);

		write(fd, &obj->bytes[0], obj->bytes.size());
	}

	close(fd);
	return builtin_value_void;
}

#endif
