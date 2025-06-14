// $Id$
// Read variable values in string representation

// Copyright (C) 1995-1998 Technische Universitaet Braunschweig, Germany.
// Copyright (C) 2003 Free Software Foundation, Inc.
// Written by Dorothea Luetkehaus <luetke@ips.cs.tu-bs.de>
// and Andreas Zeller <zeller@gnu.org>.
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

char value_read_rcsid[] =
    "$Id$";

#ifndef LOG_DETERMINE_TYPE
#define LOG_DETERMINE_TYPE 0
#endif

//-----------------------------------------------------------------------------
// Read variable values in string representation
//-----------------------------------------------------------------------------

#include "value-read.h"
#include <ctype.h>

#include "string-fun.h"
#include "base/assert.h"
#include "comm-manag.h"
#include "base/cook.h"
#include "ddd.h"
#include "base/isid.h"
#include "regexps.h"
#include "GDBAgent.h"
#include "PosBuffer.h"

static void read_token(const char *value, int& pos);
static void read_leading_junk(string& value);
static void read_leading_comment(string& value);

//-----------------------------------------------------------------------------

#if RUNTIME_REGEX
static regex rxindex("[[]-?[0-9][0-9]*].*");
static regex rxvtable(
    RXADDRESS " <[^ \n>]* v(irtual )?t(a)?bl(e)?>[^{},]*[{].*");
static regex rxdbx_baseclass("[ \t\n]*[a-zA-Z_$][^({\n=]*:[(]");
#endif

// Determine the type of VALUE.
static DispValueType _determine_type (string& valuefull)
{
    strip_leading_space(valuefull);

    // create a copy of the first part of value to speedup evaluation of regular expressions
    string value = valuefull.at(0, std::min((unsigned int)128, valuefull.length()));

    // DBX on DEC prepends `[N]' before array member N.
    if (value.matches(rxindex))
    {
	if (!gdb->has_array_braces())
	    return Array;	// In Ladebug, arrays don't have braces

	value = value.after(']');
    }

    // References.
    if (value.contains('(', 0))
    {
	int ref_index = value.index(')') + 1;
	while (ref_index < int(value.length()) && isspace(value[ref_index]))
	    ref_index++;
#if RUNTIME_REGEX
	static regex rxreference(
	    "@ *(0(0|x)[0-9a-f]+|[(]nil[)]) *:.*");
#endif
	if (value.matches(rxreference, ref_index))
	    return Reference;
    }

    // Texts.  (like `Local variables:\n' in JDB 'locals` output)
    int nl_index = value.index('\n');
    if (nl_index > 0 && value[nl_index - 1] == ':')
	return Text;

    // Vtables.
    if (value.matches(rxvtable))
	return Array;

    // Scalars.
    if (gdb->has_scalars())
    {
	// DBX uses this representation for out-of-range Pascal/Modula-2
	// enumerations.
	if (value.contains("(scalar = ", 0))
	    return Simple;
    }

    // AIX DBX prepends `BASECLASS:' before base class structs.
    if (value.contains(rxdbx_baseclass, 0))
	return Struct;

    // Structs.
    // XDB and JDB prepend the address before each struct.  JDB also
    // prepends the class name.
#if RUNTIME_REGEX
    static regex rxstruct_begin(
	"(" RXADDRESS ")? *"
	"([(]|[{]|record\n|RECORD\n|RECORD |OBJECT "
	"|struct|class|union|interface).*");
    static regex rxstruct_end("([)]|[}]|end\n|END\n|end;|END;)");
#endif

    if (value.matches(rxstruct_begin))
    {
	// Check for empty struct.
	const char *v = value.chars();
	value.consuming(true);
	string addr;
	bool ok = (read_struct_begin(value, addr) && 
		   value.contains(rxstruct_end, 0));
	value = v;
	if (ok)
	    return Struct;

	switch (gdb->program_language())
	{
	case LANGUAGE_PASCAL:
	case LANGUAGE_ADA:
	case LANGUAGE_CHILL:
	{
	    // Check for keywords introducing a struct (not
	    // necessarily followed by braces or likewise)
#if RUNTIME_REGEX
	    static regex rxstruct_keyword_begin(
		"(" RXADDRESS ")? *"
		"(record\n|RECORD\n|RECORD |OBJECT).*");
#endif
	    if (value.matches(rxstruct_keyword_begin))
		return Struct;

	    break;
	}

	default:
	    break;		// Don't care
	}

	// Check for leading braces.  DEC DBX uses `{' for arrays as
	// well as for structs; likewise, AIX DBX uses `(' for arrays
	// and structs.  To disambiguate between arrays and structs,
	// we check for some struct member -- that is, a ` = ' before
	// any other sub-struct or struct end.

	// Go behind leading `{' or '('
	int i = 0;
	while (i < int(value.length()) && value[i] != '(' && value[i] != '{')
	    i++;
	i++;

	// Check for further `{' or '('
	while (i < int(value.length()))
	{
	    switch(value[i++])
	    {
	    case ':':
		if (gdb->program_language() == LANGUAGE_JAVA)
		    return Struct;
		break;

	    case '=':
	    {
		// We have a member.
		//
		// DEC DBX issues arrays of strings as
		//
		//     {
		//         [0] 0x400188 = "Pioneering"
		//         [1] 0x10000dd0 = "women"
		//         [2] 0x10000dcc = "in"
		//     }
		//
		// Avoid interpreting these addresses as struct members.

		int j = i - 2;	// Character before `='
		while (j >= 0 && isspace(value[j]))
		    j--;
		int end_of_member_name = j + 1;
		while (j >= 0 && !isspace(value[j]))
		    j--;
		j++;
		string first_member = value.at(j, end_of_member_name - j);
		strip_space(first_member);
		if (first_member.contains(rxint, 0) || 
		    first_member.contains(rxaddress, 0))
		{
		    // The first `member' is a number or an address.
		    // This is not a struct; it's an array.
		    return Array;
		}

		// In all other cases, we have a struct.
		return Struct;
	    }

	    case '(':
	    case '{':
	    case ')':
	    case '}':
		goto next;		// Something else, possibly an array
	    }
	}
    next: ;
    }

    // Pointers.

    int pointer_index = 0;
    if (gdb->has_typed_pointers() &&
	(value.contains('(', 0) || value.contains('{', 0)))
    {
	// GDB and JDB prepend the exact pointer type enclosed in
	// `(...)'.  If the pointer type contains `(...)' itself (such
	// as in pointers to functions), GDB uses '{...}' instead (as
	// in `{int ()} 0x2908 <main>').

	read_token(value.chars(), pointer_index);
	while (pointer_index < int(value.length()) && 
	       isspace(value[pointer_index]))
	    pointer_index++;
    }

    int addr_len = rxaddress.match(value.chars(), value.length(), pointer_index);
    if (addr_len > 0)
    {
	// We have an address.
	// In JDB, an address may still be followed by a struct.
	int brace = pointer_index + addr_len;
	while (brace < int(value.length()) && isspace(value[brace]))
	    brace++;
	if (brace < int(value.length()) && value[brace] == '{')
	{
	    int nl = value.index('\n', pointer_index + addr_len);
	    if (nl >= 0 && brace < nl)
		return Struct;
	}

	// We have an ordinary pointer
	return Pointer;
    }

#if RUNTIME_REGEX
        static regex rxchain("[-a-zA-Z0-9::_>.`]+");
#endif
    // In GDB and JDB, Java pointers may be printed as `[TYPE]@ADDR'
    int at_index = value.index('@');
    if (at_index >= 0)
    {
	string id   = value.before(at_index);
	string addr = value.from(at_index);
	if (id.matches(rxchain) && addr.contains(rxaddress, 0))
	{
	    // We have a Java pointer
	    return Pointer;
	}
    }

    // In Perl, pointers are printed as `TYPE(ADDR)'
#if RUNTIME_REGEX
#define RXADDRESS "(0x[0-9a-fA-F]+|0[0-9a-fA-F]+[hH]|H'[0-9a-fA-F]+" \
                  "|00+|[(]nil[)]|NIL|null|@[0-9a-fA-F]+|16_[0-9a-f]+)"
#define STANDARD_IDENTFIER "([A-Za-z_$@%][A-Za-z0-9_$]*|[$]([^ \n\t\f]|[0-9]+))"
        static regex rxperlref("((" STANDARD_IDENTFIER "::)*" STANDARD_IDENTFIER "=)?" STANDARD_IDENTFIER "[(]" RXADDRESS "[)]");
#endif
    if (value.contains(rxperlref, 0))
	return Pointer;

    // Arrays.
    if (value.contains('{', 0))
	return Array;
    if (value.contains('[', 0))
	return Array;
    if (value.contains('(', 0))
	return Array;

    // Simple values.
    // Everything else failed - assume simple value.
    return Simple;
}

DispValueType determine_type(string& value)
{
#if LOG_DETERMINE_TYPE
    std::clog << quote(value);
#endif

    const char *v = value.chars();
    value.consuming(true);
    DispValueType type = _determine_type(value);
    value = v;

#if LOG_DETERMINE_TYPE
    std::clog << " has type " << type << "\n";
#endif

    return type;
}

bool checkSTL(string& value, DispValueType &type)
{
    strip_leading_space(value);
    
    int sep1 = value.index(' ');
    int sep2 = value.index('<');
    if (sep2>0 && sep2<sep1)
        sep1 = sep2;
    
    subString keyword = value.before(sep1);
    
    if (keyword.contains("std::")==false)
        return false;
        
    if (keyword.contains("::vector"))
    {
        type =  STLVector;
        return true;
    }
    
    if (keyword.contains("::list"))
    {
        type =  STLList;
        return true;
    }
    
    if (keyword.contains("::map") || keyword.contains("::multimap") || keyword.contains("::unordered_map") || keyword.contains("::unordered_multimap"))
    {
        type =  STLList;
        return true;
    }

    if (keyword.contains("::set") || keyword.contains("::multiset") || keyword.contains("::unordered_set") || keyword.contains("::unordered_multiset"))
    {
        type =  STLList;
        return true;
    }

    return false;
}

// Read tokens up to character DELIM
static bool read_up_to(const char *value, int& pos, char delim)
{
    if (value[pos] == '\0')
	return false;

    // Add opening delimiter
    pos++;

    // Read up to closing delimiter
    while (value[pos] && value[pos] != delim)
	read_token(value, pos);

    if (value[pos] == '\0')
	return false;

    // Add closing delimiter
    pos++;

    return true;
}

// Read tokens up to word DELIM
static bool read_up_to(const char *value, int& pos, const char* delim)
{
    if (value[pos] == '\0')
	return false;

    int len = strlen(delim);

    // Read up to closing delimiter
    while (value[pos] && strncmp(value + pos, delim, len))
	read_token(value, pos);

    if (value[pos] == '\0')
	return false;

    // Add closing delimiter
    pos += len;

    return true;
}

// Read a string enclosed in DELIM.  We do accept `\n' in strings.
static bool read_string(const char *value, int& pos, char delim)
{
    if (value[pos] == '\0')
	return false;

    // Add opening delimiter
    pos++;

    // Read up to closing delimiter
    while (value[pos] != '\0')
    {
	if (value[pos] == '\\' && value[pos + 1] != '\0')
	    pos += 2;
	else if (value[pos] == delim)
	    break;
	else
	    pos++;
    }

    if (value[pos] == '\0')
	return false;		// Could not find end

    // Add closing delimiter
    pos++;

    return true;
}


// Read a C-like token.
static void read_token(const char *value, int& pos)
{
    if (value[pos] == '\0')
	return;

    int start = pos;
    bool ok = true;

    string token;
    
    switch (value[pos])
    {
    case '\\':
	if (value[pos + 1] != '\0')
	    pos += 2;
	else
	    pos++;
	break;

    case '(':
	ok = read_up_to(value, pos, ')');
	break;

    case '[':
	ok = read_up_to(value, pos, ']');
	break;

    case '{':
	ok = read_up_to(value, pos, '}');
	break;

    case '\"':
    case '\'':
	ok = read_string(value, pos, value[pos]);
	break;

    case '<':
	if (gdb && gdb->has_print_r_option())
	{
	    // David Kirwan <David-Kirwan@vertel.com> reports that SUN
	    // DBX 3.0 has trouble issuing templates - the closing `>'
	    // is missing, as in `TSL_map<unsigned = { ...'.  Hence,
	    // don't check for the closing `>'.
	    pos++;
	}
	else
	{
	    ok = read_up_to(value, pos, '>');
	}
	break;

    default:
	if (isid(value[pos]))
	{
	    // Name or number
	    int start = pos;
	    pos++;
	    while (isid(value[pos]))
		pos++;

	    if (gdb && gdb->program_language() != LANGUAGE_JAVA)
	    {
		string name(value + start, pos - start);
		if (name == "record")
		    read_up_to(value, pos, "end");
		else if (name == "object")
		    read_up_to(value, pos, "end");
		else if (name == "RECORD")
		    read_up_to(value, pos, "END");
		else if (name == "OBJECT")
		    read_up_to(value, pos, "END");
	    }
	}
	else if (isspace(value[0]))
	{
	    // Whitespace
	    pos++;
	    while (isspace(value[pos]))
		pos++;
	}
	else
	{
	    // Single-character token
	    pos++;
	}
	break;
    }

    if (!ok)
    {
	// Could not find closing delimiter.  Treat as one-character token.
	pos = start + 1;
    }
}

// Read next token from VALUE.
string read_token(string& value)
{
    if (value.empty())
	return "";

    int pos = 0;
    read_token(value.chars(), pos);
    string token = string(value.chars(), pos);
    value = value.from(pos);

    // std::clog << "read_token() = " << quote(token) << "\n";
    return token;
}

static bool is_ending_with_end(const string& value)
{
    int i = 0;
    while (i < int(value.length()) && isspace(value[i]))
	i++;
    if (i >= int(value.length()))
	return false;		// At end of value

    return value.contains("end\n", i)
	|| value.contains("END\n", i)
	|| value.contains("end;", i)
	|| value.contains("END;", i)
	|| value.from(i) == "end"
	|| value.from(i) == "END";
}

static bool is_ending_with_paren(const string& value)
{
    int i = 0;
    while (i < int(value.length()) && isspace(value[i]))
	i++;
    if (i >= int(value.length()))
	return false;		// At end of value

    return value.contains('}', i)
	|| value.contains(')', i)
	|| value.contains(']', i);
}

bool is_ending(const string& value)
{
    return is_ending_with_end(value) || is_ending_with_paren(value);
}

bool is_delimited(const string& value)
{
    if (value.contains('\n', 0)
	|| value.contains(',', 0)
	|| value.contains(';', 0)
	|| value.empty()
	|| value.matches(rxwhite))
	return true;

    return is_ending(value);
}

// Read a simple value from VALUE.
string read_simple_value(string& value, int depth, bool ignore_repeats)
{
    // Read values up to [)}],\n]

    read_leading_junk(value);

    string ret = "";
    while (!value.empty() && value[0] != '\n' && 
	   (depth == 0 || !is_delimited(value)))
    {
	ret += read_token(value);

	if (ignore_repeats)
	{
	    // Don't read in `<repeats N times>'
	    int index = 0;
	    while (index < int(value.length()) && isspace(value[index]))
		index++;

#if RUNTIME_REGEX
            static regex rxrepeats("[<]repeats[ \f\t]+[1-9][0-9]*[ \f\t]+times[>]");
#endif
	    if (index < int(value.length()) && 
		value.contains('<', index) && 
		value.contains(rxrepeats, index))
		break;
	}
    }

    strip_trailing_space(ret);

    // std::clog << "read_simple_value() = " << quote(ret) << "\n";
    return ret;
}

// Read a pointer value.
string read_pointer_value (string& value, bool ignore_repeats)
{
    return read_simple_value(value, 1, ignore_repeats);
}

// Read the beginning of an array from VALUE.  Return false iff failure.
bool read_array_begin(string& value, string& addr)
{
    addr = "";
    strip_leading_space(value);

    // GDB has a special format for vtables
    if (value.matches(rxvtable))
	value = value.from("{");

    int pointer_index = 0;
    if (gdb->has_typed_structs() && value.contains('(', 0))
    {
	// JDB prepends the array type and address, as in 
	// `list = (List)0x4070ee90 { ... }'.  Skip the type.
	read_token(value.chars(), pointer_index);
	while (pointer_index < int(value.length()) && 
	       isspace(value[pointer_index]))
	    pointer_index++;
    }

    // XDB and JDB prepend the address before each struct.  Save it.
    int addr_len = rxaddress.match(value.chars(), value.length(), pointer_index);
    if (addr_len > 0)
    {
	addr  = value.at(pointer_index, addr_len);
	value = value.after(pointer_index + addr_len);
	strip_leading_space(value);
    }

    // DBX on DEC prepends `struct' or `class' before each struct;
    // XDB also appends the struct type name.
    if (value.contains("struct", 0) ||
	value.contains("class", 0) ||
	value.contains("union", 0) ||
	value.contains("interface", 0))
    {
	// Skip prefix
	if (!value.contains('{'))
	    return false;

	value = value.from('{');
    }

    if (!gdb->has_array_braces() && value.matches(rxindex))
    {
	value = value.after('=');
	return true;
    }

    if (value.contains('{', 0))
	value = value.after(0);
    else if (value.contains('(', 0))
	value = value.after(0);
    else if (value.contains('[', 0))
	value = value.after(0);
    else if (value.contains("record", 0))
	value = value.after("record");
    else if (value.contains("RECORD", 0))
	value = value.after("RECORD");
    else if (value.contains("OBJECT", 0))
	value = value.after("OBJECT");
    else
	return false;

    strip_leading_space (value);

    // DBX on DEC prepends `[N]' before array member N
    if (value.matches(rxindex))
	value = value.after(']');

    return true;
}

// Read next array element from VALUE.  Return false iff done.
bool read_array_next(string& value)
{
    read_leading_junk(value);

    // DBX on DEC prepends `[N]' before array member N
    if (value.matches(rxindex))
    {
	if (!gdb->has_array_braces())
	    value = value.after('=');
	else
	    value = value.after(']');
	read_leading_junk(value);
    }

    // XDB and M3GDB append `;' after each struct element; others use `,'
    if (value.contains(',', 0) || value.contains(';', 0))
    {
	value = value.after(0);

	if (!gdb->has_array_braces())
	{
	    // Ladebug doesn't end array with "}". For a multi-dimensional
	    // array, we have to detect the next dimmension: '[N] = [N] ='
#if RUNTIME_REGEX
            static regex rxdoubleindex("[[]-?[0-9]+][ \f\t]+*=[ \f\t]+[[]-?[0-9]+](.|\n)*");
#endif
            if (value.matches(rxdoubleindex))
		return false;

	    if (value.matches(rxindex))
		value = value.after('=');
	}

	read_leading_junk (value);
	return !value.empty() && !is_ending(value); // More stuff follows
    }

    if (value.contains('{', 0)
	|| value.contains('(', 0)
	|| value.contains('[', 0)
	|| value.contains("record\n", 0)
	|| value.contains("RECORD\n", 0)
	|| value.contains("OBJECT\n", 0))
    {
	// DBX on Solaris issues arrays of structs without special delimiter
	return true;
    }

    return !value.empty() && !is_ending(value);
}

// Read end of array from VALUE.  Return false iff done.
void read_array_end(string& value)
{
    read_leading_junk(value);

    if (is_ending_with_end(value))
    {
	value = value.from(int(strlen("end")));
	return;
    }

    if (is_ending_with_paren(value))
    {
	value = value.after(0);

	// XDB appends `;' after each struct
	if (value.contains(';', 0))
	    value = value.after(0);

	return;
    }
}

// Read `<repeats N times>'; return N (1 if no repeat)
int read_repeats(string& value)
{
    int repeats = 1;

    strip_leading_space(value);
#if RUNTIME_REGEX
    static regex rxrepeats("[<]repeats[ \f\t]+[1-9][0-9]*[ \f\t]+times[>]");
#endif
    if (value.contains('<', 0) && value.contains(rxrepeats, 0))
    {
	value = value.from(rxint);
	repeats = atoi(value.chars());
	value = value.after('>');
    }

    return repeats;
}

// Read the beginning of a struct from VALUE.  Return false iff done.
bool read_struct_begin (string& value, string& addr)
{
    return read_array_begin(value, addr);
}

// Read next struct element from VALUE.  Return false iff done.
bool read_struct_next (string& value)
{
    return read_array_next(value);
}

// Read end of struct from VALUE.  Return false iff done.
void read_struct_end(string& value)
{
    read_array_end(value);
}

// Skip `members of SUBCLASS:' in VALUE.  Return false iff failure.
bool read_members_prefix (string& value)
{
#if RUNTIME_REGEX
    static regex rxmembers_of_nl("members of [^\n]+: ?\n");
#endif

    read_leading_junk(value);
    if (value.contains(rxmembers_of_nl, 0))
    {
	value = value.after('\n');
	return true;
    }

    return false;
}

// Read member name from VALUE up to SEP
static string get_member_name(string& value, const string& sep)
{
    string prefix = value.before(sep);

    string member_name = "";
    while (!prefix.empty() && prefix[0] != '\n' && !is_ending(prefix))
	member_name += read_token(prefix);

    if (is_ending(prefix))
    {
	// Found an ending in member name.  This is no member.
	return "";
    }

    value = value.after(sep);
    return member_name;
}


// Read member name; return "" upon error
string read_member_name (string& value, bool picky)
{
    read_leading_junk(value);

    if (picky && (value.contains(')', 0)
		  || value.contains('}', 0)
		  || value.contains(']', 0)
		  || value.contains(',', 0)))
    {
	// No value
	return "";
    }

    if (picky && (value.contains('(', 0)
		  || value.contains('{', 0)
		  || value.contains('[', 0)))
    {
	// No value
	return "";
    }

    if (value.contains('=', 0))
    {
	// Missing member name: anonymous union or likewise
	value = value.after(0);
	read_leading_junk(value);
	return " ";
    }

    if (value.contains(rxdbx_baseclass, 0))
    {
	// AIX DBX base class name
	string base = value.before(":(");
	value = value.from("(");

	// Simulate GDB base class name
	return "<" + base + ">";
    }

    string v;
    if (value.contains('\n'))
	v = value.before('\n');
    else
	v = value;

    bool strip_qualifiers = true;

    // GDB, DBX, and XDB separate member names and values by ` = '; 
    // GDB using the Java language uses `: ' instead.
    // JDB printing classes uses `:\n' for the interface list.
    // GDB with GNAT support and Perl use `=> '.
    string member_name = "";
    string sep = gdb->member_separator();
    string sepnl = sep;
    strip_trailing_space(sepnl);
    sepnl += '\n';

    if (v.contains("Virtual table at ", 0))
    {
	// `Virtual table at 0x1234' or likewise.  WDB gives us such things.
	member_name = get_member_name(value, " at ");
	strip_qualifiers = false;
    }
    else if (v.contains(" = "))
    {
	member_name = get_member_name(value, " = ");
    }
    else if (v.contains(sep))
    {
	member_name = get_member_name(value, sep);
    }
    else if (v.contains(sepnl))
    {
	member_name = get_member_name(value, sepnl);
    }
    else
    {
	// Member value in unknown format
	// Should we treat this as anonymous union?  (FIXME)
    }

    if (member_name.empty())
	return "";

    if (!is_BaseClass_name(member_name))
    {
	// Work around buggy member names as generated by Centerline CC.
	// Reported by eggert@igd.fhg.de (Lars Eggert)
	string m = member_name;
	while (m.index("::") > 1)
	    m = m.after("::");
	if (m.index("(") > 1)
	    m = m.before("(");
	if (!m.empty())
	    member_name = m;
    }

    if (gdb->type() == PERL || gdb->type() == DBG)
    {
        if (member_name.contains('\'', 0) && member_name.contains('\'', -1))
        {
	    // Some Perl debugger flavours quote the member name.
	    member_name = unquote(member_name);
        }

	// A PERL hash key can contain spaces. If we try to strip the
	// qualifiers, the key name will be the word after the last space
	// in the key name:
	// 'my key' will be returned as 'key'
	// This has several effects:
	// 1. The key name is displayed incorrectly
	// 2. Since the key name is wrong, you can't access the associated
	//    value trough the Display Data window.
	// 3. Could lead to display a value associated to a valid key
	// so, turn it OFF.
	strip_qualifiers = false;
    }

    if (member_name.contains("[]", -1))
    {
	// JDB lets array member names end in `[]'
	member_name = member_name.before(int(member_name.length()) - 2);
    }

    read_leading_junk (member_name);

    if (member_name.empty())
    {
	// Anonymous union
	return " ";
    }

    if (strip_qualifiers)
    {
	// Strip leading qualifiers.  <Gyula.Kun-Szabo@eth.ericsson.se>
	// reports that his GDB reports static members as `static j = 45'.
	// JDB also qualifies member names (`private String name = Ada`)
	for (;;)
	{
	    strip_space(member_name);
	    string qualifier = member_name.before(' ');
	    if (!qualifier.empty() && qualifier.matches(rxidentifier))
		member_name = member_name.after(' ');
	    else
		break;
	}
    }

    return member_name;
}

// Read `vtable entries' prefix.  Return "" if not found.
string read_vtable_entries (string& value)
{
#if RUNTIME_REGEX
    static regex rxvtable_entries("[0-9][0-9]* vtable entries,");
#endif

    strip_leading_space (value);
    if (value.contains(rxvtable_entries, 0))
    {
	string vtable_entries = value.before(',');
	value = value.after(',');
	return vtable_entries;
    }

    return "";
}


// Return true iff NAME is a baseclass name (enclosed in <>).
bool is_BaseClass_name (const string& name)
{
    return name.length() >= 2
	&& name[0] == '<' && name[name.length() - 1] == '>';
}

// Remove baseclass names. 
// "foo.<Base>" becomes "foo"; "foo-><Base>" becomes "*foo"
void cut_BaseClass_name (string& full_name)
{
    int index = full_name.index (".<", -1);
    if (index > 0) {
	full_name = full_name.before(index);
    }
    else if ((index = full_name.index ("-><", -1)) > 0) {
	full_name = full_name.before(index);
	full_name.prepend ("*");
    }
}

// Skip blanks, M3 comments, and GDB warnings
static void read_leading_junk (string& value)
{
#if RUNTIME_REGEX
    static regex rxm3comment("\\(\\*.*\\*\\).*");
#endif
  
    strip_leading_space(value);
  
    while (value.matches(rxm3comment))
    {
	read_leading_comment(value);
	strip_leading_space(value);
    }

    while (value.contains("warning: ", 0))
    {
	value = value.after('\n');
	strip_leading_space(value);
    }
}

// Read leading M2/M3 comment.
static void read_leading_comment (string& value)
{
    int i = 2;
    bool sf = false;

    while (i < int(value.length()))
    {
	switch (value[i])
	{
	case '*': 
	    sf = true;
	    break;

	case ')': 
	    if (sf)
	    {
		value = value.from(i + 1);
		return;
	    }
        // FALL THROUGH

	default: 
	    sf = false; 
	    break;
	}
	i++;
    }

    value = value.from(i);
}
