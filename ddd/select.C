// $Id$ -*- C++ -*-
// Select from a list of choices presented by GDB

// Copyright (C) 1997 Technische Universitaet Braunschweig, Germany.
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

char select_rcsid[] = 
    "$Id$";

#include "select.h"

#include "x11/Delay.h"
#include "GDBAgent.h"
#include "HelpCB.h"
#include "Command.h"
#include "ddd.h"
#include "editing.h"
#include "file.h"
#include "mydialogs.h"
#include "status.h"
#include "string-fun.h"
#include "x11/verify.h"
#include "wm.h"

#include <Xm/List.h>
#include <Xm/SelectioB.h>
#include <Xm/Text.h>

#include <vector>

Widget gdb_selection_dialog = 0;
static Widget gdb_selection_list_w = 0;

static void SelectCB(Widget, XtPointer client_data, XtPointer)
{
    string& reply = *((string *)client_data);

    std::vector<int> numbers;
    getItemNumbers(gdb_selection_list_w, numbers);
    if (numbers.size() > 0)
	reply = itostring(numbers[0]) + "\n";
}

static void CancelCB(Widget, XtPointer client_data, XtPointer)
{
    string& reply = *((string *)client_data);
    reply = "\003";
}


// Answer GDB question
static void select_from_gdb(const string& question, string& reply)
{
    int count       = question.freq('\n') + 1;
    string *choices = new string[count];
    bool *selected  = new bool[count];

    split(question, choices, count, '\n');

    // Highlight choice #1 by default
    for (int i = 0; i < count; i++)
    {
	if (!has_nr(choices[i]))
	{
	    // Choice has no number (prompt) - remove it
	    for (int j = i; j < count - 1; j++)
		choices[j] = choices[j + 1];
	    count--;
	    i--;
	}
	else
	{
	    selected[i] = (get_positive_nr(choices[i]) == 1);
	}
    }

    if (count < 2)
    {
	// Nothing to choose from
	if (count == 1)
	{
	    // Take the first choice.
	    reply = itostring(atoi(choices[0].chars())) + "\n";
	}
	
	delete[] choices;
	delete[] selected;
	return;
    }

    // Popup selection dialog
    static string selection_reply;

    if (gdb_selection_dialog == 0)
    {
	Arg args[10];
	Cardinal arg = 0;
	XtSetArg(args[arg], XmNautoUnmanage, False); arg++;

	gdb_selection_dialog = 
	    verify(XmCreateSelectionDialog(find_shell(gdb_w),
					   XMST("gdb_selection_dialog"),
					   args, arg));
	Delay::register_shell(gdb_selection_dialog);

	XtUnmanageChild(XmSelectionBoxGetChild(gdb_selection_dialog,
					       XmDIALOG_TEXT));
	XtUnmanageChild(XmSelectionBoxGetChild(gdb_selection_dialog, 
					       XmDIALOG_SELECTION_LABEL));
	XtUnmanageChild(XmSelectionBoxGetChild(gdb_selection_dialog, 
					       XmDIALOG_APPLY_BUTTON));

	gdb_selection_list_w = XmSelectionBoxGetChild(gdb_selection_dialog, 
						      XmDIALOG_LIST);
	XtVaSetValues(gdb_selection_list_w,
		      XmNselectionPolicy, XmSINGLE_SELECT,
		      XtPointer(0));
	XtAddCallback(gdb_selection_dialog,
		      XmNokCallback, SelectCB, &selection_reply);
	XtAddCallback(gdb_selection_dialog,
		      XmNcancelCallback, CancelCB, &selection_reply);
	XtAddCallback(gdb_selection_dialog,
		      XmNhelpCallback, ImmediateHelpCB, 0);
    }

    setLabelList(gdb_selection_list_w, choices, selected, count, false, false);

    delete[] choices;
    delete[] selected;

    manage_and_raise(gdb_selection_dialog);

    selection_reply = "";
    while (selection_reply.empty() 
	   && gdb->running() && !gdb->isReadyWithPrompt())
	XtAppProcessEvent(XtWidgetToApplicationContext(gdb_w), XtIMAll);

    // Found a reply - return
    reply = selection_reply;
}

// Select a file
static void select_file(const string& /* question */, string& reply)
{
    gdbOpenFileCB(find_shell(), 0, 0);

    open_file_reply = "";
    while (open_file_reply.empty() 
	   && gdb->running() && !gdb->isReadyWithPrompt())
	XtAppProcessEvent(XtWidgetToApplicationContext(gdb_w), XtIMAll);

    // Found a reply - return
    reply = open_file_reply + "\n";
}

void gdb_selectHP(Agent *, void *, void *call_data)
{
    ReplyRequiredInfo *info = (ReplyRequiredInfo *)call_data;

#if 0
    if (gdb_keyboard_command)
    {
	// Use the GDB console to answer this query
	info->reply = "";
	return;
    }
#endif

    // Fetch previous output lines, in case this is a multi-line message.
    int num_chars =  XmTextGetLastPosition(gdb_w) - messagePosition;
    int buffer_size = (num_chars* MB_CUR_MAX) + 1;
    char *buffer = new char[buffer_size];
    // this works for latin1 and utf-8
    XmTextGetSubstring(gdb_w, messagePosition, num_chars, buffer_size, buffer);
    string prompt(buffer);
    delete [] buffer;

    prompt += info->question;


    // Issue prompt right now
    _gdb_out(info->question);
    info->question = "";

    // Set and issue reply
    if (prompt.contains("file name"))
    {
	// File selection
	select_file(prompt, info->reply);
    }
    else
    {
	// Option selection
	select_from_gdb(prompt, info->reply);
    }

    _gdb_out(info->reply);
}
