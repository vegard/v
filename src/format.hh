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

#ifndef V_FORMAT_HH
#define V_FORMAT_HH

#include <stdexcept>
#include <sstream>
#include <string>

void format(std::ostringstream &ss, const char *s)
{
	while (*s) {
		if (*s == '$')
			throw std::runtime_error("too few arguments provided to format");

		ss << *s++;
	}
}

template<typename t, typename... Args>
void format(std::ostringstream &ss, const char *s, t &value, Args... args)
{
	while (*s) {
		if (*s == '$') {
			ss << value;
			format(ss, ++s, args...);
			return;
		}

		ss << *s++;
	}

	throw std::runtime_error("too many arguments provided to format");
}

template<typename... Args>
std::string format(const char *s, Args... args)
{
	std::ostringstream ss;
	format(ss, s, args...);
	return ss.str();
}

#endif
