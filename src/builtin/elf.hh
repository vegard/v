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
#include "../x86_64.hh"

struct elf_data {
	value_ptr entry_point;
	std::map<std::string, value_ptr> exports;

	elf_data():
		entry_point(&builtin_value_void)
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

	value_ptr invoke(ast_node_ptr node)
	{
		if (!is_parent_of(this->s, state->scope))
			state->error(node, "'entry' used outside defining scope");

		auto entry_value = eval(node);
		if (entry_value->storage_type != VALUE_TARGET_GLOBAL)
			state->error(node, "entry point must be a compile-time target constant");

		// TODO: check here that entry_point is not void and callable with no args

		elf.entry_point = entry_value;
		return &builtin_value_void;
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

	value_ptr invoke(ast_node_ptr node)
	{
		if (node->type != AST_JUXTAPOSE)
			state->error(node, "expected juxtaposition");

		auto lhs = state->get_node(node->binop.lhs);
		if (lhs->type != AST_SYMBOL_NAME)
			state->error(node, "definition of non-symbol");

		auto symbol_name = state->get_symbol_name(lhs);

		// TODO: create new value?
		auto rhs = (use_scope(s), compile(state->get_node(node->binop.rhs)));
		s->define(state->function, state->source, node, symbol_name, rhs);

		if (do_export)
			elf.exports[symbol_name] = rhs;
		return &builtin_value_void;
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

	value_ptr invoke(ast_node_ptr node)
	{
		if (!is_parent_of(this->s, state->scope))
			state->error(node, "'export' used outside defining scope");

		// TODO: we really need to implement read vs. write scopes so
		// that when the user defines something it still becomes visible
		// in the parent scope
		auto new_scope = std::make_shared<scope>(state->scope);
		new_scope->define_builtin_macro("_define", std::make_shared<define_macro>(s, elf, true));

		return (use_scope(new_scope), eval(node));
	}
};

struct elf_writer {
	struct element {
		size_t offset;
		uint8_t *data;
		size_t size;
	};

	template<typename t>
	struct handle {
		t *data;
		Elf64_Off offset;
		Elf64_Addr addr;

		handle():
			data(nullptr)
		{
		}

		handle(t *data, Elf64_Off offset, Elf64_Addr addr):
			data(data),
			offset(offset),
			addr(addr)
		{
		}

		operator bool() const
		{
			return data;
		}

		t &operator*() const
		{
			return *data;
		}

		t *operator->() const
		{
			return data;
		}
	};

	Elf64_Off offset;
	Elf64_Addr addr;
	std::vector<element> elements;

	elf_writer(Elf64_Addr addr):
		offset(0),
		addr(addr)
	{
	}

	uint8_t *append(size_t alignment, size_t size)
	{
		align(alignment);

		uint8_t *data = new uint8_t[size];
		elements.push_back(element { offset, data, size });
		addr += size;
		offset += size;
		return data;
	}

	template<typename t>
	handle<t> append()
	{
		Elf64_Off orig_offset = offset;
		Elf64_Addr orig_addr = addr;

		uint8_t *data = append(alignof(t), sizeof(t));
		assert(((uintptr_t) data & (alignof(t) - 1)) == 0);
		return handle<t>((t *) data, orig_offset, orig_addr);
	}

	void align(size_t alignment)
	{
		// alignment must be a power of 2
		assert((alignment & (alignment - 1)) == 0);

		size_t remainder = offset & (alignment - 1);
		if (remainder == 0)
			return;

		size_t size = alignment - remainder;
		elements.push_back(element { offset, nullptr, size });
		addr += size;
		offset += size;
	}
};

// TODO: platform definitions
const unsigned int page_size = 4096;
const unsigned int exe_vaddr_base = 0x400000;
const char interp[] = "/lib64/ld-linux-x86-64.so.2";

static value_ptr builtin_macro_elf(ast_node_ptr node)
{
	auto elf_node = node;

	state->expect(node, node->type == AST_JUXTAPOSE,
		"expected 'elf [attributes...] filename:<expression> <expression>'");

	enum {
		STATIC,
		DYNAMIC,
	} linking_type = STATIC;

	enum {
		EXECUTABLE,
		LIBRARY,
		OBJECT,
	} file_type = EXECUTABLE;

	ast_node_ptr lhs_node = state->get_node(node->binop.lhs);
	if (lhs_node->type == AST_SQUARE_BRACKETS) {
		for (auto attribute_node: traverse<AST_COMMA>(state->source->tree, state->get_node(lhs_node->unop))) {
			// XXX: error handling
			assert(attribute_node->type == AST_SYMBOL_NAME);
			auto symbol_name = state->get_symbol_name(attribute_node);

			if (symbol_name == "static")
				linking_type = STATIC;
			else if (symbol_name == "dynamic")
				linking_type = DYNAMIC;
			else if (symbol_name == "exe")
				file_type = EXECUTABLE;
			else if (symbol_name == "lib")
				file_type = LIBRARY;
			else if (symbol_name == "obj")
				file_type = OBJECT;
			else
				state->error(attribute_node, "expected attribute");
		}

		node = state->get_node(node->binop.rhs);
	}

	state->expect(elf_node, node->type == AST_JUXTAPOSE,
		"expected 'elf [attributes...] filename:<expression> <expression>'");

	auto filename_node = state->get_node(node->binop.lhs);
	auto filename_value = eval(filename_node);
	if (filename_value->storage_type != VALUE_GLOBAL)
		state->error(filename_node, "output filename must be known at compile time");
	if (filename_value->type != builtin_type_str)
		state->error(filename_node, "output filename must be a string");
	auto filename = *(std::string *) filename_value->global.host_address;

	elf_data elf;
	auto objects = std::make_shared<std::vector<object_ptr>>();

	auto new_scope = std::make_shared<scope>(state->scope);
	new_scope->define_builtin_macro("_define", std::make_shared<define_macro>(new_scope, elf, false));
	new_scope->define_builtin_macro("entry", std::make_shared<entry_macro>(new_scope, elf));
	new_scope->define_builtin_macro("export", std::make_shared<export_macro>(new_scope, elf));

	use_objects _asdf(objects, new_scope);

	// we allocate the interpreter as an object because we need to get
	// both its address and its offset
	unsigned int interp_object_id;
	object_ptr interp_object;
	if (linking_type == DYNAMIC && file_type == EXECUTABLE) {
		interp_object = std::make_shared<object>(interp);
		interp_object_id = state->new_object(interp_object);
	}

	auto expr_node = state->get_node(node->binop.rhs);
	eval(expr_node);

	elf_writer w(file_type == EXECUTABLE ? exe_vaddr_base : 0);

	// ELF header

	auto ehdr = w.append<Elf64_Ehdr>();
	*ehdr = {};
	ehdr->e_ident[EI_MAG0] = ELFMAG0;
	ehdr->e_ident[EI_MAG1] = ELFMAG1;
	ehdr->e_ident[EI_MAG2] = ELFMAG2;
	ehdr->e_ident[EI_MAG3] = ELFMAG3;
	ehdr->e_ident[EI_CLASS] = ELFCLASS64;
	ehdr->e_ident[EI_DATA] = ELFDATA2LSB;
	ehdr->e_ident[EI_VERSION] = EV_CURRENT;
	ehdr->e_ident[EI_OSABI] = ELFOSABI_SYSV;
	ehdr->e_ident[EI_ABIVERSION] = 0;
	ehdr->e_ident[EI_PAD] = 0;

	switch (file_type) {
	case EXECUTABLE:
		ehdr->e_type = ET_EXEC;
		break;
	case LIBRARY:
		ehdr->e_type = ET_DYN;
		break;
	case OBJECT:
		ehdr->e_type = ET_REL;
		break;
	}

	ehdr->e_machine = EM_X86_64;
	ehdr->e_version = EV_CURRENT;
	ehdr->e_ehsize = sizeof(Elf64_Ehdr);
	ehdr->e_phentsize = sizeof(Elf64_Phdr);
	ehdr->e_shentsize = sizeof(Elf64_Shdr);

	// Dynamic entries (necessary for ld.so)

	// TODO: move after program headers?
	size_t dyn_offset = 0;
	size_t dyn_size = 0;
	elf_writer::handle<Elf64_Dyn> strtab_dyn;
	elf_writer::handle<Elf64_Dyn> symtab_dyn;
	if (linking_type == DYNAMIC) {
		w.align(alignof(Elf64_Dyn));
		dyn_offset = w.offset;

		strtab_dyn = w.append<Elf64_Dyn>();
		*strtab_dyn = {};
		strtab_dyn->d_tag = DT_STRTAB;
		strtab_dyn->d_un.d_ptr = /* TODO */ 0;
		dyn_size += sizeof(Elf64_Dyn);

		symtab_dyn = w.append<Elf64_Dyn>();
		*symtab_dyn = {};
		symtab_dyn->d_tag = DT_SYMTAB;
		symtab_dyn->d_un.d_ptr = /* TODO */ 0;
		dyn_size += sizeof(Elf64_Dyn);
	}

	// Program headers

	elf_writer::handle<Elf64_Phdr> phdr;
	elf_writer::handle<Elf64_Phdr> phdr_phdr;
	elf_writer::handle<Elf64_Phdr> interp_phdr;
	elf_writer::handle<Elf64_Phdr> elf_phdr;
	elf_writer::handle<Elf64_Phdr> dynamic_phdr;

	size_t phdr_offset;
	size_t phdr_offset_end;

	if (file_type == EXECUTABLE || file_type == LIBRARY) {
		w.align(alignof(Elf64_Phdr));

		phdr_offset = w.offset;
		ehdr->e_phoff = phdr_offset;

		// segment covering the ELF headers/segments
		if (linking_type == DYNAMIC && file_type == EXECUTABLE) {
			// libc rtld *requires* a PT_PHDR segment for dynamic objects
			phdr_phdr = w.append<Elf64_Phdr>();
			*phdr_phdr = {};
			phdr_phdr->p_type = PT_PHDR;
			phdr_phdr->p_offset = phdr_phdr.offset;
			phdr_phdr->p_vaddr = phdr_phdr.addr;
			phdr_phdr->p_paddr = phdr_phdr.addr;
			phdr_phdr->p_flags = PF_R | PF_X;
			phdr_phdr->p_align = 8;
			++ehdr->e_phnum;

			interp_phdr = w.append<Elf64_Phdr>();
			*interp_phdr = {};
			interp_phdr->p_type = PT_INTERP;
			interp_phdr->p_flags = PF_R;
			interp_phdr->p_align = 1;
			++ehdr->e_phnum;

			// LOAD covering the ELF header itself
			elf_phdr = w.append<Elf64_Phdr>();
			*elf_phdr = {};
			elf_phdr->p_type = PT_LOAD;
			elf_phdr->p_flags = PF_R | PF_X;
			elf_phdr->p_align = page_size;
			++ehdr->e_phnum;
		}

		// TODO: just one segment for everything for now
		phdr = w.append<Elf64_Phdr>();
		*phdr = {};
		phdr->p_type = PT_LOAD;
		phdr->p_flags = PF_X | PF_W | PF_R;
		phdr->p_align = page_size;
		++ehdr->e_phnum;

		if (linking_type == DYNAMIC) {
			dynamic_phdr = w.append<Elf64_Phdr>();
			*dynamic_phdr = {};
			dynamic_phdr->p_type = PT_DYNAMIC;
			dynamic_phdr->p_flags = PF_R | PF_W;
			dynamic_phdr->p_align = alignof(Elf64_Dyn);
			++ehdr->e_phnum;
		}

		// End of program headers!
		phdr_offset_end = w.offset;
	}

	if (phdr_phdr) {
		phdr_phdr->p_filesz = phdr_offset_end - phdr_offset;
		phdr_phdr->p_memsz = phdr_offset_end - phdr_offset;
	}

	// NOTE: the low order bits of this offset must match with the
	// virtual address. We pad with zeros to the nearest page boundary
	// to avoid loading parts of the ELF header for static executables.
	w.align(page_size);
	Elf64_Addr base_addr = w.addr;
	Elf64_Off base_offset = w.offset;
	if (phdr) {
		phdr->p_offset = base_offset;
		phdr->p_vaddr = base_addr;
		phdr->p_paddr = base_addr;
	}

	unsigned int entry_object_id;

	if (file_type == EXECUTABLE) {
		// TODO: is this check sufficient?
		if (elf.entry_point != &builtin_value_void) {
			assert(elf.entry_point->storage_type == VALUE_TARGET_GLOBAL);

			// We cannot set the entry point _directly_ at the function
			// given by the user, as the stack doesn't contain a return
			// value for us to return to. So we instead generate a new
			// function with a call to the entry point given by the user
			// and finish off with a syscall to terminate the process.

			// XXX: this is obviously highly Linux/x86-64-specific.

			auto new_f = std::make_shared<x86_64_function>(state->scope, state->context, false, std::vector<value_type_ptr>(), builtin_type_void);

			new_f->emit_call(elf.entry_point);
			new_f->emit_move_reg_to_reg(RAX, RDI);
			new_f->emit_move_imm_to_reg(/* SYS_exit_group */ 231, RAX);

			// syscall
			new_f->emit_byte(0x0f);
			new_f->emit_byte(0x05);

			entry_object_id = state->new_object(new_f->this_object);
		}
	}

	printf("%lu objects!\n", objects->size());

	struct elf_segment {
		size_t offset;
		size_t size;
		uint8_t *bytes;
		std::vector<unsigned int> objects;
	};

	std::vector<elf_segment> segments;
	segments.push_back(elf_segment());

	// allocate segments
	// TODO: split based on permissions (e.g. r, rw, rx); just put everything in one segment for now
	size_t nr_objects = objects->size();
	for (unsigned int i = 0; i < nr_objects; ++i) {
		const auto obj = (*objects)[i];

		segments[0].objects.push_back(i);
	}

	struct elf_object_info {
		Elf64_Off segment_offset;
		Elf64_Off offset;
		Elf64_Addr addr;
	};

	std::vector<elf_object_info> object_infos(nr_objects);

	size_t offset = 0;
	for (auto &segment: segments) {
		// align segment to page boundary
		offset = (offset + page_size - 1) & ~(page_size - 1);
		segment.offset = offset;

		for (const auto object_id: segment.objects) {
			const auto obj = (*objects)[object_id];

			// align object
			// TODO: store alignment in object...
			const size_t alignment = 16;
			offset = (offset + alignment - 1) & ~(alignment - 1);

			object_infos[object_id] = {
				.segment_offset = offset,
				.offset = base_offset + offset,
				.addr = base_addr + offset,
			};

			offset += obj->bytes.size();
		}

		segment.size = offset - segment.offset;

		// write data to segment
		uint8_t *bytes = w.append(/* XXX */ 16, segment.size);
		memset(bytes, 0, segment.size);
		for (const auto object_id: segment.objects) {
			const auto obj = (*objects)[object_id];
			const auto &info = object_infos[object_id];
			memcpy(bytes + info.segment_offset, obj->bytes.data(), obj->bytes.size());
		}

		segment.bytes = bytes;
	}

	// apply relocations
	for (const auto &segment: segments) {
		for (const auto object_id: segment.objects) {
			const auto obj = (*objects)[object_id];
			uint8_t *object_bytes = segment.bytes + object_infos[object_id].segment_offset;

			// apply relocations
			for (const auto &reloc: obj->relocations) {
				switch (reloc.type) {
				case R_X86_64_64:
					{
						uint64_t target = object_infos[reloc.object].addr;
						for (unsigned int i = 0; i < 8; ++i)
							object_bytes[reloc.offset + i] = target >> (8 * i);
					}
					break;
				case R_X86_64_PC32:
					{
						uint64_t S = object_infos[reloc.object].addr;
						uint64_t A = reloc.addend;
						uint64_t P = object_infos[object_id].addr + reloc.offset;

						uint64_t value = S + A - P;
						for (unsigned int i = 0; i < 4; ++i)
							object_bytes[reloc.offset + i] = value >> (8 * i);
					}
					break;
				default:
					assert(false);
				}
			}

			//disassemble(obj->bytes.data(), obj->bytes.size(), object_infos[object_id].addr, obj->comments);
		}
	}

	if (file_type == EXECUTABLE) {
		if (elf.entry_point != &builtin_value_void)
			ehdr->e_entry = object_infos[entry_object_id].addr;
	}

	if (interp_phdr) {
		interp_phdr->p_offset = object_infos[interp_object_id].offset;
		interp_phdr->p_vaddr = object_infos[interp_object_id].addr;
		interp_phdr->p_paddr = object_infos[interp_object_id].addr;
		interp_phdr->p_filesz = interp_object->bytes.size();
		interp_phdr->p_memsz = interp_object->bytes.size();
	}

	if (elf_phdr) {
		elf_phdr->p_offset = ehdr.offset;
		elf_phdr->p_vaddr = ehdr.addr;
		elf_phdr->p_paddr = ehdr.addr;
		elf_phdr->p_filesz = phdr_offset_end;
		elf_phdr->p_memsz = phdr_offset_end;
	}

	if (dynamic_phdr) {
		// String table

		dynamic_phdr->p_offset = dyn_offset;
		dynamic_phdr->p_vaddr = strtab_dyn.addr;
		dynamic_phdr->p_paddr = strtab_dyn.addr;
		dynamic_phdr->p_filesz = dyn_size;
		dynamic_phdr->p_memsz = dyn_size;
	}

	// TODO
	if (phdr) {
		phdr->p_filesz = segments[0].size;
		phdr->p_memsz = segments[0].size;
	}

	// TODO: error handling, temporaries, etc.
	int fd = open(filename.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0755);
	if (fd == -1)
		state->error(filename_node, "couldn't open '$' for writing: $", filename.c_str(), strerror(errno));

	for (auto x: w.elements) {
		if (x.data) {
			// TODO: proper error handling
			ssize_t len = write(fd, x.data, x.size);
			if (len == -1)
				error(EXIT_FAILURE, errno, "write()");
		} else {
			// Skip over padding
			// TODO: proper error handling
			off_t offset = lseek(fd, x.size, SEEK_CUR);
			if (offset == -1)
				error(EXIT_FAILURE, errno, "lseek()");
		}
	}

	close(fd);
	return &builtin_value_void;
}

#endif
