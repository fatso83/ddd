// $Id$ -*- C++ -*-
// Create a combo box

// Copyright (C) 1998 Technische Universitaet Braunschweig, Germany.
// Copyright (C) 2001 Free Software Foundation, Inc.
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

char ComboBox_rcsid[] = 
    "$Id$";

// #define as 0 to rely exclusively on our replacement routines
#define USE_XM_COMBOBOX 0

#include "ComboBox.h"

#include "base/bool.h"
#include "x11/frame.h"
#include "x11/charsets.h"
#include "x11/verify.h"
#include "wm.h"
#include "agent/TimeOut.h"
#include "x11/AutoRaise.h"
#include "mydialogs.h"
#include "base/misc.h"

#include <Xm/Xm.h>
#include <Xm/ArrowB.h>
#include <Xm/TextF.h>
#include <Xm/List.h>
#include <Xm/Frame.h>
#include <Xm/Form.h>
#include <X11/Shell.h>
#include <X11/cursorfont.h>

// Whether to use XmComboBox
#ifndef USE_XM_COMBOBOX
#if XmVersion >= 2000
#define USE_XM_COMBOBOX 1
#else
#define USE_XM_COMBOBOX 0
#endif
#endif

#if USE_XM_COMBOBOX
#include <Xm/ComboBox.h>
#endif


//-----------------------------------------------------------------------
// ComboBox helpers
//-----------------------------------------------------------------------

struct ComboBoxInfo
{
    Widget top;			// The top-level window
    Widget text;		// The text to be updated
    Widget button;		// The arrow button
    Widget list;		// The list to select from
    Widget shell;		// The shell that contains the list
    XtIntervalId timer;		// The timer that controls popup time
    bool popped_up;		// True iff the combo box is popped up

    ComboBoxInfo()
	: top(0), text(0), button(0), list(0), shell(0), 
	  timer(0), popped_up(false)
    {}

private:
    ComboBoxInfo(const ComboBoxInfo&);

    ComboBoxInfo& operator= (const ComboBoxInfo&);
};


#if !USE_XM_COMBOBOX

static void CloseWhenActivatedCB(XtPointer client_data, XtIntervalId *id)
{
    ComboBoxInfo *info = (ComboBoxInfo *)client_data;
    
    assert(*id == info->timer);
    (void) id;

    info->timer = 0;
}

static void PopdownComboListCB(Widget, XtPointer client_data, XtPointer)
{
    ComboBoxInfo *info = (ComboBoxInfo *)client_data;

    XtVaSetValues(info->button, XmNarrowDirection, XmARROW_DOWN, XtPointer(0));
    XtPopdown(info->shell);
    info->popped_up = false;
}

static void PopupComboListCB(Widget w, XtPointer client_data, 
			     XtPointer call_data)
{
    ComboBoxInfo *info = (ComboBoxInfo *)client_data;

    if (info->popped_up)
    {
	PopdownComboListCB(w, client_data, call_data);
	return;
    }

    // Get size and position
    Position top_x, top_y;
    XtTranslateCoords(info->top, 0, 0, &top_x, &top_y);

    Dimension top_height = 0;
    Dimension top_width  = 0;
    XtVaGetValues(info->top, XmNheight, &top_height,
		  XmNwidth, &top_width, XtPointer(0));

    // Query preferred height of scroll window
    XtWidgetGeometry size;
    size.request_mode = CWHeight;
    XtQueryGeometry(XtParent(info->list), (XtWidgetGeometry *)0, &size);

    Dimension current_height;
    XtVaGetValues(info->shell, XmNheight, &current_height, XtPointer(0));
    
    Position x       = top_x;
    Position y       = top_y + top_height;
    Dimension width  = top_width;
    Dimension height = max(size.height, current_height);

    XtVaSetValues(info->shell, XmNx, x, XmNy, y, 
		  XmNwidth, width, XmNheight, height, XtPointer(0));


    // Popup shell
    XtPopup(info->shell, XtGrabNone);
    if (XtIsRealized(info->shell))
	XRaiseWindow(XtDisplay(info->shell), XtWindow(info->shell));
    info->popped_up = true;

    // Unmanage horizontal scrollbar
    Widget horizontal_scrollbar = 0;
    XtVaGetValues(XtParent(info->list), XmNhorizontalScrollBar, 
		  &horizontal_scrollbar, XtPointer(0));
    if (horizontal_scrollbar != 0)
	XtUnmanageChild(horizontal_scrollbar);

    XtVaSetValues(info->button, XmNarrowDirection, XmARROW_UP, XtPointer(0));

    static Cursor cursor = XCreateFontCursor(XtDisplay(info->shell), XC_arrow);
    XDefineCursor(XtDisplay(info->shell), XtWindow(info->shell), cursor);

    // If we release the button within the next 250ms, keep the menu open.
    // Otherwise, pop it down again.
    if (info->timer != 0)
	XtRemoveTimeOut(info->timer);
    info->timer = 
	XtAppAddTimeOut(XtWidgetToApplicationContext(info->shell), 250,
			CloseWhenActivatedCB, XtPointer(info));
}

static void ActivatePopdownComboListCB(Widget w, XtPointer client_data, 
				       XtPointer call_data)
{
    ComboBoxInfo *info = (ComboBoxInfo *)client_data;
    if (info->timer == 0)
	PopdownComboListCB(w, client_data, call_data);
}

#endif // !USE_XM_COMBOBOX


static void RefreshComboTextCB(Widget w, XtPointer client_data,
			       XtPointer call_data)
{
    (void) w;			// Use it

    XmListCallbackStruct *cbs = (XmListCallbackStruct *)call_data;
    ComboBoxInfo *info = (ComboBoxInfo *)client_data;

    XmString item = cbs->item;
    String item_s;
    XmStringGetLtoR(item, CHARSET_TT, &item_s);
    XmTextFieldSetString(info->text, item_s);
    XtFree(item_s);

    XmTextPosition last_pos = XmTextFieldGetLastPosition(info->text);
    XmTextFieldSetInsertionPosition(info->text, last_pos);
    XmTextFieldShowPosition(info->text, 0);
    XmTextFieldShowPosition(info->text, last_pos);

#if !USE_XM_COMBOBOX
    PopdownComboListCB(w, client_data, call_data);
#endif
}


//-----------------------------------------------------------------------
// ComboBox access
//-----------------------------------------------------------------------

// Access ComboBox members
Widget ComboBoxList(Widget text)
{
    XtPointer userData;
    XtVaGetValues(text, XmNuserData, &userData, XtPointer(0));
    ComboBoxInfo *info = (ComboBoxInfo *)userData;
    return info->list;
}

Widget ComboBoxText(Widget text)
{
    return text;
}

Widget ComboBoxButton(Widget text)
{
    XtPointer userData;
    XtVaGetValues(text, XmNuserData, &userData, XtPointer(0));
    ComboBoxInfo *info = (ComboBoxInfo *)userData;
    return info->button;
}

#if 0
// unused
Boolean ComboBoxIsSimple(Widget text)
{
    return ComboBoxButton(text) != 0;
}
#endif

Widget ComboBoxTop(Widget text)
{
    XtPointer userData;
    XtVaGetValues(text, XmNuserData, &userData, XtPointer(0));
    ComboBoxInfo *info = (ComboBoxInfo *)userData;
    return info->top;
}


// Set items
void ComboBoxSetList(Widget text, const std::vector<string>& items)
{
    Widget list = ComboBoxList(text);

    // Check for value change
    XmStringTable old_items;
    int old_items_count = 0;

    XtVaGetValues(list,
		  XmNitemCount, &old_items_count,
		  XmNitems,     &old_items,
		  XtPointer(0));

    int i;
    if (old_items_count == int(items.size()))
    {
	for (i = 0; i < int(items.size()); i++)
	{
	    String _old_item;
	    XmStringGetLtoR(old_items[i], CHARSET_TT, &_old_item);
	    string old_item(_old_item);
	    XtFree(_old_item);

	    if (old_item != items[i])
		break;
	}

	if (i >= int(items.size()))
	{
	    // All elements are equal
	    return;
	}
    }

    // Set new values
    XmStringTable xmlist = 
	XmStringTable(XtMalloc(items.size() * sizeof(XmString)));

    for (i = 0; i < int(items.size()); i++)
	xmlist[i] = XmStringCreateLtoR(XMST(items[i].chars()), CHARSET_TT);

    XtVaSetValues(list,
		  XmNitems,     xmlist,
		  XmNitemCount, items.size(),
		  XtPointer(0));

    freeXmStringTable(xmlist, items.size());
}


//-----------------------------------------------------------------------
// ComboBox creation
//-----------------------------------------------------------------------

// Create a combo box
Widget CreateComboBox(Widget parent, const _XtString name, 
		      ArgList _args, Cardinal _arg, bool editable)
{
    ArgList args = new Arg[_arg + 10];
    Cardinal arg = 0;

    ComboBoxInfo *info = new ComboBoxInfo;

    arg = 0;
    XtSetArg(args[arg], XmNshadowType, XmSHADOW_IN); arg++;
    XtSetArg(args[arg], XmNmarginWidth,        0); arg++;
    XtSetArg(args[arg], XmNmarginHeight,       0); arg++;
    XtSetArg(args[arg], XmNborderWidth,        0); arg++;
    XtSetArg(args[arg], XmNhighlightThickness, 0); arg++;
    info->top = verify(XmCreateFrame(parent, XMST("frame"), args, arg));
    XtManageChild(info->top);

    arg = 0;
    XtSetArg(args[arg], XmNmarginWidth,        0); arg++;
    XtSetArg(args[arg], XmNmarginHeight,       0); arg++;
    XtSetArg(args[arg], XmNborderWidth,        0); arg++;
    XtSetArg(args[arg], XmNhighlightThickness, 0); arg++;
    Widget form = verify(XmCreateForm(info->top, XMST("form"), args, arg));
    XtManageChild(form);

#if USE_XM_COMBOBOX
    // ComboBoxes in OSF/Motif 2.0 sometimes resize themselves without
    // apparent reason.  Prevent this by constraining them in a form.
    arg = 0;
    XtSetArg(args[arg], XmNleftAttachment,     XmATTACH_FORM); arg++;
    XtSetArg(args[arg], XmNrightAttachment,    XmATTACH_FORM); arg++;
    XtSetArg(args[arg], XmNtopAttachment,      XmATTACH_FORM); arg++;
    XtSetArg(args[arg], XmNbottomAttachment,   XmATTACH_FORM); arg++;
    XtSetArg(args[arg], XmNresizable,          False);         arg++;
    for (Cardinal i = 0; i < _arg; i++)
	args[arg++] = _args[i];
    Widget combo = verify(XmCreateDropDownComboBox(form, XMST(name), 
						   args, arg));
    XtManageChild(combo);

    arg = 0;
    XtSetArg(args[arg], XmNshadowThickness, 0); arg++;
    XtSetArg(args[arg], XmNhighlightThickness, 0); arg++;
    XtSetArg(args[arg], XmNborderWidth, 0); arg++;
    XtSetArg(args[arg], XmNmarginWidth, 0); arg++;
    XtSetArg(args[arg], XmNmarginHeight, 0); arg++;
    XtSetValues(combo, args, arg);

    info->text = XtNameToWidget(combo, "*Text");
    arg = 0;
    XtSetArg(args[arg], XmNshadowThickness, 0); arg++;
    XtSetValues(info->text, args, arg);

    info->list = XtNameToWidget(combo, "*List");
    arg = 0;
    XtSetArg(args[arg], XmNshadowThickness,    2); arg++;
    XtSetArg(args[arg], XmNhighlightThickness, 0); arg++;
    XtSetValues(info->list, args, arg);

    info->shell = info->list;
    while (!XtIsShell(info->shell))
	info->shell = XtParent(info->shell);

    // Set form size explicitly.
    XtWidgetGeometry size;
    size.request_mode = CWHeight | CWWidth;
    XtQueryGeometry(combo, (XtWidgetGeometry *)0, &size);
    XtVaSetValues(form, 
		  XmNheight, size.height, 
		  XmNwidth, size.width,
		  XtPointer(0));

    // Set frame size explicitly, too
    Dimension shadow_thickness;
    XtVaGetValues(info->top,
		  XmNshadowThickness, &shadow_thickness,
		  XtPointer(0));
    XtVaSetValues(info->top, 
		  XmNheight, size.height + shadow_thickness * 2, 
		  XmNwidth, size.width + shadow_thickness * 2,
		  XtPointer(0));
#else
    // Create text field and arrow
    arg = 0;
    XtSetArg(args[arg], XmNborderWidth,        0);     arg++;
    XtSetArg(args[arg], XmNhighlightThickness, 0);     arg++;
    XtSetArg(args[arg], XmNshadowThickness,    0);     arg++;
    XtSetArg(args[arg], XmNresizable,          False); arg++;
    if (editable==false)
    {
        XtSetArg(args[arg], XmNeditable,           False); arg++;
        XtSetArg(args[arg], XmNcursorPositionVisible, False); arg++;
    }
    for (Cardinal i = 0; i < _arg; i++)
	args[arg++] = _args[i];
    info->text = verify(XmCreateTextField(form, XMST(name), args, arg));
    XtManageChild(info->text);

    Pixel foreground;
    XtVaGetValues(parent, XmNbackground, &foreground, XtPointer(0));

    arg = 0;
    XtSetArg(args[arg], XmNarrowDirection,     XmARROW_DOWN);  arg++;
    XtSetArg(args[arg], XmNborderWidth,        0);             arg++;
    XtSetArg(args[arg], XmNforeground,         foreground);    arg++;
    XtSetArg(args[arg], XmNhighlightThickness, 0);             arg++;
    XtSetArg(args[arg], XmNshadowThickness,    0);             arg++;
    XtSetArg(args[arg], XmNresizable,          False);         arg++;
    XtSetArg(args[arg], XmNrightAttachment,    XmATTACH_FORM); arg++;
    XtSetArg(args[arg], XmNtopAttachment,      XmATTACH_FORM); arg++;
    XtSetArg(args[arg], XmNbottomAttachment,   XmATTACH_FORM); arg++;
    info->button = XmCreateArrowButton(form, 
				       XMST("comboBoxArrow"), args, arg);
    XtManageChild(info->button);

    XtVaSetValues(info->text,
		  XmNleftAttachment,   XmATTACH_FORM,
		  XmNrightAttachment,  XmATTACH_WIDGET,
		  XmNrightWidget,      info->button,
		  XmNtopAttachment,    XmATTACH_FORM,
		  XmNbottomAttachment, XmATTACH_FORM,
		  XtPointer(0));

    XtAddCallback(info->button, XmNarmCallback,
		  PopupComboListCB, XtPointer(info));
    XtAddCallback(info->button, XmNactivateCallback,
		  ActivatePopdownComboListCB, XtPointer(info));

    XtAddCallback(info->text, XmNvalueChangedCallback,
		  PopdownComboListCB, XtPointer(info));
    XtAddCallback(info->text, XmNactivateCallback,
		  PopdownComboListCB, XtPointer(info));

    // Create the popup shell
    Widget shell = parent;
    while (!XtIsShell(shell))
	shell = XtParent(shell);

    XtAddCallback(shell, XmNpopdownCallback,
		  PopdownComboListCB, XtPointer(info));

    arg = 0;
    XtSetArg(args[arg], XmNborderWidth, 0); arg++;
    info->shell = XtCreatePopupShell("comboBoxShell", 
				     overrideShellWidgetClass,
				     parent, args, arg);

    arg = 0;
    XtSetArg(args[arg], XmNhighlightThickness, 0); arg++;
    info->list = XmCreateScrolledList(info->shell, XMST("list"), args, arg);
    XtManageChild(info->list);

    // Keep shell on top
    _auto_raise(info->shell);

    // Set form size explicitly.
    XtWidgetGeometry size;
    size.request_mode = CWHeight | CWWidth;
    XtQueryGeometry(info->text, (XtWidgetGeometry *)0, &size);
    XtVaSetValues(form,
		  XmNheight, size.height,
		  XmNwidth, size.width,
		  XtPointer(0));

    // Set frame size explicitly, too
    Dimension shadow_thickness;
    XtVaGetValues(info->top,
		  XmNshadowThickness, &shadow_thickness,
		  XtPointer(0));
    XtVaSetValues(info->top,
		  XmNheight, size.height + shadow_thickness * 2, 
		  XmNwidth, size.width + shadow_thickness * 2,
		  XtPointer(0));
#endif

    // Give shell a little more border
    XtVaSetValues(info->shell, XmNborderWidth, 1, XtPointer(0));

    // Store ComboBox info in text field
    XtVaSetValues(info->text, XmNuserData, XtPointer(info), XtPointer(0));

    // Synchronize text and list
    XtAddCallback(info->list, XmNbrowseSelectionCallback,
		  RefreshComboTextCB, XtPointer(info));
    XtAddCallback(info->list, XmNsingleSelectionCallback,
		  RefreshComboTextCB, XtPointer(info));
    XtAddCallback(info->list, XmNmultipleSelectionCallback,
		  RefreshComboTextCB, XtPointer(info));
    XtAddCallback(info->list, XmNextendedSelectionCallback,
		  RefreshComboTextCB, XtPointer(info));

    delete[] args;
    return info->text;
}
