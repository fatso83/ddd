// $Id$ -*- C++ -*-
// DDD session management

// Copyright (C) 1997 Technische Universitaet Braunschweig, Germany.
// Copyright (C) 2001 Universitaet des Saarlandes, Germany.
// Copyright (C) 2001-2025 Free Software Foundation, Inc.
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

char session_rcsid[] = 
    "$Id$";

#include "session.h"

#include "config.h"

#if XtSpecificationRelease >= 6
#include <X11/SM/SM.h>
#endif

#include <sys/types.h>
#include <stdlib.h>		// putenv(), getenv(), getuid()
#include <stdio.h>
#include <signal.h>		// kill()
#include <string.h>		// strerror()
#include <errno.h>
#include <fstream>
#include <pwd.h>

#include "AppData.h"
#include "DataDisp.h"
#include "x11/Delay.h"
#include "x11/DestroyCB.h"
#include "x11/ExitCB.h"
#include "GDBAgent.h"
#include "HelpCB.h"
#include "motif/MakeMenu.h"
#include "SmartC.h"
#include "SourceView.h"
#include "x11/charsets.h"
#include "Command.h"
#include "base/basename.h"
#include "comm-manag.h"
#include "disp-read.h"
#include "ddd.h"
#include "base/cook.h"
#include "editing.h"
#include "exit.h"
#include "file.h"
#include "filetype.h"
#include "base/glob.h"
#include "history.h"
#include "base/home.h"
#include "hostname.h"
#include "mydialogs.h"
#include "options.h"
#include "post.h"
#include "question.h"
#include "settings.h"
#include "status.h"
#include "string-fun.h"
#include "base/tabs.h"
#include "x11/verify.h"
#include "version.h"
#include "windows.h"
#include "wm.h"
#include "config_manager.h"
#include "config_data.h"

#include <Xm/Xm.h>
#include <Xm/List.h>
#include <Xm/MessageB.h>
#include <Xm/SelectioB.h>
#include <Xm/PushB.h>
#include <Xm/ToggleB.h>
#include <Xm/Text.h>
#include <Xm/TextF.h>

extern "C" {
#include <sys/types.h>
#include <unistd.h>
#include <ctype.h>

#if HAVE_SYS_STAT_H
#include <sys/stat.h>		// mkdir()
#endif
}

#ifndef S_IRWXU
#define S_IRWXU 0700
#endif

#ifndef S_IRWXG
#define S_IRWXG 070
#endif

#ifndef S_IRWXO
#define S_IRWXO 07
#endif

#if !HAVE_PUTENV_DECL
extern "C" int putenv(char *string);
#endif

// ---------------------------------------------------------------------------
// How to present a default session to the user
// ---------------------------------------------------------------------------

const string NO_SESSION = "[none]";


// ---------------------------------------------------------------------------
// Session files
// ---------------------------------------------------------------------------

string session_state_dir()
{
    const char *ddd_state = getenv(DDD_NAME "_STATE");
    if (ddd_state != 0)
	return ddd_state;
    else
	return string(gethome()) + "/." ddd_NAME;
}

static string session_base_dir()
{
    const char *ddd_sessions = getenv(DDD_NAME "_SESSIONS");
    if (ddd_sessions != 0)
	return ddd_sessions;
    else
	return session_state_dir() + "/sessions";
}

static string session_dir(const string& session)
{
    if (session == DEFAULT_SESSION)
	return session_state_dir();
    else
	return session_base_dir() + "/" + session;
}

string session_file(const string& session, const char *base)
{
    return session_dir(session) + "/" + base;
}


// ---------------------------------------------------------------------------
// Session file creation
// ---------------------------------------------------------------------------

class StreamAction {
private:
    std::ostream& stream;
    string action;
    bool ok;

public:
    StreamAction(std::ostream& os, const string& c)
	: stream(os), action(c), ok(true)
    {
	stream << action << "...\n";
    }
    ~StreamAction()
    {
	if (ok)
	    stream << action << "...done.\n";
    }
    void failed(const string& msg)
    {
	stream << action << " failed: " << msg << "\n";
	ok = false;
    }
};

static int makedir(const string& name, std::ostream& msg, bool user_only = false)
{
    StreamAction action(msg, "Creating " + quote(name + "/"));

    mode_t mask = umask(0);
    umask(mask);
    mode_t mode;
    if (user_only)
	mode = S_IRWXU & ~mask;
    else
	mode = (S_IRWXU | S_IRWXG | S_IRWXO) & ~mask;
    int ret = mkdir(name.chars(), mode);

    if (ret != 0)
	action.failed(strerror(errno));

    return ret;
}

static void copy(const string& from_name, const string& to_name, std::ostream& msg)
{
    FILE *from = fopen(from_name.chars(), "r");
    if (from == 0)
	return;			// Don't care

    StreamAction action(msg, "Copying " + quote(from_name) 
			+ " to " + quote(to_name));

    FILE *to = fopen(to_name.chars(), "w");
    if (to == 0)
    {
	action.failed(strerror(errno));
        fclose(from);
	return;
    }

    int c;
    while ((c = getc(from)) != EOF)
	putc(c, to);

    fclose(from);
    if (fclose(to) == EOF)
    {
	action.failed(strerror(errno));
	unlink(to_name.chars());
    }
}

// True if SESSION is a saved session
bool is_saved_session(const string& session)
{
    return session == DEFAULT_SESSION || is_directory(session_dir(session));
}

// True if SESSION is temporary
bool is_temporary_session(const string& session)
{
    return is_saved_session(session) && 
	is_regular_file(session_tmp_flag(session));
}

// True if SESSION is temporary
void set_temporary_session(const string& session, bool temporary)
{
    if (temporary)
    {
        const string s1 = session_tmp_flag(session); 
	std::ofstream os(s1.chars());
	os << "This session will be deleted unless saved explicitly.\n";
    }
    else
    {
      const string s1 = session_tmp_flag(session.chars());
	unlink(s1.chars());
    }
}

// Create DDD state directory
static void create_session_state_dir(std::ostream& msg)
{
    // Create or find state directory
    if (!is_directory(session_state_dir()) && 
	makedir(session_state_dir(), msg, true) == 0)
    {
	// Check for DDD 2.1 `~/.dddinit' and `~/.ddd_history' files; 
	// copy them to new location if needed
	copy(string(gethome()) + "/.dddinit",
	     session_state_file(DEFAULT_SESSION), msg);
	copy(string(gethome()) + "/.ddd_history",
	     session_history_file(DEFAULT_SESSION), msg);
    }

    // Create session base directory
    if (!is_directory(session_base_dir()))
	makedir(session_base_dir(), msg);

    // Create themes directory
    if (!is_directory(session_themes_dir()))
	makedir(session_themes_dir(), msg);
}

// Create session directory
void create_session_dir(const string& session, std::ostream& msg)
{
    // Create state directory
    create_session_state_dir(msg);

    // Create session directory
    if (session != DEFAULT_SESSION 
	&& is_directory(session_base_dir())
	&& !is_directory(session_dir(session))
	&& makedir(session_dir(session), msg) == 0)
    {
	// Check for default session state, history, settings files;
	// copy them to new location
	copy(session_state_file(DEFAULT_SESSION), 
	     session_state_file(session), msg);
	copy(session_history_file(DEFAULT_SESSION), 
	     session_history_file(session), msg);
	copy(session_settings_file(DEFAULT_SESSION), 
	     session_settings_file(session), msg);
    }
}

// Same, but issue messages in status line
void create_session_dir(const string& session)
{
    std::ostringstream messages;
    create_session_dir(session, messages);
    string msg(messages);
    while (!msg.empty())
    {
	string line = msg.before('\n');
	set_status(line);
	msg = msg.after('\n');
    }
}


// ---------------------------------------------------------------------------
// Session locks
// ---------------------------------------------------------------------------

bool lock_session_dir(Display *display,
		      const string& session, 
		      LockInfo& info)
{
    info.pid = 0;
    string lock_file = session_lock_file(session);

    {
	std::ifstream is(lock_file.chars());
	if (!is.bad())
	{
	    string version;
	    is >> version;

	    if (version.contains(DDD_NAME, 0))
	    {
		// Lock already exists -- use contents as diagnostics
		is >> info.hostname
		   >> info.pid 
		   >> info.display 
		   >> info.uid
		   >> info.username;

		if (info.username.empty())
		{
		    // DDD 3.0.90 and earlier do not save the user name
		    info.username = itostring(info.uid);
		}

		if (info.hostname != fullhostname())
		    return false;	// Process running on remote host

		if (info.pid > 0 && kill(info.pid, 0) == 0)
		    return false;	// Process running on host
	    }
	}
    }

    // Lock session dir, possibly overriding old lock
    {
	string username;
	struct passwd *pw = getpwuid(getuid());
	if (pw != 0)
	    username = pw->pw_name;
	else
	    username = itostring(getuid());

	std::ofstream os(lock_file.chars());
	os << DDD_NAME "-" DDD_VERSION
	   << " " << fullhostname()
	   << " " << getpid()
	   << " " << XDisplayString(display)
	   << " " << getuid()
	   << " " << username
	   << "\n";
    }

    return true;
}

bool unlock_session_dir(const string& session)
{
    string lock_file = session_lock_file(session);

    std::ifstream is(lock_file.chars());
    if (!is.bad())
    {
	// There is a lock -- check whether it's ours
	LockInfo info;
	string version;
	is >> version 
	   >> info.hostname
	   >> info.pid;

	if (info.hostname == fullhostname() && info.pid == getpid())
	{
	    // It's ours -- remove it
	    is.close();
	    unlink(lock_file.chars());
	    return true;
	}
	else
	{
	    return false;	// Mismatched lock
	}
    }

    return false;		// No lock
}


// ---------------------------------------------------------------------------
// Session selection
// ---------------------------------------------------------------------------

static void get_sessions(std::vector<string>& arr)
{
    string mask = session_state_file("*");
    char **files = glob_filename(mask.chars());
    if (files == (char **)0)
    {
	std::cerr << mask << ": glob failed\n";
    }
    else if (files == (char **)-1)
    {
	if (errno != 0)
	    post_error(string(mask) + ": " + strerror(errno), 
		       "no_sessions_error");
    }
    else
    {
	int count;
	for (count = 0; files[count] != 0; count++)
	    ;
	smart_sort(files, count);

	for (int i = 0; i < count; i++)
	{
	    string file = files[i];
	    free(files[i]);
	    file = file.before('/', -1);
	    file = file.after('/', -1);
	    arr.push_back(file);
	}
	free((char *)files);
    }
}

// Update state of `delete' button
static void update_delete(Widget dialog)
{
    Widget sessions = XmSelectionBoxGetChild(dialog, XmDIALOG_LIST);

    // Check for `Delete' button
    XmStringTable selected_items;
    int selected_items_count = 0;
    XtVaGetValues(sessions,
		  XmNselectedItemCount, &selected_items_count,
		  XmNselectedItems, &selected_items,
		  XtPointer(0));

    bool sensitive = false;
    for (int i = 0; i < selected_items_count; i++)
    {
	String _item;
	XmStringGetLtoR(selected_items[i], LIST_CHARSET, &_item);
	string item(_item);
	XtFree(_item);

	if (item != NO_SESSION)
	{
	    sensitive = true;
	    break;
	}
    }

    Widget delete_w = XmSelectionBoxGetChild(dialog, XmDIALOG_APPLY_BUTTON);
    set_sensitive(delete_w, sensitive);
}

// Update list of sessions
static void update_sessions(Widget dialog)
{
    Widget sessions = XmSelectionBoxGetChild(dialog, XmDIALOG_LIST);

    std::vector<string> session_list;
    session_list.push_back(NO_SESSION);
    get_sessions(session_list);
	
    bool *selected = new bool[session_list.size()];
    int i;
    for (i = 0; i < int(session_list.size()); i++)
    {
	selected[i] = (session_list[i] == app_data.session
		       || (session_list[i] == NO_SESSION 
			   && app_data.session == DEFAULT_SESSION));
    }

    setLabelList(sessions, session_list.data(),
		 selected, session_list.size(), false, false);

    delete[] selected;

    update_delete(dialog);
}

#if 0
// Update list of sessions
static void UpdateSessionsCB(Widget dialog, XtPointer, XtPointer)
{
    update_sessions(dialog);
}
#endif

// Set argument from selected list item
static void SelectSessionCB(Widget sessions,
			    XtPointer client_data, XtPointer call_data)
{
    Widget dialog = Widget(client_data);
    XmListCallbackStruct *cbs = (XmListCallbackStruct *)call_data;

    // Update argument field
    if (cbs->selected_item_count == 1)
    {
	int pos = cbs->item_position;
	ListSetAndSelectPos(sessions, pos);

	String value_s;
	XmStringGetLtoR(cbs->item, LIST_CHARSET, &value_s);
	string value = value_s;
	XtFree(value_s);

	if (value == NO_SESSION)
	    value = DEFAULT_SESSION;

	Widget text_w = XmSelectionBoxGetChild(dialog, XmDIALOG_TEXT);
	XmTextSetString(text_w, XMST(value.chars()));
    }

    // Update delete button
    update_delete(dialog);
}

// Create custom session dialog
static Widget create_session_panel(Widget parent, const _XtString name,
				   XtCallbackProc ok,
				   XtCallbackProc apply)
{
    Arg args[10];
    int arg = 0;

    XtSetArg(args[arg], XmNautoUnmanage, False); arg++;
    Widget dialog = 
	verify(XmCreateSelectionDialog(find_shell(parent), XMST(name), args, arg));

    Delay::register_shell(dialog);

    Widget sessions = XmSelectionBoxGetChild(dialog, XmDIALOG_LIST);

    XtAddCallback(sessions, XmNsingleSelectionCallback,
		  SelectSessionCB, XtPointer(dialog));
    XtAddCallback(sessions, XmNmultipleSelectionCallback,
		  SelectSessionCB, XtPointer(dialog));
    XtAddCallback(sessions, XmNextendedSelectionCallback,
		  SelectSessionCB, XtPointer(dialog));
    XtAddCallback(sessions, XmNbrowseSelectionCallback,
		  SelectSessionCB, XtPointer(dialog));

    XtAddCallback(dialog, XmNokCallback, 
		  ok, XtPointer(sessions));
    XtAddCallback(dialog, XmNapplyCallback, 
		  apply, XtPointer(sessions));
    XtAddCallback(dialog, XmNcancelCallback, 
		  UnmanageThisCB, XtPointer(dialog));
    XtAddCallback(dialog, XmNhelpCallback, ImmediateHelpCB, 0);

    return dialog;
}



// ---------------------------------------------------------------------------
// Session deletion
// ---------------------------------------------------------------------------

// Delete session SESSION
void delete_session(const string& session, bool silent)
{
    if (session == DEFAULT_SESSION || session == NO_SESSION)
	return;

    StatusDelay delay("Deleting session " + quote(session));
    {
      const string s1 = session_state_file(session);
      unlink(s1.chars());
    }
    {
      const string s1 = session_core_file(session);
      unlink(s1.chars());
    }
    {
      const string s1 = session_history_file(session);
      unlink(s1.chars());
    }
    {
      const string s1 = session_lock_file(session);
      unlink(s1.chars());
    }
    {
      const string s1 = session_tmp_flag(session);
      unlink(s1.chars());
    }
    {
      const string s1 = session_settings_file(session);
      unlink(s1.chars());
    }

    const string s2 = session_dir(session); 
    if (rmdir(s2.chars()) && !silent)
    {
	post_error("Could not delete " + quote(session_dir(session)) + ": " 
		   + strerror(errno), "delete_session_error");
	delay.outcome = strerror(errno);
    }

    if (session == app_data.session)
    {
	// Current session is deleted
	set_session(DEFAULT_SESSION);
    }
}

// Remove selected sessions
static void DeleteSessionsCB(Widget dialog, XtPointer client_data, XtPointer)
{
    Widget sessions = Widget(client_data);

    XmStringTable selected_items;
    int selected_items_count = 0;

    assert(XmIsList(sessions));

    XtVaGetValues(sessions,
		  XmNselectedItemCount, &selected_items_count,
		  XmNselectedItems,     &selected_items,
		  XtPointer(0));

    for (int i = 0; i < selected_items_count; i++)
    {
	String _item;
	XmStringGetLtoR(selected_items[i], LIST_CHARSET, &_item);
	string item(_item);
	XtFree(_item);

	delete_session(item);
    }

    update_sessions(dialog);
}



// ---------------------------------------------------------------------------
// Session save
// ---------------------------------------------------------------------------

static string get_chosen_session(Widget dialog)
{
    Widget text     = XmSelectionBoxGetChild(dialog, XmDIALOG_TEXT);

    String v = 0;
    if (XmIsText(text))
	v = XmTextGetString(text);
    else if (XmIsTextField(text))
	v = XmTextFieldGetString(text);

    string value = v;
    if (value == NO_SESSION)
	value = DEFAULT_SESSION;
    return value;
}

// Set session to V
void set_session(const string& v)
{
    static string value;
    value = v;

    app_data.session = value.chars();

    string session_name;
    if (app_data.session == DEFAULT_SESSION)
	session_name = NO_SESSION;
    else
	session_name = quote(app_data.session);

    set_status("Current session is " + session_name + ".");
    create_session_dir(app_data.session);
}

// Set the current session
static void SetSessionCB(Widget dialog, XtPointer, XtPointer)
{
    set_session(get_chosen_session(dialog));
    update_sessions(dialog);

    if (app_data.session == DEFAULT_SESSION)
    {
	XtUnmanageChild(dialog);
	return;
    }
    else
    {
	std::vector<string> session_list;
	get_sessions(session_list);
	for (int i = 0; i < int(session_list.size()); i++)
	    if (session_list[i] == app_data.session)
	    {
		// Okay, proceed
		XtUnmanageChild(dialog);
		return;
	    }
    }
}

static Widget dump_core_w     = 0;
static Widget may_kill_w      = 0;
static Widget may_gcore_w     = 0;
static Widget may_ptrace_w    = 0;
static Widget gcore_methods_w = 0;

static void SetGCoreSensitivityCB(Widget = 0, XtPointer = 0, XtPointer = 0)
{
    bool set = 
	XmToggleButtonGetState(dump_core_w) && XtIsSensitive(dump_core_w);

    set_sensitive(gcore_methods_w, set);

    set_sensitive(may_kill_w, set);
    set_sensitive(may_gcore_w, set && gdb->type() == GDB &&
		   !string(app_data.get_core_command).empty());
#if HAVE_PTRACE_DUMPCORE
    set_sensitive(may_ptrace_w, set && gdb->type() == GDB);
#else
    set_sensitive(may_ptrace_w, false);
#endif
}

static unsigned long gcore_method = 0;

static void SetGCoreMethodCB(Widget, XtPointer client_data, XtPointer)
{
    gcore_method = (unsigned long)client_data;
}

static MMDesc gcore_methods[] =
{
    { "kill",   MMPush, 
      { SetGCoreMethodCB, XtPointer(0) }, 0, &may_kill_w, 0, 0 },
    { "gcore",  MMPush, 
      { SetGCoreMethodCB, XtPointer(MAY_GCORE) }, 0, &may_gcore_w, 0, 0 },
    { "ptrace", MMPush, 
      { SetGCoreMethodCB, XtPointer(MAY_PTRACE) }, 0, &may_ptrace_w, 0, 0 },
    MMEnd
};

static MMDesc gcore_items[] =
{
    { "dump",     MMToggle, 
      { SetGCoreSensitivityCB, 0 }, 0, &dump_core_w, 0, 0 },
    { "method",   MMOptionMenu, 
      { SetGCoreSensitivityCB, 0 }, gcore_methods, &gcore_methods_w, 0, 0 },
    MMEnd
};


// OK pressed in `save session'
static void SaveSessionCB(Widget w, XtPointer client_data, XtPointer call_data)
{
    SetSessionCB(w, client_data, call_data);

    if (app_data.session != DEFAULT_SESSION)
    {
	unsigned long flags = SAVE_SESSION | MAY_INTERACT;

	if (XmToggleButtonGetState(dump_core_w))
	    flags |= SAVE_CORE | gcore_method;

	DDDSaveOptionsCB(w, XtPointer(flags), call_data);
    }

    // Mark as `non-temporary'
    set_temporary_session(app_data.session, false);
}

// Save current session from a list of choices
void SaveSessionAsCB(Widget w, XtPointer, XtPointer)
{
    static Widget dialog = 
	create_session_panel(w, "sessions_to_save",
			     SaveSessionCB, DeleteSessionsCB);

    if (dump_core_w == 0)
    {
	// Create panel
	Widget panel = MMcreateButtonPanel(dialog, "panel", gcore_items);
	XtVaSetValues(panel, 
		      XmNorientation, XmHORIZONTAL,
		      XmNborderWidth,  0,
		      XmNentryBorder,  0,
		      XmNspacing,      0,
		      XmNmarginWidth,  0,
		      XmNmarginHeight, 0,
		      XtPointer(0));
	MMaddCallbacks(gcore_items);
	MMaddHelpCallback(gcore_items, ImmediateHelpCB);

	// Initialize: use `kill debuggee' as default item
	XtCallActionProc(may_kill_w, "ArmAndActivate", 
			 (XEvent *)0, (String *)0, 0);
    }

    ProgramInfo info;
    bool have_data = 
	info.running || (info.core != NO_GDB_ANSWER && !info.core.empty());
    XmToggleButtonSetState(dump_core_w, have_data, True);
    set_sensitive(dump_core_w, info.running);
    SetGCoreSensitivityCB();

    string name = "";
    if (app_data.session == DEFAULT_SESSION)
    {
	// No current session - suggest a default name based on executable
	if (info.file != NO_GDB_ANSWER)
	    name = info.file;


	name = basename(name.chars());
    }
    else
    {
	// Use current session name
	name = app_data.session;
    }

    MString text(name);
    XtVaSetValues(dialog, XmNtextString, text.xmstring(), XtPointer(0));

    update_sessions(dialog);
    manage_and_raise(dialog);
}



// ---------------------------------------------------------------------------
// Session open
// ---------------------------------------------------------------------------

// Get a string resource from DB named NAME with class CLS
static string get_resource(XrmDatabase db, string name, string cls)
{
    static string prefix = DDD_CLASS_NAME ".";
    name.prepend(prefix);
    cls.prepend(prefix);

    char *rtype = 0;
    XrmValue value;
    
    XrmGetResource(db, name.chars(), cls.chars(), &rtype, &value);
    if (value.addr != 0)
	return string((char *)value.addr, value.size - 1);

    return "";		// Not found
}

static Boolean done_if_idle(XtPointer data)
{
    if (emptyCommandQueue() && can_do_gdb_command())
    {
	update_settings();	// Refresh settings and signals
	update_signals();

	delete (Delay *)data;
	return True;		// Remove from the list of work procs
    }

    return False;		// Get called again
}

static void done(const string&, void *data)
{
    XtAppAddWorkProc(XtWidgetToApplicationContext(command_shell),
		     done_if_idle, data);
}

// Open the session given in SESSION
static void open_session(const string& session)
{
    // Fetch init file for this session
    string dbfile = session_state_file(session);
    XrmDatabase db = XrmGetFileDatabase(dbfile.chars());
    if (db == 0)
    {
	post_error("Cannot open session.", "open_session_error");
	return;
    }

    StatusDelay *delay_ptr = 
	new StatusDelay("Opening session " + quote(session));

    // Don't leave any more commands in the list; they'd refer to
    // another session.
    clearCommandQueue();

    ProgramInfo info;
    // Kill or detach the debuggee.
    Command c("set confirm off");
    c.verbose  = false;
    c.prompt   = false;
    c.check    = false;
    c.priority = COMMAND_PRIORITY_INIT;

    if (gdb->type() == GDB)
	gdb_command(c);

    c.command = (info.attached ? "detach" : "kill");
    gdb_command(c);

    // Clear all breakpoints and displays
    source_view->reset();
    data_disp->reset();

    // Discard current exec and core files
    if (gdb->type() == GDB)
    {
	c.command  = "file";
	gdb_command(c);
	c.command  = "core";
	gdb_command(c);
    }

    // Clear debugger console
    gdbClearWindowCB(0, 0, 0);

    // Load session-specific command history
    load_history(session_history_file(session));

    // Remove settings and signals panel (such that GDB settings and
    // signals will be updated from scratch)
    reset_settings();
    reset_signals();


    // Set buttons and display shortcuts
    static string data_buttons;
    data_buttons = get_resource(db, XtNdataButtons, XtCButtons);
    app_data.data_buttons = data_buttons.chars();

    static string source_buttons;
    source_buttons = get_resource(db, XtNsourceButtons, XtCButtons);
    app_data.source_buttons = source_buttons.chars();

    static string console_buttons;
    console_buttons = get_resource(db, XtNconsoleButtons, XtCButtons);
    app_data.console_buttons = console_buttons.chars();

    static string display_shortcuts;

    const _XtString shortcuts = 0;
    switch (gdb->type())
    {
    case BASH: shortcuts = XtNbashDisplayShortcuts; break;
    case DBG:  shortcuts = XtNdbgDisplayShortcuts;  break;
    case DBX:  shortcuts = XtNdbxDisplayShortcuts;  break;
    case GDB:  shortcuts = XtNgdbDisplayShortcuts;  break;
    case JDB:  shortcuts = XtNjdbDisplayShortcuts;  break;
    case MAKE: shortcuts = XtNmakeDisplayShortcuts; break;
    case PERL: shortcuts = XtNperlDisplayShortcuts; break;
    case PYDB: shortcuts = XtNpydbDisplayShortcuts; break;
    case XDB:  shortcuts = XtNxdbDisplayShortcuts;  break;
    }

    display_shortcuts = get_resource(db, shortcuts, XtCDisplayShortcuts);

    switch (gdb->type())
    {
    case BASH:
	app_data.bash_display_shortcuts = display_shortcuts.chars();
	break;
    case DBG:  
         app_data.dbg_display_shortcuts  = display_shortcuts.chars(); 
	break;
    case DBX:
	app_data.dbx_display_shortcuts  = display_shortcuts.chars();
	break;
    case GDB:
	app_data.gdb_display_shortcuts  = display_shortcuts.chars();
	break;
    case JDB:
	app_data.jdb_display_shortcuts  = display_shortcuts.chars();
	break;
    case MAKE:
	app_data.make_display_shortcuts = display_shortcuts.chars();
	break;
    case PERL:
	app_data.perl_display_shortcuts = display_shortcuts.chars();
	break;
    case PYDB:
	app_data.pydb_display_shortcuts = display_shortcuts.chars();
	break;
    case XDB:
	app_data.xdb_display_shortcuts  = display_shortcuts.chars();
	break;
    }
    update_user_buttons();

    // Set options
    const string tab_width_s = get_resource(db, XtNtabWidth, XtCTabWidth);
    int tab_width = atoi(tab_width_s.chars());
    app_data.tab_width = tab_width ? tab_width : DEFAULT_TAB_WIDTH;

    update_options();


    // Enqueue start-up commands
    string restart = get_resource(db, XtNrestartCommands, XtCInitCommands);
    string settings;
    switch (gdb->type())
    {
    case BASH:
    settings = string(config_get_string("bash.settings"));
	break;

    case DBG:
    settings = string(config_get_string("dbg.settings"));
	break;

    case DBX:
    settings = string(config_get_string("dbx.settings"));
	break;

    case GDB:
    settings = string(config_get_string("gdb.settings"));
	break;

    case XDB:
    settings = string(config_get_string("xdb.settings"));
	break;

    case JDB:
    settings = string(config_get_string("jdb.settings"));
	break;

    case MAKE:
    settings = string(config_get_string("make.settings"));
	break;

    case PYDB:
    settings = string(config_get_string("pydb.settings"));
	break;

    case PERL:
    settings = string(config_get_string("perl.settings"));
	break;

    }
    init_session(restart, settings, app_data.source_init_commands);

    // One last command to clear the delay and set up breakpoints
    c.command  = "# reset";
    c.callback = done;
    c.data     = (void *)(Delay *)delay_ptr;
    c.priority = COMMAND_PRIORITY_BATCH;
    c.verbose  = false;
    c.prompt   = false;
    c.check    = true;
    gdb_command(c);
}

// Restart GDB only.
void RestartDebuggerCB(Widget, XtPointer, XtPointer)
{
    static string restart_commands;
    static string settings;
    unsigned long flags = 0;

    if (gdb->type() == PERL)
	flags |= DONT_RELOAD_FILE;

    (void) get_restart_commands(restart_commands, flags);
    app_data.restart_commands = restart_commands.chars();

    settings = get_settings(gdb->type());
    switch (gdb->type())
    {
    case BASH:
	GDBAgent_PERL_settings = (char *) settings.chars();
	break;

    case DBG:
	GDBAgent_DBG_settings = (char *) settings.chars();
	break;

    case DBX:
	GDBAgent_DBX_settings = (char *) settings.chars();
	break;

    case GDB:
	GDBAgent_GDB_settings = (char *) settings.chars();
	break;

    case JDB:
	GDBAgent_JDB_settings = (char *) settings.chars();
	break;

    case MAKE:
	GDBAgent_MAKE_settings = (char *) settings.chars();
	break;

    case PERL:
	GDBAgent_PERL_settings = (char *) settings.chars();
	break;

    case PYDB:
	GDBAgent_PYDB_settings = (char *) settings.chars();
	break;

    case XDB:
	GDBAgent_XDB_settings = (char *) settings.chars();
	break;
    }

    _gdb_out("Restarting " + gdb->title() + "\n");

    // Clear all breakpoints and displays
    source_view->reset();
    data_disp->reset();

    // Abort old GDB (if still running)
    gdb->abort();

    // Start GDB again
    start_gdb(false);
}


// OK pressed in `open session'
static void OpenThisSessionCB(Widget w, XtPointer client_data, 
			      XtPointer call_data)
{
    SetSessionCB(w, client_data, call_data);
    if (app_data.session != DEFAULT_SESSION)
    {
	open_session(app_data.session);

	// Mark as `non-temporary'
	set_temporary_session(app_data.session, false);
    }
}

// Load session from a list of choices
void OpenSessionCB(Widget w, XtPointer, XtPointer)
{
    static Widget dialog = 
	create_session_panel(w, "sessions_to_open",
			     OpenThisSessionCB, DeleteSessionsCB);

    // Suggest reloading current session
    if (app_data.session != DEFAULT_SESSION)
    {
	MString text(app_data.session);
	XtVaSetValues(dialog, XmNtextString, text.xmstring(), XtPointer(0));
    }

    update_sessions(dialog);
    manage_and_raise(dialog);
}

// Name of restart session
string restart_session()
{
    if (getenv(DDD_NAME "_SESSION") != 0)
	return getenv(DDD_NAME "_SESSION");
    return "";
}

void set_restart_session(const string& session)
{
    static string env;
    env = DDD_NAME "_SESSION=" + session;
    putenv(CONST_CAST(char*,env.chars()));
}


// ---------------------------------------------------------------------------
// Core file
// ---------------------------------------------------------------------------

string session_core_file(const string& session)
{
    // Several UNIX systems have cron jobs that remove files named
    // `core' after a period of time.  So we use `dddcore' instead.

    string core = session_file(session, "core");
    if (is_core_file(core))
	return core;

    return session_file(session, ddd_NAME "core");
}


// ---------------------------------------------------------------------------
// X11R6 Session management
// ---------------------------------------------------------------------------

#if XtSpecificationRelease >= 6

// Realize X11R6 session management protocols.

static void AskSaveSmSessionCB    (Widget w, XtPointer, XtPointer call_data);
static void ConfirmSaveSmSessionCB(Widget w, XtPointer, XtPointer call_data);
static void CancelSaveSmSessionCB (Widget w, XtPointer, XtPointer call_data);

static void AskSmShutdownCB       (Widget w, XtPointer, XtPointer call_data);
static void ConfirmSmShutdownCB   (Widget w, XtPointer, XtPointer call_data);
static void CancelSmShutdownCB    (Widget w, XtPointer, XtPointer call_data);

static void ask(const string& text, const _XtString name,
		XtCheckpointToken token, Widget w,
		XtCallbackProc yes, XtCallbackProc no);


Widget delayed_widget = nullptr;
XtCheckpointTokenRec delayed_token;
void DelayedSaveSmSession(XtPointer, XtIntervalId*)
{
    if (delayed_widget!=nullptr)
        SaveSmSessionCB(delayed_widget, nullptr, &delayed_token);
    delayed_widget = nullptr;
}

// 1. The user initiates a checkpoint.  Have DDD save its options.
void SaveSmSessionCB(Widget w, XtPointer, XtPointer call_data)
{
    XtCheckpointToken token = XtCheckpointToken(call_data);

    if (app_data.session!=nullptr && strcmp(app_data.session, RESTART_SESSION.chars())==0)
    {
        // in thes case of restart the callback for XtNsaveCallback has to be delayed,
        // since the value of app_data.session is delayed, too.
        delayed_widget = w;
        memcpy(&delayed_token, call_data, sizeof(XtCheckpointTokenRec));
        XtAppContext app_context = XtWidgetToApplicationContext(w);
        XtAppAddTimeOut(app_context, 500, DelayedSaveSmSession, call_data);
        token->save_success = true;
        return;
    }

    // Save session
    const bool interact = (token->interact_style == SmInteractStyleAny);
    const bool shutdown = token->shutdown;

    unsigned long flags = SAVE_SESSION | SAVE_GEOMETRY | SAVE_CORE;
    if (interact)
	flags |= MAY_INTERACT;
    if (shutdown)
	flags |= MAY_KILL;

    if (!shutdown && saving_options_kills_program(flags))
    {
	if (interact)
	    XtAddCallback(w, XtNinteractCallback, AskSaveSmSessionCB, 0);
	else
	    return;		// Cannot save state right now
    }
    else
    {
	// Save it
	token->save_success = save_options(flags);
    }

    if (shutdown)
    {
	// Prepare for shutdown
	if (interact)
	    XtAddCallback(w, XtNinteractCallback, AskSmShutdownCB, 0);
	else
	{
	    if (!can_do_gdb_command())
	    {
		// GDB is still busy - cannot shutdown right now
		token->request_cancel = true;
	    }
	}
    }
    else
    {
	// Ordinary checkpoint -- mark as temporary
	set_temporary_session(app_data.session, true);
    }
}


// Save state

// 2. Request license to kill the program
static void AskSaveSmSessionCB(Widget w, XtPointer, XtPointer call_data)
{
    XtCheckpointToken token = XtCheckpointToken(call_data);

    ask("", "kill_to_save_dialog", token, w, 
	ConfirmSaveSmSessionCB, CancelSaveSmSessionCB);
}

// Ok, kill it
static void ConfirmSaveSmSessionCB(Widget, XtPointer client_data, XtPointer)
{
    XtCheckpointToken token = XtCheckpointToken(client_data);

    unsigned long flags = 
	SAVE_SESSION | SAVE_GEOMETRY | MAY_KILL | MAY_INTERACT;

    token->save_success = save_options(flags);
    XtSessionReturnToken(token);
}

// No, don't kill -- save everything else.
static void CancelSaveSmSessionCB(Widget, XtPointer client_data, XtPointer)
{
    XtCheckpointToken token = XtCheckpointToken(client_data);

    unsigned long flags = SAVE_SESSION | SAVE_GEOMETRY | MAY_INTERACT;

    token->save_success = save_options(flags);
    XtSessionReturnToken(token);
}


// Shutdown

// 2. If the checkpoint was part of a shutdown, make sure the
//     debugged program is killed properly.
static void AskSmShutdownCB(Widget w, XtPointer, XtPointer call_data)
{
    XtCheckpointToken token = XtCheckpointToken(call_data);

    if (token->cancel_shutdown || token->request_cancel)
    {
	XtSessionReturnToken(token);
	return;
    }

    if (gdb == 0 || !gdb->running())
    {
	XtSessionReturnToken(token);
	return;
    }

    if (!can_do_gdb_command())
    {
	// Debugger is still running; request confirmation
	ask(gdb->title() + " is still busy.  Shutdown anyway (and kill it)?",
	    "quit_dialog", token, w, ConfirmSmShutdownCB, 
	    CancelSmShutdownCB);
	return;
    }

    ProgramInfo info;
    if (info.running)
    {
	// Program is still running; request confirmation
	ask("The program is running.  Shutdown anyway (and kill it)?",
	    "shutdown_dialog", token, w, ConfirmSmShutdownCB, 
	    CancelSmShutdownCB);
	return;
    }

    // Okay, go ahead.
    ConfirmSmShutdownCB(w, XtPointer(token), call_data);
}

// 3. Confirm or cancel shutdown.
static void ConfirmSmShutdownCB(Widget, XtPointer client_data, XtPointer)
{
    XtCheckpointToken token = XtCheckpointToken(client_data);
    XtSessionReturnToken(token);
}

static void CancelSmShutdownCB(Widget, XtPointer client_data, XtPointer)
{
    XtCheckpointToken token = XtCheckpointToken(client_data);
    token->request_cancel = true;
    XtSessionReturnToken(token);
}

// 4. Let DDD die.  This leaves the session as temporary.
void ShutdownSmSessionCB(Widget w, XtPointer, XtPointer call_data)
{
    if (gdb != 0 && can_do_gdb_command())
    {
	if (gdb->type() == GDB)
	    gdb_question("set confirm off");
    }

    ddd_is_restarting    = false;
    ddd_is_shutting_down = true;
    _DDDExitCB(w, XtPointer(EXIT_SUCCESS), call_data);
}

// Create a confirmation dialog
static void ask(const string& text, const _XtString name, XtCheckpointToken token, Widget w,
		XtCallbackProc yes, XtCallbackProc no)
{
    // Program is still running; request confirmation
    Arg args[10];
    int arg;

    static Widget dialog = 0;
    if (dialog)
	DestroyWhenIdle(dialog);

    arg = 0;
    MString msg = rm(text);
    if (!text.empty())
    {
	XtSetArg(args[arg], XmNmessageString, msg.xmstring()); arg++;
    }
    dialog = verify(XmCreateQuestionDialog(find_shell(w), XMST(name),
					   args, arg));
    Delay::register_shell(dialog);
    XtAddCallback(dialog, XmNokCallback,     yes, XtPointer(token));
    XtAddCallback(dialog, XmNcancelCallback, no, XtPointer(token));
    XtAddCallback(dialog, XmNhelpCallback,   ImmediateHelpCB, 0);

    manage_and_raise(dialog);
}

#else // XtSpecificationRelease < 6

// X11R5 and earlier: Nothing yet...
void SaveSmSessionCB(Widget, XtPointer, XtPointer) {}
void ShutdownSmSessionCB(Widget, XtPointer, XtPointer) {}

#endif // XtSpecificationRelease < 6
