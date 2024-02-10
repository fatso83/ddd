// $Id$ -*- C++ -*-
// DDD window color functions

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

char darkmode_rcsid[] =
    "$Id$";

#include "darkmode.h"

#include <Xm/Xm.h>

    
void setColorMode(Widget w, bool darkmode)
{
    // Fetch children
    WidgetList children;
    Cardinal numChildren = 0;
    XtVaGetValues(w, XmNchildren, &children, XmNnumChildren, &numChildren, XtPointer(0));

    for (int i = 0; i < int(numChildren); i++)
    {
        Widget child = children[i];
        Pixel color;
        XtVaGetValues(child, XmNbackground, &color,  XtPointer(0));
        int sumcolor = ((color & 0xff0000)>>16) + ((color & 0x00ff00)>>8) + (color & 0x0000ff);
        if (darkmode && sumcolor>3*128)
        {
            color = color ^ 0xffffff;
            XmChangeColor(child, color);
        }
        else if (!darkmode && sumcolor<=3*128)
        {
            color = color ^ 0xffffff;
            XmChangeColor(child, color);
        }

        setColorMode (child, darkmode);
    }
}

