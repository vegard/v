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

#ifndef V_LINE_NUMBER_INFO_HH
#define V_LINE_NUMBER_INFO_HH

#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <algorithm>
#include <sstream>

struct line_number_info {
	std::map<unsigned int, unsigned int> byte_offset_to_line_number_map;

	line_number_info(const char *buf, size_t len)
	{
		unsigned int current_line = 1;

		for (unsigned int i = 0; i < len; ++i) {
			byte_offset_to_line_number_map.insert(std::make_pair(i, current_line++));

			while (i < len && buf[i] != '\n')
				++i;
		}

		byte_offset_to_line_number_map.insert(std::make_pair(len, current_line));
	}

	struct lookup_result {
		unsigned int line_start;
		unsigned int line_length;

		unsigned int line;
		unsigned int column;
	};

	struct lookup_result lookup(unsigned int byte_offset) const
	{
		auto it = byte_offset_to_line_number_map.upper_bound(byte_offset);
		if (it == byte_offset_to_line_number_map.begin())
			return lookup_result{0, 0, 0, 0};

		auto it2 = it;
		--it;

		assert(byte_offset >= it->first);
		return lookup_result{
			it->first,
			it2->first - it->first,
			it->second,
			byte_offset - it->first,
		};
	}
};

#endif
