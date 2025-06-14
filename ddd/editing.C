// $Id$ -*- C++ -*-
// DDD command-line actions

// Copyright (C) 1996-1998 Technische Universitaet Braunschweig, Germany.
// Copyright (C) 2000 Universitaet Passau, Germany.
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

char editing_rcsid[] = 
    "$Id$";

#include "editing.h"

#include "AppData.h"
#include "ArgField.h"
#include "Command.h"
#include "DataDisp.h"
#include "SourceView.h"
#include "agent/TimeOut.h"
#include "args.h"
#include "cmdtty.h"
#include "complete.h"
#include "base/cook.h"
#include "ctrl.h"
#include "ddd.h"
#include "history.h"
#include "base/misc.h"
#include "post.h"
#include "regexps.h"
#include "status.h"
#include "string-fun.h"
#include "windows.h"

#include <iostream>
#include <Xm/Xm.h>
#include <Xm/Text.h>
#include <Xm/TextF.h>
#include <Xm/RowColumn.h>	// XmMenuPosition()

// ANSI C++ doesn't like the XtIsRealized() macro
#ifdef XtIsRealized
#undef XtIsRealized
#endif

// True if last input was at gdb prompt
bool gdb_input_at_prompt = false;


//-----------------------------------------------------------------------------
// Helpers
//-----------------------------------------------------------------------------

static void move_to_end_of_line(XtPointer, XtIntervalId *)
{
    XmTextPosition pos = XmTextGetLastPosition(gdb_w);
    XmTextSetInsertionPosition(gdb_w, pos);
    XmTextShowPosition(gdb_w, pos);
}

static XmTextPosition start_of_line()
{
    XmTextPosition end = XmTextGetLastPosition(gdb_w);
    XmTextPosition start;
    bool res = XmTextFindString(gdb_w, end, XMST("\n("), XmTEXT_BACKWARD, &start);
    if (res)
        return start + 1; // advance by '\n'

    res = XmTextFindString(gdb_w, end, XMST("\n>"), XmTEXT_BACKWARD, &start);
    if (res)
        return start + 1;

    String str = XmTextGetString(gdb_w);
    string s = str;
    XtFree(str);
    if (!s.contains('(', 0) && !s.contains('>', 0)) // ok for utf-8, no position needed
        return XmTextPosition(-1);

    return 0;
}


//-----------------------------------------------------------------------------
// Incremental search
//-----------------------------------------------------------------------------

enum ISearchState { ISEARCH_NONE = 0, ISEARCH_NEXT = 1, ISEARCH_PREV = -1 };
static ISearchState isearch_state = ISEARCH_NONE;
static string isearch_string = "";
static string isearch_line = "";
static bool have_isearch_line = false;
static bool isearch_motion_ok = false;

static const char *isearch_prompt         = "(i-search)";
static const char *reverse_isearch_prompt = "(reverse-i-search)";


// Return current line
string current_line()
{
    if (have_isearch_line)
	return isearch_line;

    int lastpos = XmTextGetLastPosition(gdb_w);
    int num_chars =  lastpos - promptPosition;
    int buffer_size = (num_chars* MB_CUR_MAX) + 1;
    char *buffer = new char[buffer_size];
    // this works for latin1 and utf-8
    XmTextGetSubstring(gdb_w, promptPosition, num_chars, buffer_size, buffer);
    string input(buffer);
    delete [] buffer;
    return input;
}

// Helpers
static void clear_isearch_after_motion(XtPointer, XtIntervalId *)
{
    clear_isearch(false);
}

static void set_isearch_motion_ok(XtPointer client_data, XtIntervalId *)
{
    isearch_motion_ok = bool((long)client_data);
}

// Show prompt according to current mode
static void show_isearch()
{
    XmTextPosition start = start_of_line();
    if (start == XmTextPosition(-1))
	return;

    string prompt;
    switch (isearch_state)
    {
    case ISEARCH_NONE:
	prompt = gdb->prompt();
	break;

    case ISEARCH_NEXT:
	prompt = isearch_prompt;
	break;

    case ISEARCH_PREV:
	prompt = reverse_isearch_prompt;
	break;
    }

    if (isearch_state != ISEARCH_NONE)
	prompt += "'" + cook(isearch_string) + "': ";
    string input = current_line();
    string line  = prompt + input;

    bool old_private_gdb_output = private_gdb_output;
    private_gdb_output = true;
    XmTextReplace(gdb_w, start, XmTextGetLastPosition(gdb_w), XMST(line.chars()));
    promptPosition = start + prompt.length();

    XmTextPosition pos = promptPosition;
    int index = input.index(isearch_string);
    if (isearch_state == ISEARCH_NONE || index < 0)
    {
	XmTextSetHighlight(gdb_w, 0, XmTextGetLastPosition(gdb_w),
			   XmHIGHLIGHT_NORMAL);
    }
    else
    {
	XmTextSetHighlight(gdb_w,
			   0,
			   pos + index,
			   XmHIGHLIGHT_NORMAL);
	XmTextSetHighlight(gdb_w,
			   pos + index, 
			   pos + index + isearch_string.length(),
			   XmHIGHLIGHT_SECONDARY_SELECTED);
	XmTextSetHighlight(gdb_w, 
			   pos + index + isearch_string.length(),
			   XmTextGetLastPosition(gdb_w),
			   XmHIGHLIGHT_NORMAL);
    }

    if (index >= 0)
	pos += index;

    XmTextSetInsertionPosition(gdb_w, pos);
    XmTextShowPosition(gdb_w, pos);
    have_isearch_line = false;
    private_gdb_output = old_private_gdb_output;
}

// When i-search is done, show history position given in client_data
static void isearch_done(XtPointer client_data, XtIntervalId *)
{
    int history = (int)(long)client_data;

    if (history >= 0)
    {
	bool old_private_gdb_output = private_gdb_output;
	private_gdb_output = true;
	goto_history(history);
	have_isearch_line = false;
	private_gdb_output = old_private_gdb_output;
    }

    show_isearch();
}

static void isearch_again(ISearchState new_isearch_state, XEvent *event)
{
    if (!gdb->isReadyWithPrompt())
	return;

    if (isearch_state == ISEARCH_NONE)
    	isearch_string = "";

    if (isearch_state == new_isearch_state)
    {
	// Same state - search again
	int history = search_history(isearch_string, int(isearch_state), true);
	if (history < 0)
	    XtCallActionProc(gdb_w, "beep", event, 0, 0);
	else
	    isearch_done(XtPointer(intptr_t(history)), 0);
    }
    else
    {
	isearch_state = new_isearch_state;
	show_isearch();
    }
}

// Action: enter reverse i-search
void isearch_prevAct(Widget, XEvent *event, String *, Cardinal *)
{
    isearch_again(ISEARCH_PREV, event);
}

// Action: enter forward i-search
void isearch_nextAct(Widget, XEvent *event, String *, Cardinal *)
{
    isearch_again(ISEARCH_NEXT, event);
}

// Action: exit i-search
void isearch_exitAct(Widget, XEvent *, String *, Cardinal *)
{
    clear_isearch();
}

// Exit i-search mode and return to normal mode
void clear_isearch(bool reset, bool show)
{
    if (!gdb->isReadyWithPrompt())
	return;

    if (isearch_state != ISEARCH_NONE)
    {
	isearch_state = ISEARCH_NONE;
	if (show)
	    show_isearch();

	if (reset)
	{
	    set_history_from_line(current_line());
	    goto_history();
	}
    }

    isearch_motion_ok = false;
}

void interruptAct(Widget w, XEvent*, String *, Cardinal *)
{
    if (isearch_state != ISEARCH_NONE)
    {
	clear_isearch();
    }
    else
    {
	gdb_keyboard_command = true;
	gdb_command("\003", w);
	gdb_keyboard_command = true;
    }
}

// Handle incremental searches; return true if processed
static bool do_isearch(Widget, XmTextVerifyCallbackStruct *change)
{
    if (isearch_state == ISEARCH_NONE)
	return false;

    string saved_isearch_string = isearch_string;
    bool processed = false;

    if (change->startPos == change->endPos)
    {
	// Character insertion
	string input = string(change->text->ptr, change->text->length);
	if (!input.contains('\n', -1))
	{
	    // Add to current search string
	    isearch_string += input;
	    processed = true;
	}
    }
    else if (change->endPos - change->startPos == 1)
    {
	// Backspace - remove last character from search string
	if (!isearch_string.empty())
	    isearch_string.after(int(isearch_string.length()) - 2) = "";
	else
	    clear_isearch(true, false);

	processed = true;
    }

    if (processed)
    {
	int history = -1;
	if (isearch_string.empty() || current_line().contains(isearch_string))
	{
	    // Search string found in current line
	    history = -1;
	}
	else
	{
	    history = search_history(isearch_string, int(isearch_state));
	    if (history < 0)
	    {
		// Search string not found in history
		if (change->event != 0)
		    XtCallActionProc(gdb_w, "beep", change->event, 0, 0);
		isearch_string = saved_isearch_string;
	    }
	}

	// Make this a no-op
	if (!have_isearch_line)
	{
	    isearch_line = current_line();
	    have_isearch_line = true;
	}

	// Redraw current line with appropriate prompt.
	XtAppAddTimeOut(XtWidgetToApplicationContext(gdb_w), 0, 
			isearch_done, XtPointer(intptr_t(history)));

	// Upon the next call to gdbMotionCB(), clear ISearch mode,
	// unless it immediately follows this one.
	isearch_motion_ok = true;
	XtAppAddTimeOut(XtWidgetToApplicationContext(gdb_w), 10,
			set_isearch_motion_ok, XtPointer(false));
    }

    return processed;
}


//-----------------------------------------------------------------------------
// Misc actions
//-----------------------------------------------------------------------------

static bool from_keyboard(XEvent *ev)
{
    return ev == 0 || (ev->type != ButtonPress && ev->type != ButtonRelease);
}

void controlAct(Widget w, XEvent *ev, String *params, Cardinal *num_params)
{
    clear_isearch();

    if (*num_params != 1)
    {
	std::cerr << "gdb-control: usage: gdb-control(CONTROL-CHARACTER)\n";
	return;
    }

    gdb_keyboard_command = from_keyboard(ev);
    gdb_command(ctrl(params[0]), w);
    gdb_keyboard_command = from_keyboard(ev);
}

void commandAct(Widget w, XEvent *ev, String *params, Cardinal *num_params)
{
    clear_isearch();

    if (*num_params != 1)
    {
	std::cerr << "gdb-command: usage: gdb-command(COMMAND)\n";
	return;
    }

    gdb_keyboard_command = from_keyboard(ev);
    gdb_button_command(params[0], w);
    gdb_keyboard_command = from_keyboard(ev);
}

void processAct(Widget w, XEvent *e, String *params, Cardinal *num_params)
{
    if (app_data.source_editing && w == source_view->source())
    {
	// Process event in source window
	string action = "self-insert";   // Default action
	String *action_params      = 0;
	Cardinal num_action_params = 0;
	if (num_params != 0 && *num_params > 0)
	{
	    action = params[0];
	    action_params = params + 1;
	    num_action_params = *num_params - 1;
	}

	XtCallActionProc(w, action.chars(), e, action_params, num_action_params);
	return;
    }

    if (e->type != KeyPress && e->type != KeyRelease)
	return;			// Forward only keyboard events

    if (!XtIsRealized(gdb_w))
	return;			// We don't have a console yet

    if (app_data.console_has_focus == Off)
	return;			// No forwarding

    if (app_data.console_has_focus == Auto && !have_command_window())
	return;			// The console is closed

    static bool running = false;

    if (running)
	return;			// We have already entered this procedure

    running = true;

#if 0
    clear_isearch();		// Why would this be needed?  -AZ
#endif

    // Give focus to GDB console
    XmProcessTraversal(gdb_w, XmTRAVERSE_CURRENT);

    // Forward event to GDB console
    Window old_window = e->xkey.window;
    e->xkey.window = XtWindow(gdb_w);
    XtDispatchEvent(e);
    e->xkey.window = old_window;

    // Return focus to original widget
    XmProcessTraversal(w, XmTRAVERSE_CURRENT);

    running = false;
}

void insert_source_argAct(Widget w, XEvent*, String*, Cardinal*)
{
    clear_isearch();

    string arg = source_arg->get_string();
    if (XmIsText(w)) {
	if (XmTextGetEditable(w)) {
	    XmTextPosition pos = XmTextGetInsertionPosition(w);
	    XmTextReplace(w, pos, pos, XMST(arg.chars()));
	}
    }
    else if (XmIsTextField(w)) {
	if (XmTextFieldGetEditable(w)) {
	    XmTextPosition pos = XmTextFieldGetInsertionPosition(w);
	    XmTextFieldReplace(w, pos, pos, XMST(arg.chars()));
	}
    }
}

void insert_graph_argAct (Widget w, XEvent *ev, 
			  String *args, Cardinal *num_args)
{
    // Since both fields are synchronized, it doesn't matter which one
    // we insert.
    insert_source_argAct(w, ev, args, num_args);
}

void next_tab_groupAct (Widget w, XEvent*, String*, Cardinal*)
{
    XmProcessTraversal(w, XmTRAVERSE_NEXT_TAB_GROUP);
}

void prev_tab_groupAct (Widget w, XEvent*, String*, Cardinal*)
{
    XmProcessTraversal(w, XmTRAVERSE_PREV_TAB_GROUP);
}

void get_focusAct (Widget w, XEvent*, String*, Cardinal*)
{
    XmProcessTraversal(w, XmTRAVERSE_CURRENT);
}

void select_allAct (Widget w, XEvent *e, String *params, Cardinal *num_params)
{
    switch (app_data.select_all_bindings)
    {
    case KDEBindings:
	XtCallActionProc(w, "select-all", e, params, *num_params);
	break;

    case MotifBindings:
	if (w == gdb_w)
	    XtCallActionProc(w, "gdb-beginning-of-line", 
			     e, params, *num_params);
	else
	    XtCallActionProc(w, "beginning-of-line", e, params, *num_params);
	break;
    }
}


//-----------------------------------------------------------------------------
// Editing actions
//-----------------------------------------------------------------------------

void beginning_of_lineAct(Widget, XEvent*, String*, Cardinal*)
{
    clear_isearch();
    XmTextSetInsertionPosition(gdb_w, promptPosition);
}

void end_of_lineAct(Widget, XEvent*, String*, Cardinal*)
{
    clear_isearch();
    XmTextSetInsertionPosition(gdb_w, XmTextGetLastPosition(gdb_w));
}

void forward_characterAct(Widget, XEvent *e, 
			  String *params, Cardinal *num_params)
{
    clear_isearch();
    XtCallActionProc(gdb_w, "forward-character", e, params, *num_params);
}

void backward_characterAct(Widget, XEvent*, String*, Cardinal*)
{
    clear_isearch();
    XmTextPosition pos = XmTextGetInsertionPosition(gdb_w);
    if (pos > promptPosition)
	XmTextSetInsertionPosition(gdb_w, pos - 1);
}

void set_current_line(const string& input)
{
    XmTextReplace(gdb_w, promptPosition, XmTextGetLastPosition(gdb_w), 
		  XMST(input.chars()));
}

void set_lineAct(Widget, XEvent*, String* params, Cardinal* num_params)
{
    clear_isearch();
    string input = "";
    if (num_params && *num_params > 0)
	input = params[0];
    set_current_line(input);
}

void delete_or_controlAct(Widget, XEvent *e, 
			  String *params, Cardinal *num_params)
{
    clear_isearch();
    string input = current_line();
    strip_trailing_newlines(input);
    if (input.empty())
	XtCallActionProc(gdb_w, "gdb-control", e, params, *num_params);
    else
	XtCallActionProc(gdb_w, "delete-next-character", e, params, *num_params);
}

//-----------------------------------------------------------------------------
// Popup menus
//-----------------------------------------------------------------------------

static MMDesc gdb_popup[] =
{
    {"clear_line",   MMPush, { gdbClearCB, 0 }, 0, 0, 0, 0},
    {"clear_window", MMPush, { gdbClearWindowCB, 0 }, 0, 0, 0, 0},
    MMEnd
};

void popupAct(Widget, XEvent *event, String*, Cardinal*)
{
    static Widget gdb_popup_w = 0;

    if (gdb_popup_w == 0)
    {
	gdb_popup_w = MMcreatePopupMenu(gdb_w, "gdb_popup", gdb_popup);
	MMaddCallbacks(gdb_popup);
	MMaddHelpCallback(gdb_popup, ImmediateHelpCB);
	InstallButtonTips(gdb_popup_w);
    }

    XmMenuPosition(gdb_popup_w, &event->xbutton);
    XtManageChild(gdb_popup_w);
}

//-----------------------------------------------------------------------------
// Callbacks
//-----------------------------------------------------------------------------

// Veto changes before the current input line
void gdbModifyCB(Widget gdb_w, XtPointer, XtPointer call_data)
{
    if (private_gdb_output)
	return;

    XmTextVerifyCallbackStruct *change = 
	(XmTextVerifyCallbackStruct *)call_data;

    if (do_isearch(gdb_w, change))
	return;

    clear_isearch();

    if (change->startPos < promptPosition)
    {
	// Attempt to change text before prompt
#if 0
	// This only works in LessTif.  
	// With Motif, this causes a core dump on Solaris.  - AZ
	change->doit = false;
#else
	// Make it a no-op
	XmTextPosition lastPos = XmTextGetLastPosition(gdb_w);
	XmTextPosition newPos = lastPos;

	if (change->text->length == 0)
	{
	    // Deletion
	    newPos = promptPosition;
	    if (change->event != 0)
		XtCallActionProc(gdb_w, "beep", change->event, 0, 0);
	}
	else
	{
	    // Some character
	    XtAppAddTimeOut(XtWidgetToApplicationContext(gdb_w), 0, 
			    move_to_end_of_line, XtPointer(0));
	}
	change->startPos = change->endPos = 
	    change->newInsert = change->currInsert = newPos;
#endif
	return;
    }

    // Make sure newlines are always inserted at the end of the line
    if (change->startPos == change->endPos &&
	(change->startPos < promptPosition || 
	 (change->text->length == 1 && change->text->ptr[0] == '\n')))
    {
	// Add any text at end of text window
	XmTextPosition lastPos = XmTextGetLastPosition(gdb_w);
	change->newInsert = change->startPos = change->endPos = lastPos;
	
	XtAppAddTimeOut(XtWidgetToApplicationContext(gdb_w), 0, 
			move_to_end_of_line, XtPointer(0));
    }
}

// Veto key-based cursor movements before current line
void gdbMotionCB(Widget, XtPointer, XtPointer call_data)
{
    if (private_gdb_output)
	return;

    XmTextVerifyCallbackStruct *change = 
	(XmTextVerifyCallbackStruct *)call_data;

    if (isearch_state != ISEARCH_NONE)
    {
	if (!isearch_motion_ok)
	{
	    XtAppAddTimeOut(XtWidgetToApplicationContext(gdb_w), 0, 
			    clear_isearch_after_motion, XtPointer(0));
	}
	isearch_motion_ok = false;
    }

    if (change->event != 0
	&& (change->event->type == KeyPress 
	    || change->event->type == KeyRelease))
    {
	if (change->newInsert < promptPosition)
	{
	    // We are before the current prompt: don't change the cursor
	    // position if a key was pressed.
#if 0
	    // With Motif, this causes a core dump on Solaris.  - AZ
	    change->doit = false;
#else
	    // Make it a no-op.
	    XtCallActionProc(gdb_w, "beep", change->event, 0, 0);
	    XtAppAddTimeOut(XtWidgetToApplicationContext(gdb_w), 0, 
			    move_to_end_of_line, 0);
#endif
	}
    }
}

// Send completed lines to GDB
void gdbChangeCB(Widget w, XtPointer, XtPointer)
{
    if (private_gdb_output)
	return;

    string input = current_line();

    bool at_prompt = gdb_input_at_prompt;
    if (at_prompt)
	input.gsub("\\\n", "");

    int newlines = input.freq('\n');
    string *lines = new string[newlines + 1];
    split(input, lines, newlines, '\n');

    private_gdb_input = true;

    if (newlines == 0 || (gdb_input_at_prompt && input.contains('\\', -1)))
    {
	// No newline found - line is still incomplete
	set_history_from_line(input, true);
    }
    else
    {
	// Process entered lines
	clear_isearch();
	promptPosition = XmTextGetLastPosition(w);
	for (int i = 0; i < newlines; i++)
	{
	    string cmd = lines[i];
	    tty_out(cmd + "\n");

	    if (gdb_input_at_prompt)
	    {
		if (cmd.matches(rxwhite) || cmd.empty())
		{
		    // Empty line: repeat last command
		    cmd = last_command_from_history();
		}
		else
		{
		    // Add new command to history
		    add_to_history(cmd);
		}
	    }

	    if (at_prompt)
	    {
		// We're typing at the GDB prompt: place CMD in command queue
		gdb_command(cmd, w);
	    }
	    else
	    {
		// Pass anything else right to GDB, clearing the command queue.
		clearCommandQueue();
		gdb->send_user_ctrl_cmd(cmd + "\n");
	    }
	}
    }

    private_gdb_input = false;

    delete[] lines;
}

//-----------------------------------------------------------------------------
// Callbacks
//-----------------------------------------------------------------------------

void gdbCommandCB(Widget w, XtPointer client_data, XtPointer call_data)
{
    clear_isearch();
    XmPushButtonCallbackStruct *cbs = (XmPushButtonCallbackStruct *)call_data;
    if (cbs->event == 0)
	return;

    gdb_button_command((String)client_data, w);

    gdb_keyboard_command = from_keyboard(cbs->event);
}

void gdb_button_command(const string& command, Widget origin)
{
    if (command.contains("..."))
    {
	set_current_line(command.before("...") + " ");
    }
    else
    {
	string c = command;
	c.gsub("()", source_arg->get_string());
	if (add_running_arguments(c, origin))
	    gdb_command(c, origin);
    }
}

void gdbPrevCB  (Widget w, XtPointer, XtPointer call_data)
{
    XmPushButtonCallbackStruct *cbs = (XmPushButtonCallbackStruct *)call_data;
    if (cbs->event == 0)
	return;

    Cardinal zero = 0;
    prev_historyAct(w, cbs->event, 0, &zero);
}

void gdbNextCB  (Widget w, XtPointer, XtPointer call_data)
{
    XmPushButtonCallbackStruct *cbs = (XmPushButtonCallbackStruct *)call_data;
    if (cbs->event == 0)
	return;

    Cardinal zero = 0;
    next_historyAct(w, cbs->event, 0, &zero);
}

void gdbISearchPrevCB  (Widget w, XtPointer, XtPointer call_data)
{
    XmPushButtonCallbackStruct *cbs = (XmPushButtonCallbackStruct *)call_data;
    if (cbs->event == 0)
	return;

    Cardinal zero = 0;
    isearch_prevAct(w, cbs->event, 0, &zero);
}

void gdbISearchNextCB  (Widget w, XtPointer, XtPointer call_data)
{
    XmPushButtonCallbackStruct *cbs = (XmPushButtonCallbackStruct *)call_data;
    if (cbs->event == 0)
	return;

    Cardinal zero = 0;
    isearch_nextAct(w, cbs->event, 0, &zero);
}

void gdbISearchExitCB  (Widget w, XtPointer, XtPointer call_data)
{
    XmPushButtonCallbackStruct *cbs = (XmPushButtonCallbackStruct *)call_data;
    if (cbs->event == 0)
	return;

    Cardinal zero = 0;
    isearch_exitAct(w, cbs->event, 0, &zero);
}

void gdbClearCB  (Widget, XtPointer, XtPointer)
{
    set_current_line("");
}

// Remove any text up to the last GDB prompt
void gdbClearWindowCB(Widget, XtPointer, XtPointer)
{
    XmTextPosition start = start_of_line();
    if (start == XmTextPosition(-1))
	return;

    private_gdb_output = true;

    XmTextReplace(gdb_w, 0, start, XMST(""));

    promptPosition  -= start;
    messagePosition -= start;
    XmTextSetInsertionPosition(gdb_w, XmTextGetLastPosition(gdb_w));

    private_gdb_output = false;
}

void gdbCompleteCB  (Widget w, XtPointer, XtPointer call_data)
{
    if (!gdb->isReadyWithPrompt())
    {
	post_gdb_busy(w);
	return;
    }

    clear_isearch();
    XmPushButtonCallbackStruct *cbs = (XmPushButtonCallbackStruct *)call_data;
    if (cbs->event == 0)
	return;

    Cardinal zero = 0;
    end_of_lineAct(gdb_w, cbs->event, 0, &zero);
    complete_commandAct(gdb_w, cbs->event, 0, &zero);
}

// Use this for push buttons
void gdbApplyCB(Widget w, XtPointer, XtPointer call_data)
{
    if (!gdb->isReadyWithPrompt())
    {
	post_gdb_busy(w);
	return;
    }

    clear_isearch();
    XmPushButtonCallbackStruct *cbs = (XmPushButtonCallbackStruct *)call_data;
    if (cbs->event == 0)
	return;

    Cardinal zero = 0;
    end_of_lineAct(gdb_w, cbs->event, 0, &zero);
    XtCallActionProc(gdb_w, "process-return", cbs->event, 0, zero);
}

// Use this for selection boxes
void gdbApplySelectionCB(Widget w, XtPointer, XtPointer call_data)
{
    if (!gdb->isReadyWithPrompt())
    {
	post_gdb_busy(w);
	return;
    }

    clear_isearch();
    XmSelectionBoxCallbackStruct *cbs = 
	(XmSelectionBoxCallbackStruct *)call_data;
    if (cbs->event == 0)
	return;

    Cardinal zero = 0;
    end_of_lineAct(gdb_w, cbs->event, 0, &zero);
    XtCallActionProc(gdb_w, "process-return", cbs->event, 0, zero);
}
