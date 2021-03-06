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

#ifndef V_GLOBALS_HH
#define V_GLOBALS_HH

// Global variables are not usually a great idea; here I make an exception,
// but they are all together and they should all have the behaviour of
// "set once at the start of the program" (based on command line arguments).

bool global_disassemble = false;

bool global_trace_eval = false;
bool global_trace_bytecode = false;

#endif
