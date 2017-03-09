//
//  The V programming language compiler
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

#include <cstdarg>
#include <cstdio>
#include <stdexcept>

// This is a wrapper around sprintf() that returns std::string
static std::string format(const char *format, ...)
{
	va_list ap;
	va_start(ap, format);
	char *str;
	if (vasprintf(&str, format, ap) == -1)
		throw std::runtime_error("vasprintf() failed");
	va_end(ap);

	std::string ret(str);
	free(str);
	return ret;
}

#endif
