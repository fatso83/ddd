// $Id$ -*- C++ -*-
// Swallower Widget - swallow a user application

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

#ifndef _DDD_Swallower_h
#define _DDD_Swallower_h

#include <X11/Intrinsic.h>
#include <X11/StringDefs.h>

// This widget `swallows' an X application.  You pass the app window,
// and it will be reparented as a child of this widget.
//
// Resources:
//
// window
//     The application window to be swallowed, or `None'.
//
// Realizing the widget requires WINDOW to be non-None.
//
//
// Callbacks:
//
// windowGoneCallback
//     Called whenever the app window is destroyed or reparented.
//
// windowCreatedCallback
//     Called whenever a new app window is created.
//     Useful if you have to wait for the app window.
//
// Each callback is passed a `SwallowerInfo' structure pointer:

struct SwallowerInfo {
    Window window;
    XEvent *event;
};

// WINDOW is the window gone or created; EVENT is the event causing
// the callback.

// Declare specific Swallower class and instance datatypes
typedef struct _SwallowerClassRec *SwallowerWidgetClass;
typedef struct _SwallowerRec      *SwallowerWidget;

// Declare the class constant
extern WidgetClass swallowerWidgetClass;

// Define any new resources
#define XtNwindowGoneCallback    "windowGoneCallback"

#endif // _DDD_Swallower_h
// DON'T ADD ANYTHING BEHIND THIS #endif
