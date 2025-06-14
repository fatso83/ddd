// $Id$
// Read variable values in string representation

// Copyright (C) 1995 Technische Universitaet Braunschweig, Germany.
// Written by Dorothea Luetkehaus <luetke@ips.cs.tu-bs.de>.
// 
// This file is part of DDD.
// 
// DDD is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public
// License as published by the Free Software Foundation; either
// version 3 of the License, or (at your option) any later version.
// 
// DDD is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
// See the GNU General Public License for more details.
// 
// You should have received a copy of the GNU General Public
// License along with DDD -- see the file COPYING.
// If not, see <http://www.gnu.org/licenses/>.
// 
// DDD is the data display debugger.
// For details, see the DDD World-Wide-Web page, 
// `http://www.gnu.org/software/ddd/',
// or send a mail to the DDD developers <ddd@gnu.org>.

//-----------------------------------------------------------------------------
// Read variable values in string representation
//-----------------------------------------------------------------------------

#ifndef _DDD_value_read_h
#define _DDD_value_read_h

#include "base/strclass.h"
#include "base/bool.h"
#include "DispValueT.h"

#include <iostream>

// Determine type of next element in VALUE
DispValueType determine_type (string& value);

// Check for STL types and set type
bool checkSTL(string& value, DispValueType &type);

// Read single token from VALUE
string read_token (string& value);

// Read simple element from VALUE
string read_simple_value (string& value, int depth, bool ignore_repeats);

// Read pointer from VALUE
string read_pointer_value (string& value, bool ignore_repeats);

// Read array; return false iff error
bool read_array_begin (string& value, string& addr);
bool read_array_next (string& value);
void read_array_end(string& value);

// Read `<repeats N times>'; return N (1 if no repeat)
int read_repeats(string& value);

// Read struct or class; return false iff error
bool read_struct_begin (string& value, string& addr);
bool read_struct_next (string& value);
void read_struct_end(string& value);

// Read GDB `members of CLASS:' prefix
bool read_members_prefix (string& value);

// Read in C++ vtable; return "" iff error
string read_vtable_entries (string& value);

// Read struct or class member name; return "" iff error
string read_member_name (string& value, bool picky = true);

// Return true if NAME has the form `<FOO>' or `<>'
bool is_BaseClass_name (const string& name);

// Cut off basename part (e.g. "foo.<Base>" becomes "foo")
void cut_BaseClass_name (string& full_name);

// True if VALUE starts with `END', `}', or likewise
bool is_ending(const string& value);

// True if VALUE is ending or starts with `,', `;', or likewise
bool is_delimited(const string& value);

#endif // _DDD_value_read_h
// DON'T ADD ANYTHING BEHIND THIS #endif
