// $Id$ -*- C++ -*-
// Regular expression class, based on POSIX regcomp()/regexec() interface

// Copyright (C) 1996 Technische Universitaet Braunschweig, Germany.
// Written by Andreas Zeller <zeller@gnu.org>.
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

char regex_rcsid[] = 
    "$Id$";

#include "config.h"
#include "bool.h"
#include "strclass.h"
#include "assert.h"
#include "misc.h"
#include "cook.h"

#include <stdlib.h>
#include <iostream>
#include <ctype.h>
#include <string.h>		// strncmp()

#if WITH_RUNTIME_REGEX
// Get a prefix character from T; let T point at the next prefix character.
char regex::get_prefix(const char *& t, int flags)
{
    if (flags & REG_EXTENDED)
    {
	switch (*t++)
	{
	case '.':
	case '(':
	case '^':
	case '$':
	    return '\0';

	case '[':
	    if (*t != ']' && *t != '\0' && *t != '^' && t[1] == ']')
	    {
		char ret = *t;
		t += 2;
		return ret;
	    }
	    return '\0';

	case '\\':
	    return *t++;

	default:
	    // Ordinary character
	    switch (*t)
	    {
	    case '+':
	    case '*':
	    case '?':
	    case '|':
		return '\0';
	    
	    default:
		return t[-1];
	    }
	}
    }
    else
    {
	// FIXME: give some rules for ordinary regexps
	return '\0';
    }
}

void regex::fatal(int errcode, const char *src)
{
    if (errcode == 0)
	return;

    size_t length = regerror(errcode, &compiled, (char *)0, 0);
    char *buffer = new char[length];
    regerror(errcode, &compiled, buffer, length);

    std::cerr << "regex ";
    if (src)
	 std::cerr << quote(src) << ": ";
    std::cerr << "error " << errcode;
    if (buffer[0] != '\0')
	std::cerr << " - " << buffer;
    std::cerr << "\n";
#if !defined(REGCOMP_BROKEN) && !defined(GNU_LIBrx_USED)
    std::cerr << "As a workaround, link with GNU librx - "
	"in `config.h', #define REGCOMP_BROKEN.\n";
#endif
    delete[] buffer;

    abort();
}

regex::regex(const char* t, int flags)
    : exprs(0), matcher(0), data(0)
{
    const string rx = "^" + string(t);
    int errcode = regcomp(&compiled, rx.chars(), flags);
    if (errcode)
	fatal(errcode, rx.chars());

    exprs = new regmatch_t[nexprs()];

    unsigned int i = 0;
    const char *s = t;
    while ((prefix[i++] = get_prefix(s, flags)) != '\0'
	   && i < sizeof(prefix) - 1)
	;
    prefix[i] = '\0';
}
#endif // WITH_RUNTIME_REGEX

regex::regex(rxmatchproc p, void *d)
    : 
#if WITH_RUNTIME_REGEX
    exprs(0),
#endif
    matcher(p), data(d)
{
#if WITH_RUNTIME_REGEX
    prefix[0] = '\0';
#endif
}

regex::~regex()
{
#if WITH_RUNTIME_REGEX
    if (matcher == 0)
	regfree(&compiled);
    delete[] exprs;
#endif // WITH_RUNTIME_REGEX
}

// Search T in S; return position of first occurrence.
// If STARTPOS is positive, start search from that position.
// If STARTPOS is negative, perform reverse search from that position 
// and return last occurrence.
// MATCHLEN contains the length of the matched expression.
// If T is not found, return -1.
int regex::search(const char* s, int len, int& matchlen, int startpos) const
{
    string substr;
    int direction = +1;

    if (startpos < 0)
    {
	startpos += len;
	direction = -1;
    }
    if (startpos < 0 || startpos > len)
	return -1;

    if (s[len] != '\0')
    {
	substr = string(s, len);
	s = substr.chars();
    }
    assert(s[len] == '\0');

#if WITH_RUNTIME_REGEX
    int errcode = 0;
    int prefix_len = strlen(prefix);
#endif

    for (; startpos >= 0 && startpos < len; startpos += direction)
    {
	if (matcher != 0)
	{
	    matchlen = matcher(data, s, len, startpos);
	    if (matchlen >= 0)
		break;
	}
#if WITH_RUNTIME_REGEX
	else
	{ 
	    const char *t = s + startpos;
	    if (strncmp(t, prefix, min(prefix_len, len - startpos)) == 0)
	    {
		errcode = regexec((regex_t *)&compiled, t, nexprs(), exprs, 0);
		if (errcode == 0)
		    break;
	    }
	}
#endif // WITH_RUNTIME_REGEX
    }

    if (startpos < 0 || startpos >= len)
	return -1;

    int matchpos = startpos;

#if WITH_RUNTIME_REGEX
    if (exprs[0].rm_so >= 0)
    {
	matchpos = exprs[0].rm_so + startpos;
	matchlen = exprs[0].rm_eo - exprs[0].rm_so;
    }
    else
    {
	matchpos = -1;
	matchlen = 0;
    }
#endif // WITH_RUNTIME_REGEX

    return matchpos;
}

// Return length of matched string iff T matches S at POS, 
// -1 otherwise.  LEN is the length of S.
int regex::match(const char *s, int len, int pos) const
{
    if (matcher != 0)
	return matcher(data, s, len, pos);

#if WITH_RUNTIME_REGEX
    string substr;
    if (pos < 0)
	pos += len;
    if (pos > len)
	return -1;

    if (s[len] != '\0')
    {
	substr = string(s, len);
	s = substr.chars();
    }
    assert(s[len] == '\0');

    int errcode = regexec((regex_t *)&compiled, s + pos, 
			  nexprs(), exprs, 0);

    if (errcode == 0 && exprs[0].rm_so >= 0)
	return exprs[0].rm_eo - exprs[0].rm_so;
#endif

    return -1;
}

#if WITH_RUNTIME_REGEX
bool regex::match_info(int& start, int& length, int nth) const
{
    if ((unsigned)(nth) >= nexprs())
	return false;
    else
    {
	start  = exprs[nth].rm_so;
	length = exprs[nth].rm_eo - start;
	return start >= 0 && length >= 0;
    }
}
#endif

bool regex::OK() const
{
#if WITH_RUNTIME_REGEX
    assert(exprs != 0);
#endif
    return true;
}


#if WITH_RUNTIME_REGEX && RUNTIME_REGEX
// Built-in regular expressions

const regex rxwhite("[ \n\t\r\v\f]+");
const regex rxint("-?[0-9]+");
const regex rxdouble("-?(([0-9]+\\.[0-9]*)|([0-9]+)|(\\.[0-9]+))"
		     "([eE][---+]?[0-9]+)?");
const regex rxalpha("[A-Za-z]+");
const regex rxlowercase("[a-z]+");
const regex rxuppercase("[A-Z]+");
const regex rxalphanum("[0-9A-Za-z]+");
const regex rxidentifier("[A-Za-z_$][A-Za-z0-9_$]*");
#endif // WITH_RUNTIME_REGEX
