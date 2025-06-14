// $Id$ -*- C++ -*-
// `smart' string comparison

// Copyright (C) 1998 Technische Universitaet Braunschweig, Germany.
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

char SmartCompare_rcsid[] = 
    "$Id$";

#include "SmartC.h"
#include "assert.h"

#include <ctype.h>
#include <stdlib.h>

// Compare S1 and S2, taking numerals into account
// returns < 0, > 0, or 0 iff S1 < S2, S1 > S2, or S1 == S2.
int smart_compare(const char *s1, const char *s2)
{
    for (;;)
    {
	while (*s1 != '\0' && *s2 != '\0' && *s1 == *s2)
	    s1++, s2++;

	if (*s1 != '\0' && isdigit(*s1) && 
	    *s2 != '\0' && isdigit(*s2))
	{
	    // Compare numerals numerically
	    const char *e1 = s1;
	    const char *e2 = s2;

	    long i1 = strtol(s1, (char **)&e1, 0);
	    long i2 = strtol(s2, (char **)&e2, 0);

	    assert(e1 != s1 && e2 != s2);

	    int ret = i1 - i2;
	    if (ret != 0)
		return ret;

	    // Continue after numerals
	    s1 = e1;
	    s2 = e2;
	}
	else
	{
	    // Simple case-insensitive string comparison
// 	    return *s1 - *s2;
            if (*s1==*s2) // only for s1 == s2 =='\0'
                return 0;

            char c1 = *s1;
            char c2 = *s2;
            if (c1>=0x61 && c1<=0x7a)
                c1 -= 0x20;
            if (c2>=0x61 && c2<=0x7a)
                c2 -= 0x20;
            if (c1!=c2)
                return c1 - c2;

            s1++;
            s2++;
	}
    }
}

int smart_compare(const string& s1, const string& s2)
{
  return smart_compare(s1.chars(), s2.chars());
}

// Shell sort -- simple and fast
#define SMART_SHELL_SORT(type, a, size) \
    int h = 1; \
    do { \
	h = h * 3 + 1; \
    } while (h <= size); \
    do { \
	h /= 3; \
	for (int i = h; i < size; i++) \
	{ \
	    type v = a[i]; \
	    int j; \
	    for (j = i; j >= h && smart_compare(a[j - h], v) > 0; j -= h) \
		a[j] = a[j - h]; \
	    if (i != j) \
		a[j] = v; \
	} \
    } while (h != 1)


// Sort array A, using smart_compare
void smart_sort(std::vector<string>& a)
{
    SMART_SHELL_SORT(string, a, int(a.size()));
}

// Sort array A, using smart_compare
void smart_sort(char *a[], int size)
{
    SMART_SHELL_SORT(char *, a, size);
}

// Sort array A, using smart_compare
void smart_sort(string *a, int size)
{
    SMART_SHELL_SORT(string, a, size);
}

// Remove adjacent duplicates in A
void uniq(std::vector<string>& a)
{
    std::vector<string> b;

    for (int i = 0; i < int(a.size()); i++)
    {
	if (i == 0 || a[i - 1] != a[i])
	    b.push_back(a[i]);
    }
    
    a = b;
}
