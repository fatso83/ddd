// $Id$ -*- C++ -*-
// Show messages in status line

// Copyright (C) 1996-1998 Technische Universitaet Braunschweig, Germany.
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

char status_rcsid[] = 
    "$Id$";

#include "status.h"

#include "Command.h"
#include "x11/Delay.h"
#include "x11/DestroyCB.h"
#include "GDBAgent.h"
#include "HelpCB.h"
#include "motif/MakeMenu.h"
#include "x11/charsets.h"
#include "cmdtty.h"		// annotate()
#include "ddd.h"
#include "x11/findParent.h"
#include "mydialogs.h"
#include "post.h"
#include "string-fun.h"
#include "x11/verify.h"

#include <ctype.h>
#include <Xm/Xm.h>
#include <Xm/Text.h>
#include <Xm/SelectioB.h>
#include <Xm/RowColumn.h>
#include <Xm/Label.h>
#include <Xm/MenuShell.h>

#include <X11/IntrinsicP.h>	// LessTif hacks

//-----------------------------------------------------------------------------
// Data
//-----------------------------------------------------------------------------

// True if last cmd came from GDB window
bool gdb_keyboard_command = false;

// True if the next line is to be displayed in the status line
bool show_next_line_in_status = false;

// True if GDB is asking `yes or no' right now
bool gdb_asks_yn;

// Current contents of status window
static MString current_status_text;

// Non-zero if status is locked (i.e. unchangeable)
static int status_locked = 0;

// True iff status is to be logged
static bool log_status = true;


//-----------------------------------------------------------------------------
// Status lock
//-----------------------------------------------------------------------------

void lock_status(void)       { status_locked++;   }
void unlock_status(void)     { if (status_locked > 0) status_locked--; }
void reset_status_lock(void) { status_locked = 0; }


//-----------------------------------------------------------------------------
// Prompt recognition
//-----------------------------------------------------------------------------

void set_buttons_from_gdb(Widget buttons, string& text)
{
    bool yn = gdb->ends_with_yn(text);

    if (yn)
    {
	if (!gdb_asks_yn)
	    annotate("query");

	gdb_asks_yn = true;
    }
    else if (gdb->isReadyWithPrompt())
    {
	if (gdb_asks_yn)
	    annotate("post-query");

	gdb_asks_yn = false;
	unpost_gdb_yn();
    }

    if (yn && !gdb_keyboard_command)
    {
        // Fetch previous output lines, in case this is a multi-line message.
        XmTextPosition pos;
        // FIXME: Handle JDB
        const char *prompt_start = (gdb->type() == XDB) ? ">" : "(";
        bool res = XmTextFindString(gdb_w, XmTextGetLastPosition(gdb_w), XMST(prompt_start), XmTEXT_BACKWARD, &pos);
        if (res)
            res = XmTextFindString(gdb_w, pos, XMST("\n"), XmTEXT_FORWARD, &pos);
        if (res)
            pos++;
        else
            pos = messagePosition;

        XmTextReplace(gdb_w, pos, XmTextGetLastPosition(gdb_w), XMST(""));
	promptPosition = pos;
        int lastpos = XmTextGetLastPosition(gdb_w);
        int num_chars =  lastpos - promptPosition;
        int buffer_size = (num_chars* MB_CUR_MAX) + 1;
        char *buffer = new char[buffer_size];
        XmTextGetSubstring(gdb_w, promptPosition, num_chars, buffer_size, buffer);
        string prompt(buffer);
        delete [] buffer;

	if (text.contains('('))
	    prompt += text.before('(', -1); // Don't repeat `(y or n)'
	else
	    prompt += text;

	post_gdb_yn(prompt);
	text = "";
	return;
    }

    if (buttons == 0)
	return;

    static bool last_yn = false;
    if (yn == last_yn)
	return;

    last_yn = yn;

    if (XtIsComposite(buttons))
    {
	set_sensitive(buttons, false);

	WidgetList children   = 0;
	Cardinal num_children = 0;

	XtVaGetValues(buttons,
		      XmNchildren, &children,
		      XmNnumChildren, &num_children,
		      XtPointer(0));

	int i;
	for (i = 0; i < int(num_children); i++)
	    XtManageChild(children[i]);
	for (i = 0; i < int(num_children); i++)
	{
	
	    Widget w = children[i];
	    string name = XtName(w);

	    if (yn == (name == "Yes" || name == "No"))
		XtManageChild(w);
	    else
		XtUnmanageChild(w);
	}

	set_sensitive(buttons, true);
    }
}


//-----------------------------------------------------------------------------
// Status history
//-----------------------------------------------------------------------------

int status_history_size = 20;
static MString *history = 0;
static int current_history = 0;

static Widget history_label = 0;
static Widget history_row   = 0;

static Widget create_status_history(Widget parent)
{
    static Widget history_shell = 0;

    if (history_shell != 0)
	return history_shell;

    Arg args[10];
    int arg;

    arg = 0;
    XtSetArg(args[arg], XmNallowShellResize, True); arg++;
    XtSetArg(args[arg], XmNwidth,            10);   arg++;
    XtSetArg(args[arg], XmNheight,           10);   arg++;
    history_shell = verify(XmCreateMenuShell(parent, XMST("status_history"), 
					     args, arg));

    arg = 0;
    XtSetArg(args[arg], XmNmarginWidth, 0);     arg++;
    XtSetArg(args[arg], XmNmarginHeight, 0);    arg++;
    XtSetArg(args[arg], XmNresizeWidth, True);  arg++;
    XtSetArg(args[arg], XmNresizeHeight, True); arg++;
    XtSetArg(args[arg], XmNborderWidth, 0);     arg++;
    XtSetArg(args[arg], XmNshadowThickness, 0); arg++;
    history_row = verify(XmCreateRowColumn(history_shell, XMST("row"), 
					   args, arg));
    XtManageChild(history_row);

    arg = 0;
    XtSetArg(args[arg], XmNresizable, True); arg++;
    XtSetArg(args[arg], XmNalignment, XmALIGNMENT_BEGINNING); arg++;
    history_label = verify(XmCreateLabel(history_row, XMST("label"), 
					 args, arg));
    XtManageChild(history_label);

    return history_shell;
}

Widget status_history(Widget parent)
{
    Widget history_shell = create_status_history(parent);

    MString history_msg;

    if (history == 0 || status_history_size == 0)
    {
	history_msg = rm("No history.");
    }
    else
    {
	history_msg = MString("Recent messages");
	history_msg += rm(" (oldest first)");
	history_msg += cr();

	int i = current_history;
	do {
	    if (!history[i].isEmpty())
	    {
		if (!history_msg.isEmpty())
		    history_msg += cr();
		history_msg += history[i];
	    }
	    i = (i + 1) % status_history_size;
	} while (i != current_history);
    }

    XtVaSetValues(history_label, XmNlabelString, history_msg.xmstring(), 
		  XtPointer(0));

    return history_shell;
}

// Return true iff S1 is a prefix of S2
static bool is_prefix(const MString& m1, const MString& m2)
{
    XmString s1 = m1.xmstring();
    XmString s2 = m2.xmstring();

    XmStringContext c1;
    XmStringContext c2;

    XmStringInitContext(&c1, s1);
    XmStringInitContext(&c2, s2);

    XmStringComponentType t1 = XmSTRING_COMPONENT_UNKNOWN;
    XmStringComponentType t2 = XmSTRING_COMPONENT_UNKNOWN;

    while (t1 != XmSTRING_COMPONENT_END && t2 != XmSTRING_COMPONENT_END)
    {
	char *s_text1            = 0;
	XmStringCharSet s_cs1    = 0;
	XmStringDirection d1     = XmSTRING_DIRECTION_DEFAULT;
	XmStringComponentType u1 = XmSTRING_COMPONENT_UNKNOWN;
	unsigned short ul1       = 0;
	unsigned char *s_uv1     = 0;
	
	t1 = XmStringGetNextComponent(c1, &s_text1, &s_cs1, &d1, 
				      &u1, &ul1, &s_uv1);

	char *s_text2            = 0;
	XmStringCharSet s_cs2    = 0;
	XmStringDirection d2     = XmSTRING_DIRECTION_DEFAULT;
	XmStringComponentType u2 = XmSTRING_COMPONENT_UNKNOWN;
	unsigned short ul2       = 0;
	unsigned char *s_uv2     = 0;

	t2 = XmStringGetNextComponent(c2, &s_text2, &s_cs2, &d2,
				      &u2, &ul2, &s_uv2);

	// Upon EOF in LessTif 0.82, XmStringGetNextComponent()
	// returns XmSTRING_COMPONENT_UNKNOWN instead of
	// XmSTRING_COMPONENT_END.  Work around this.
	if (t1 == XmSTRING_COMPONENT_UNKNOWN && s_uv1 == 0)
	    t1 = XmSTRING_COMPONENT_END;
	if (t2 == XmSTRING_COMPONENT_UNKNOWN && s_uv2 == 0)
	    t2 = XmSTRING_COMPONENT_END;

	// Place string values in strings
	string text1(s_text1 == 0 ? "" : s_text1);
	string text2(s_text2 == 0 ? "" : s_text2);
	string cs1(s_cs1 == 0 ? "" : s_cs1);
	string cs2(s_cs2 == 0 ? "" : s_cs2);
	string uv1;
	string uv2;
	if (s_uv1 != 0)
	    uv1 = string((char *)s_uv1, ul1);
	if (s_uv2 != 0)
	    uv2 = string((char *)s_uv2, ul2);

	// Free unused memory
	XtFree(s_text1);
	XtFree(s_text2);
	XtFree(s_cs1);
	XtFree(s_cs2);
	XtFree((char *)s_uv1);
	XtFree((char *)s_uv2);

	if (t1 != t2)
	{
	    goto done;		// Differing tags
	}

	switch (t1)
	{
	case XmSTRING_COMPONENT_CHARSET:
	{
	    if (cs1.empty())	// In LessTif 0.82, XmStringGetNextComponent()
		cs1 = text1;	// swaps CS and TEXT.  Work around this.
	    if (cs2.empty())
		cs2 = text2;

	    if (cs1 != cs2)
		goto done;	// Differing character sets
	    break;
	}

	case XmSTRING_COMPONENT_TEXT:
	case XmSTRING_COMPONENT_LOCALE_TEXT:
	case XmSTRING_COMPONENT_WIDECHAR_TEXT:
	{
	    if (text1.empty())	// In LessTif 0.82, XmStringGetNextComponent()
		text1 = cs1;	// swaps CS and TEXT.  Work around this.
	    if (text2.empty())
		text2 = cs2;

	    if (!text2.contains(text1, 0))
		goto done;
	    XmStringComponentType next2 = XmStringPeekNextComponent(c2);

	    // In LessTif 0.82, XmStringPeekNextComponent() returns
	    // XmSTRING_COMPONENT_UNKNOWN instead of
	    // XmSTRING_COMPONENT_END.  Work around this.
	    if (next2 != XmSTRING_COMPONENT_END && 
		next2 != XmSTRING_COMPONENT_UNKNOWN)
		goto done;
	    break;
	}

	case XmSTRING_COMPONENT_DIRECTION:
	{
	    if (d1 != d2)
		goto done;
	    break;
	}

	case XmSTRING_COMPONENT_SEPARATOR:
	case XmSTRING_COMPONENT_END:
	{
	    // These are the same by definition
	    break;
	}

	case XmSTRING_COMPONENT_UNKNOWN:
	{
	    if (uv1 != uv2)
		goto done;	// Differing unknown tags
	    break;
	}

	default:
	{
	    break;		// Skip everything else
	}
	}
    }
 done:

    XmStringFreeContext(c2);
    XmStringFreeContext(c1);

    return t1 == XmSTRING_COMPONENT_END;
}

static void add_to_status_history(const MString& message)
{
    static MString empty = rm(" ");

    if (history == 0)
	history = new MString[status_history_size];

    int last_history = 
	(status_history_size + current_history - 1) % status_history_size;

    if (message.isNull() || message.isEmpty() || message == empty)
	return;

    if (is_prefix(history[last_history], message))
    {
	history[last_history] = message;
	return;
    }

    history[current_history] = message;
    current_history = (current_history + 1) % status_history_size;
}

//-----------------------------------------------------------------------------
// Status recognition
//-----------------------------------------------------------------------------

void set_status_from_gdb(const string& text)
{
    if (private_gdb_input)
	return;

    if (!show_next_line_in_status && !text.contains(gdb->prompt(), -1))
	return;

    // fetch all full lines from GDB window
    XmTextPosition pos = messagePosition;
    XmTextPosition messageEnd = messagePosition;
    while (XmTextFindString(gdb_w, pos, XMST("\n"), XmTEXT_FORWARD, &pos))
    {
        pos++;
        messageEnd = pos;

        if (show_next_line_in_status==false)
            break;
    }

    string message;
    if (messageEnd!=messagePosition)
    {
        int num_chars =  messageEnd - messagePosition;
        int buffer_size = (num_chars* MB_CUR_MAX) + 1;
        char *buffer = new char[buffer_size];
        XmTextGetSubstring(gdb_w, messagePosition, num_chars, buffer_size, buffer);
        message = buffer;
        delete [] buffer;
        messagePosition = messageEnd;
    }

    if (message.empty() && text.contains('\n'))
	message = text;

    if (show_next_line_in_status && 
	(message.empty() || message[message.length() - 1] != '\n'))
	return;

    // Skip prompt and uncomplete lines
    int idx = message.index('\n', -1);
    if (idx >= 0)
	message = message.before(idx);

    strip_trailing_newlines(message);
    if (message.empty() && text.contains('\n'))
	message = text;

    if (show_next_line_in_status)
    {
	show_next_line_in_status = false;
	message.gsub('\n', ' ');
    }
    else
    {
	// Show first line only
	while (!message.empty() && message[0] == '\n')
	    message = message.after('\n');
	if (message.contains('\n'))
	    message = message.before('\n');
    }

    strip_trailing_newlines(message);
    message.gsub('\t', ' ');
    if (message.empty())
	return;

    // Don't log this stuff - it's already logged
    bool old_log_status = log_status;
    log_status = false;
    set_status(message);
    log_status = old_log_status;
}

// Show MESSAGE in status window.
// If TEMPORARY is set, override locks and do not add to status history.
void set_status(const string& message_, bool temporary)
{
    if (status_w == 0)
	return;

    string message(message_);
    if (message.length() > 0
	&& !message.contains("=") 
	&& isascii(message[0])
	&& islower(message[0]))
	message[0] = toupper(message[0]);

    set_status_mstring(rm(message), temporary);
}

// Same, but use an MString.
void set_status_mstring(const MString& message, bool temporary)
{
    if (status_w == 0)
	return;

    if (!temporary)
	add_to_status_history(message);

    if (!status_locked)
    {
	current_status_text = message;

	XtVaSetValues(status_w,
		      XmNlabelString, message.xmstring(),
		      XtPointer(0));
	XFlush(XtDisplay(status_w));
	XmUpdateDisplay(status_w);
    }

    if (log_status && !temporary)
    {
	// Log status message
	string s = message.str();
	if (!s.empty() && s != " ")
	{
	    dddlog << "#  " << s << "\n";
	    dddlog.flush();
	}
    }
}

const MString& current_status(void)
{
    return current_status_text;
}
