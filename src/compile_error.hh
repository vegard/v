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

#ifndef V_COMPILE_ERROR_HH
#define V_COMPILE_ERROR_HH

#include "ast.hh"
#include "format.hh"

struct compile_error: std::runtime_error {
	unsigned int pos;
	unsigned int end;

	template<typename... Args>
	compile_error(const ast_node_ptr &node, const char *fmt, Args... args):
		std::runtime_error(format(fmt, args...)),
		pos(node->pos),
		end(node->end)
	{
		assert(end >= pos);
	}
};

#endif
