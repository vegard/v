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

#ifndef V_NAMESPACE_HH
#define V_NAMESPACE_HH

#include "compile.hh"
#include "value.hh"

struct namespace_member: member {
	value_ptr val;

	namespace_member(value_ptr val):
		val(val)
	{
	}

	namespace_member(value_type_ptr type)
	{
		// XXX: this leaks
		val = new value(nullptr, VALUE_GLOBAL, builtin_type_type);
		val->global.host_address = (void *) new value_type_ptr(type);
	}

	value_ptr invoke(const compile_state &state, value_ptr v, ast_node_ptr node)
	{
		return val;
	}
};

#endif
