// $Id$ -*- C++ -*-
// Setup DDD fonts

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

#ifndef _DDD_fonts_h
#define _DDD_fonts_h

#include "base/strclass.h"
#include "AppData.h"
#include "motif/MakeMenu.h"

#include <vector>

#include <X11/Intrinsic.h>

// Font types
enum DDDFont { DefaultDDDFont       = 0,
	       VariableWidthDDDFont = 1,
	       FixedWidthDDDFont    = 2,
               DataDDDFont          = 4 };

// Setup font specs.  DB is the resource database in use.
extern void setup_fonts(AppData& app_data, XrmDatabase db = 0);

// Return font name from BASE, overriding with parts from OVERRIDE.
// Override contains "-SPEC-SPEC-..."; each non-empty SPEC overrides
// the default in BASE.
extern string make_font(const AppData& ad, DDDFont base, 
			const string& override = "");

extern string make_xftfont(const AppData& ad, DDDFont base);

// Set a new font resource
extern void set_font(DDDFont n, const string& name);

std::vector<string> GetFixedWithFonts();
std::vector<string> GetVariableWithFonts();

#ifndef HAVE_FREETYPE
// Browse fonts
extern void BrowseFontCB(Widget w, XtPointer, XtPointer);
#endif

// Set font name and size
extern void SetFontNameCB(Widget w, XtPointer, XtPointer);
extern void SetFontSizeCB(Widget w, XtPointer, XtPointer);

#endif // _DDD_fonts_h
// DON'T ADD ANYTHING BEHIND THIS #endif
