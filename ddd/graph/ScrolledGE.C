// $Id$  -*- C++ -*-
// Scrolled Graph Editor

// Copyright (C) 1995 Technische Universitaet Braunschweig, Germany.
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

char ScrolledGraphEdit_rcsid[] = 
    "$Id$";

#include <iostream>
#include "ScrolleGEP.h"
#include "GraphEdit.h"
#include "x11/verify.h"
#include "base/strclass.h"

// We have no special class for scrolling a graph editor, but use the
// Motif ScrolledWindow class instead.

WidgetClass scrolledGraphEditWidgetClass = xmScrolledWindowWidgetClass;

static void ResizeEH(Widget, XtPointer client_data, XEvent *, Boolean *)
{
    Widget graphEdit = Widget(client_data);
    graphEditSizeChanged(graphEdit);
}

void CallActionScrolled(Widget gw, XEvent *event, String *params, Cardinal *num_params)
{
    (void) event;
    if (*num_params != 2)
        return;

    int dir = 2;   // 0: horizontal, 1: vertical, 2: unknown
    for (int i=0; i<int(*num_params); i++)
    {
        if (params[i][0] != '0')
        {
            dir = i;
            break;
        }
    }

    if (dir == 2)
        return;

    Widget sbw = nullptr;
    if (dir == 0)
        XtVaGetValues (gw, XmNhorizontalScrollBar, &sbw, nullptr);
    else
        XtVaGetValues (gw, XmNverticalScrollBar, &sbw, nullptr);
    int increment=0;
    int maximum=0;
    int minimum=0;
    int page_incr=0;
    int slider_size=0;
    int value=0;
    XtVaGetValues (sbw, XmNincrement, &increment,
                        XmNmaximum, &maximum,
                        XmNminimum, &minimum,
                        XmNpageIncrement, &page_incr,
                        XmNsliderSize, &slider_size,
                        XmNvalue, &value,
                        NULL);

    if (params[dir][0] == '-')
        value -= increment;
    else
        value += increment;

    value = std::max(minimum, std::min(maximum-slider_size, value));

    XmScrollBarSetValues(sbw, value, slider_size, increment, page_incr, true);
}

Widget createScrolledGraphEdit(Widget parent, const _XtString name,
			       ArgList arglist, Cardinal argcount)
{
    Arg args[10];
    int arg = 0;

    XtSetArg(args[arg], XmNscrollingPolicy, XmAUTOMATIC); arg++;
    string swindow_name = string(name) + "_swindow";

    XtSetArg(args[arg], XmNborderWidth,     0); arg++;
    XtSetArg(args[arg], XmNspacing,         0); arg++;
    XtSetArg(args[arg], XmNshadowThickness, 0); arg++;
    XtSetArg(args[arg], XmNpaneMaximum,  5000); arg++;

    Widget scrolledWindow = 
	verify(XtCreateManagedWidget(swindow_name.chars(), 
				     scrolledGraphEditWidgetClass,
				     parent, args, arg));

    Widget graphEdit = 
	verify(XtCreateManagedWidget(name, graphEditWidgetClass,
				     scrolledWindow, arglist, argcount));

    XtAddEventHandler(scrolledWindow, StructureNotifyMask, False,
		      ResizeEH, XtPointer(graphEdit));

    // Propagate requested width and height of graph editor to scrolled window
    Dimension width, height;
    XtVaGetValues(graphEdit, 
		  XtNrequestedWidth, &width,
		  XtNrequestedHeight, &height,
		  XtPointer(0));

    if (width > 0)
	XtVaSetValues(scrolledWindow, XmNwidth, width, XtPointer(0));
    if (height > 0)
	XtVaSetValues(scrolledWindow, XmNheight, height, XtPointer(0));

    return graphEdit;
}

// For a given graph editor W, return its scroller
Widget scrollerOfGraphEdit(Widget w)
{
    XtCheckSubclass(w, GraphEditWidgetClass, "Bad widget class");

    Widget parent = w;
    while (parent != 0)
    {
	if (XtIsSubclass(parent, scrolledGraphEditWidgetClass))
	    break;
	parent = XtParent(parent);
    }
    return parent;
}
