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

#ifndef V_OBJECT_HH
#define V_OBJECT_HH

// An object is a sequence of bytes that is produced during compilation and
// which may eventually be part of the output of the compiler. It is similar
// to a variable or function in C as it appears in the object file.
//
// Linking is done in a generalised way. Instead of symbols, every single
// constant that is referenced by the program (including compiled functions
// as well as strings and integer constants) is an object that has a unique
// id number. Every object keeps itself track of references that it has to
// other objects (these are the relocations). We use id numbers instead of
// direct (reference counted) pointers to avoid any problems with cycles in,
// say, self-referential data.

struct relocation {
	// Where to apply it
	unsigned int offset;

	// What to point to
	unsigned int object;

	relocation(unsigned int offset, unsigned int object):
		offset(offset),
		object(object)
	{
	}
};

struct object;
typedef std::shared_ptr<object> object_ptr;

// An "object" in memory that may end up getting output
struct object {
	std::vector<uint8_t> bytes;

	// TODO: should this be actual full-fledged relocations or just
	// references? See http://www.ucw.cz/~hubicka/papers/abi/node19.html
	std::vector<relocation> relocations;

	std::map<size_t, std::vector<std::pair<unsigned int, std::string>>> comments;

	object()
	{
	}

	template<typename t>
	explicit object(const t &value):
		bytes(sizeof(t))
	{
		// TODO: is there a better C++ way to do this?
		memcpy(&bytes[0], &value, sizeof(t));
	}
};

#endif
