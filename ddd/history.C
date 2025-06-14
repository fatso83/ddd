// $Id$ -*- C++ -*-
// DDD command history

// Copyright (C) 1996-1998 Technische Universitaet Braunschweig, Germany.
// Copyright (C) 2000 Universitaet Passau, Germany.
// Copyright (C) 2003 Free Software Foundation, Inc.
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

// ------------------------------------------------------
// Yesterday
// (sung to Yesterday by the Beatles)
// 
// Yesterday, all my code still worked this way
// Now it looks as though bugs are here to stay
// Oh, I believe in yesterday.
// 
// Suddenly, it's not half as good as it used to be,
// There's a release hanging over me.
// Oh, failure came suddenly.
// 
// Why it had to break I don't know it wouldn't say.
// I typed something wrong, now I long for yesterday.
// 
// Yesterday, coding was a kind of game to play.
// Now I need a place to hide away.
// Oh, I believe in yesterday.
//
// -- Ralf Hildebrandt <Ralf.Hildebrandt@gmx.de>
// ------------------------------------------------------

char history_rcsid[] = 
    "$Id$";

#include "history.h"

#include "AppData.h"
#include "template/Assoc.h"
#include "motif/ComboBox.h"
#include "Command.h"
#include "x11/Delay.h"
#include "x11/DestroyCB.h"
#include "GDBAgent.h"
#include "HelpCB.h"
#include "motif/MString.h"
#include "motif/MakeMenu.h"
#include "SmartC.h"
#include "SourceView.h"
#include "args.h"
#include "base/cook.h"
#include "base/cwd.h"
#include "ddd.h"
#include "disp-read.h"
#include "editing.h"
#include "filetype.h"
#include "base/home.h"
#include "logo.h"
#include "base/misc.h"
#include "mydialogs.h"
#include "post.h"
#include "regexps.h"
#include "shell.h"
#include "status.h"
#include "string-fun.h"
#include "base/uniquify.h"
#include "x11/verify.h"
#include "wm.h"
#include "base/strclass.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <fstream>
#include <vector>

#include <Xm/Xm.h>
#include <Xm/Text.h>
#include <Xm/List.h>
#include <Xm/SelectioB.h>

#if WITH_READLINE
// `history.h' has no complete declaration for `add_history',
// so we install our own.
#define add_history old_add_history
extern "C" {
#include "readline/history.h"
}
#undef add_history
extern "C" void add_history(const char *line);
#endif

#ifndef ARG_MAX
#define ARG_MAX 4096
#endif


//-----------------------------------------------------------------------------
// Command history
//-----------------------------------------------------------------------------

// History viewer
static Widget gdb_history_w  = 0;
static Widget gdb_commands_w = 0;

// History storage
static std::vector<string> gdb_history;

// Index to current history entry
static int gdb_current_history;

// File name to save the history
static string _gdb_history_file;

// Size of saved history
static int gdb_history_size = 100;

// True if the history was just loaded
static bool gdb_new_history = true;

// True if history command was issued
static bool private_gdb_history = false;

// Update all combo boxes
static void update_combo_boxes();

// Update combo boxes listening to NEW_ENTRY
static void update_combo_boxes(const string& new_entry);


#if WITH_READLINE
// Initialize history
struct HistoryInitializer {
    HistoryInitializer()
    {
	using_history();
    }
};

static HistoryInitializer history_initializer;
#endif


void set_gdb_history_file(const string& file)
{
    _gdb_history_file = file;
}

string gdb_history_file()
{
    return _gdb_history_file;
}

static void set_line_from_history()
{
    private_gdb_history = true;

    const string& input = gdb_history[gdb_current_history];
    XmTextReplace(gdb_w, promptPosition,
		  XmTextGetLastPosition(gdb_w), XMST(input.chars()));
    XmTextSetInsertionPosition(gdb_w, XmTextGetLastPosition(gdb_w));

    if (gdb_history_w)
	ListSetAndSelectPos(gdb_commands_w, gdb_current_history + 1);

    private_gdb_history = false;
}

void set_history_from_line(const string& line,
			   bool ignore_history_commands)
{
    if (ignore_history_commands && private_gdb_history)
	return;

    while (gdb_history.size() < 1)
	gdb_history.push_back("");
    gdb_history[gdb_history.size() - 1] = line;

    if (gdb_history_w)
    {
	int pos = gdb_history.size();

	// XmListReplaceItemsPos() disturbs the current selection, so
	// save it here
	int *selected;
	int selected_count;
	if (!XmListGetSelectedPos(gdb_commands_w, &selected, &selected_count))
	    selected = 0;

	MString xm_line(line, LIST_CHARSET);
	XmString xms = xm_line.xmstring();
	XmListReplaceItemsPos(gdb_commands_w, &xms, 1, pos);

	if (selected)
	{
	    // Restore old selection
	    for (int i = 0; i < selected_count; i++)
		XmListSelectPos(gdb_commands_w, selected[i], False);
	    XtFree((char *)selected);
	}
    }
}

// Enter LINE in history
void add_to_history(const string& line)
{
    if (!gdb->isReadyWithPrompt())
	return;

    set_history_from_line(line);

    if (gdb_history.size() < 2 || line != gdb_history[gdb_history.size() - 2])
    {
	gdb_history.push_back("");

	if (gdb_history_w)
	{
	    MString xm_line(line, LIST_CHARSET);
	    int pos = gdb_history.size();
	    XmListAddItem(gdb_commands_w, xm_line.xmstring(), pos - 1);
	    XmListSelectPos(gdb_commands_w, 0, False);
	    XmListSetBottomPos(gdb_commands_w, 0);
	}
    }

    gdb_current_history = gdb_history.size();
    set_history_from_line("");

    if (gdb_history_w)
    {
	XmListSelectPos(gdb_commands_w, 0, False);
	XmListSetBottomPos(gdb_commands_w, 0);
    }

    gdb_new_history = false;

    add_to_arguments(line);
    update_arguments();
    update_combo_boxes(line);

#if WITH_READLINE
    add_history(line.chars());
#endif
}

// Load history from history file
void load_history(const string& file)
{
    if (file.empty())
	return;

    // Delay d;

    std::ifstream is(file.chars());
    if (is.bad())
	return;

    gdb_history.clear();

#if WITH_READLINE
    clear_history();
#endif

    assert(gdb_history.size() == 0);

    // If the first command in history is a `file' command,
    // insert them all and disregard later ones.
    bool first_command = true;
    bool first_is_file = false;
    bool get_files     = true;

    while (is)
    {
	// We accept lines up to a length of ARG_MAX (the maximum
	// length of the ARGV argument passed to a program)
	char _line[ARG_MAX + BUFSIZ];
	_line[0] = '\0';

	is.getline(_line, sizeof(_line));
	if (_line[0] != '\0')
	{
	    string line(_line);

	    bool added = false;
	    if (is_file_cmd(line, gdb) && line != "# reset")
	    {
		if (first_command)
		    first_is_file = true;

		if (get_files)
		{
		    string arg = line.after(rxwhite);
		    if (gdb->type() == PERL)
		    {
			arg = arg.after(" -d ");
			arg = arg.before('\"');
			if (arg.contains(rxwhite))
			    arg = arg.before(rxwhite);
		    }
		    add_to_recent(arg);
		    added = first_is_file;
		}
	    }
	    else
	    {
		if (first_is_file)
		    get_files = false;
		if (first_command)
		    first_is_file = false;
	    }

	    if (!added && line[0] != '#')
	    {
		gdb_history.push_back(line);
		add_to_arguments(line);

#if WITH_READLINE
		add_history(line.chars());
#endif
	    }

	    first_command = false;
	}
    }

    gdb_history.push_back("");
    gdb_current_history = gdb_history.size() - 1;
    gdb_new_history = true;

    update_arguments();
    update_combo_boxes();
}

// Save history into history file
void save_history(const string& file, Widget origin)
{
    if (!file.empty())
    {
	StatusDelay delay("Saving history in " + quote(file));
	std::ofstream os(file.chars());
	if (os.bad())
	{
	    post_error("Cannot save history in " + quote(file),
		       "history_save_error", origin);
	    delay.outcome = "failed";
	    return;
	}

	// Save the 10 most recently opened files
	int i;
	std::vector<string> recent;
	get_recent(recent);
	for (i = recent.size() - 1; i >= 0 && i >= int(recent.size()) - 10; i--)
	    os << gdb->debug_command(recent[i]) << "\n";

	// Now save the command history itself
	int start = gdb_history.size() - gdb_history_size;
	if (start < 0)
	    start = 0;

	for (i = start; i < int(gdb_history.size()); i++)
	    os << gdb_history[i] << "\n";
    }
}

// Set history file name
void process_history_filename(string answer)
{
    answer = answer.after('"');
    answer = answer.before('"');
    set_gdb_history_file(answer);
}

// Set history size
void process_history_size(string answer)
{
    answer = answer.from(rxint);
    int ret = get_positive_nr(answer);
    if (ret >= 0)
	gdb_history_size = ret;
}

// History viewer
static void SelectHistoryCB(Widget, XtPointer, XtPointer call_data)
{
    XmListCallbackStruct *cbs = (XmListCallbackStruct *)call_data;
    gdb_current_history = cbs->item_position - 1;

    clear_isearch();
    set_line_from_history();
}

static void HistoryDestroyedCB(Widget, XtPointer client_data, XtPointer)
{
    Widget old_gdb_history_w = Widget(client_data);
    if (gdb_history_w == old_gdb_history_w)
    {
	gdb_history_w = 0;
	gdb_commands_w = 0;
    }
}

void gdbHistoryCB(Widget w, XtPointer, XtPointer)
{
    if (gdb_history_w)
    {
	manage_and_raise(gdb_history_w);
	return;
    }

    Arg args[10];
    int arg;
	
    // Create history viewer
    arg = 0;
    gdb_history_w =
	verify(createTopLevelSelectionDialog(find_shell(w), "history_dialog", 
					     args, arg));
    Delay::register_shell(gdb_history_w);

    XtUnmanageChild(XmSelectionBoxGetChild(gdb_history_w, 
					   XmDIALOG_OK_BUTTON));
    XtUnmanageChild(XmSelectionBoxGetChild(gdb_history_w, 
					   XmDIALOG_TEXT));
    XtUnmanageChild(XmSelectionBoxGetChild(gdb_history_w, 
					   XmDIALOG_SELECTION_LABEL));

    gdb_commands_w = XmSelectionBoxGetChild(gdb_history_w, XmDIALOG_LIST);
    XtVaSetValues(gdb_commands_w,
		  XmNselectionPolicy, XmSINGLE_SELECT,
		  XtPointer(0));

    XtAddCallback(gdb_commands_w,
		  XmNsingleSelectionCallback, SelectHistoryCB, 0);
    XtAddCallback(gdb_commands_w,
		  XmNmultipleSelectionCallback, SelectHistoryCB, 0);
    XtAddCallback(gdb_commands_w,
		  XmNextendedSelectionCallback, SelectHistoryCB, 0);
    XtAddCallback(gdb_commands_w,
		  XmNbrowseSelectionCallback, SelectHistoryCB, 0);

    XtAddCallback(gdb_history_w, XmNokCallback, gdbApplySelectionCB, 0);
    XtAddCallback(gdb_history_w, XmNapplyCallback, gdbApplySelectionCB, 0);
    XtAddCallback(gdb_history_w, XmNcancelCallback, DestroyThisCB, 
		  gdb_history_w);
    XtAddCallback(gdb_history_w, XmNhelpCallback,  ImmediateHelpCB, 0);
    XtAddCallback(gdb_history_w, XmNdestroyCallback,
		  HistoryDestroyedCB, XtPointer(gdb_history_w));

    bool *selected = new bool[gdb_history.size() + 1];
    for (int i = 0; i < int(gdb_history.size()) + 1; i++)
	selected[i] = false;
    selected[gdb_current_history] = true;

    setLabelList(gdb_commands_w, gdb_history.data(), 
		 selected, gdb_history.size(), false, false);

    delete[] selected;

    set_history_from_line(current_line());
    XmListSelectPos(gdb_commands_w, 0, False);
    XmListSetBottomPos(gdb_commands_w, 0);

    manage_and_raise(gdb_history_w);
}


// Return last command
string last_command_from_history()
{
    if (gdb_history.size() >= 2 && !gdb_new_history)
    {
	return gdb_history[gdb_history.size() - 2];
    }
    else
    {
	// No history yet -- perform a no-op command
	return gdb->echo_command("");
    }
}

// History actions
void prev_historyAct(Widget, XEvent*, String*, Cardinal*)
{
    if (gdb_current_history == 0)
	return;

    while (gdb_current_history >= int(gdb_history.size()))
	gdb_current_history--;
    gdb_current_history--;

    clear_isearch();
    set_line_from_history();
}

void next_historyAct(Widget, XEvent*, String*, Cardinal*)
{
    if (gdb_current_history >= int(gdb_history.size()) - 1)
	return;

    gdb_current_history++;

    clear_isearch();
    set_line_from_history();
}

// Search TEXT in history; return POS iff found, -1 if none
// DIRECTION = -1 means search backward, DIRECTION = +1 means search forward
// RESEARCH = true means skip current position
int search_history(const string& text, int direction, bool research)
{
    int i = min(gdb_current_history, gdb_history.size() - 1);
    if (research)
	i += direction;

    while (i < int(gdb_history.size()) && i >= 0)
    {
	if (gdb_history[i].contains(text))
	    return i;

	i += direction;
    }

    return -1;			// Not found
}

// Set history position to POS; -1 means last position.
void goto_history(int pos)
{
    if (pos == -1)
	pos = gdb_history.size() - 1;

    assert (pos >= 0 && pos < int(gdb_history.size()));
    gdb_current_history = pos;
    set_line_from_history();
}


//-----------------------------------------------------------------------------
// Combo Box Histories
//-----------------------------------------------------------------------------

// Update combo box containing TEXT
static void update_combo_box(Widget text, HistoryFilter filter)
{
    std::vector<string> entries;

    for (int i = gdb_history.size() - 1; i >= 0; i--)
    {
	const string& entry = gdb_history[i];
	string arg = filter(entry);
	if (arg.empty())
	    continue;		// Empty arg

	if (entries.size() > 0 && entries[entries.size() - 1] == arg)
	    continue;		// Adjacent duplicate

	if (app_data.popdown_history_size > 0 &&
	    int(entries.size()) >= int(app_data.popdown_history_size))
	    break;		// Enough entries

	bool found_duplicate = false;
	if (!app_data.sort_popdown_history)
	{
	    // We'll not be sorted.  If we already have this value,
	    // ignore new entry.
	    for (int j = 0; !found_duplicate && j < int(entries.size()); j++)
		found_duplicate = (entries[j] == arg);
	}
	if (!found_duplicate)
	    entries.push_back(arg);
    }

    if (app_data.sort_popdown_history)
    {
	smart_sort(entries);
	uniq(entries);
    }

    ComboBoxSetList(text, entries);
}

static WidgetHistoryFilterAssoc combo_boxes;

// Update all combo boxes
static void update_combo_boxes()
{
    for (WidgetHistoryFilterAssocIter iter(combo_boxes); iter.ok(); ++iter)
	update_combo_box(iter.key(), iter.value());
}

// Update combo boxes listening to NEW_ENTRY
static void update_combo_boxes(const string& new_entry)
{
    for (WidgetHistoryFilterAssocIter iter(combo_boxes); iter.ok(); ++iter)
    {
	HistoryFilter filter = iter.value();
	string arg = filter(new_entry);
	if (!arg.empty())
	    update_combo_box(iter.key(), iter.value());
    }
}

static void RemoveComboBoxCB(Widget text, XtPointer, XtPointer)
{
    combo_boxes.remove(text);
}

// Tie a ComboBox to global history
void tie_combo_box_to_history(Widget text, HistoryFilter filter)
{
    combo_boxes[text] = filter;
    update_combo_box(text, filter);
    XtAddCallback(text, XmNdestroyCallback, RemoveComboBoxCB, XtPointer(0));
}


//-----------------------------------------------------------------------------
// Recent file history
//-----------------------------------------------------------------------------

static void update_recent_menus();

// Recent files storage
static std::vector<string> recent_files;

// Add FILE to recent file history
void add_to_recent(const string& file)
{
    string full_path;
    if (gdb->type() != JDB)
	full_path = SourceView::full_path(file);
    else
	full_path = file;

    if (recent_files.size() > 0 && 
	recent_files[recent_files.size() - 1] == full_path)
	return;			// Already in list

    for (int i = 0; i < int(recent_files.size()); i++)
	if (recent_files[i] == full_path ||
	    (!remote_gdb() && 
	     gdb->type() != JDB &&
	     same_file(recent_files[i], full_path)))
	    recent_files[i] = ""; // Clear old entry

    recent_files.push_back(full_path);
    update_recent_menus();
}

// Get recent file history (most recent first)
void get_recent(std::vector<string>& arr)
{
    for (int i = recent_files.size() - 1; i >= 0; i--)
    {
	string& recent = recent_files[i];
	if (recent.empty())
	    continue;		// Removed

	arr.push_back(recent_files[i]);
    }
}


// Menus to be updated
static std::vector<const MMDesc*> menus;

static void update_recent_menu(const MMDesc *items)
{
    std::vector<string> recent_files;
    {
	std::vector<string> r;
	get_recent(r);
	for (int i = 0; i < int(r.size()) && items[i].widget != 0; i++)
	    recent_files.push_back(r[i]);
    }

    // Uniquify labels
    char sep = '/';
    if (gdb->type() == JDB)
	sep = '.';

    std::vector<string> labels;
    uniquify(recent_files, labels, sep);

    // Set labels
    int i;
    for (i = 0; i < int(labels.size()); i++)
    {
	MString label(itostring(i + 1) + " ");
	label += tt(labels[i]);

	Widget w = items[i].widget;
	set_label(w, label);

	const string& file = recent_files[i];

	bool sens = true;
	if (!remote_gdb())
	{
	    if (gdb->has_exec_files() && !is_debuggee_file(file))
		sens = false;	// File not accessible
	    else if (!gdb->has_classes() && !is_regular_file(file))
		sens = false;	// File not accessible
	}

	set_sensitive(w, sens);
	XtManageChild(w);
    }

    // Unmanage remaining items
    for (; items[i].widget != 0; i++)
	XtUnmanageChild(items[i].widget);
}

static void update_recent_menus()
{
    for (int i = 0; i < int(menus.size()); i++)
    {
	const MMDesc *items = menus[i];
	update_recent_menu(items);
    }
}

void tie_menu_to_recent_files(MMDesc *items)
{
    if (items == 0 || items[0].widget == 0)
	return;

    menus.push_back(items);
    update_recent_menu(items);
}

