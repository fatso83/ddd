// $Id$ -*- C++ -*-
// Assoc<int, VarArray<int> > and Assoc<string, VarArray<int> >.

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

#ifndef _DDD_IntIntArrayAssoc_h
#define _DDD_IntIntArrayAssoc_h

#include "Assoc.h"
#include "base/strclass.h"

#include <vector>

typedef Assoc<int, std::vector<int>> IntIntArrayAssoc;
typedef AssocIter<int, std::vector<int>> IntIntArrayAssocIter;

typedef std::vector<std::vector<int>> IntArrayArray;

typedef Assoc<string, IntArrayArray> StringIntArrayArrayAssoc;
typedef AssocIter<string, IntArrayArray> StringIntArrayArrayAssocIter;

#endif // _DDD_IntIntArrayAssoc_h
// DON'T ADD ANYTHING BEHIND THIS #endif
