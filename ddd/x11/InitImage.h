// $Id$ -*- C++ -*-
// Create an XImage from bitmap data

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

#ifndef _DDD_InitImage_h
#define _DDD_InitImage_h

#include <X11/Xlib.h>
#include <X11/Intrinsic.h>

// Create IMAGE from bitmap source
extern XImage *CreateImageFromBitmapData(unsigned char *bits,
					 int width, int height);

// Install bitmap in Motif cache
Boolean InstallBitmap(unsigned char *bits, int width, int height, const char *name);

// Install bitmap as Pixmap in Motif cache
Boolean InstallBitmapAsXImage(Widget w, unsigned char *bits, int width, int height, const char *name, int scalefactor);



#endif // _DDD_InitImage_h
// DON'T ADD ANYTHING BEHIND THIS #endif
