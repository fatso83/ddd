// $Id$ -*- C++ -*-
// Printing dialog

// Copyright (C) 1996-1998 Technische Universitaet Braunschweig, Germany.
// Copyright (C) 2000-2001 Universitaet Passau, Germany.
// Copyright (C) 2001, 2003 Free Software Foundation, Inc.
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

char print_rcsid[] = 
    "$Id$";

#include "print.h"

#include "AppData.h"
#include "DataDisp.h"
#include "x11/DestroyCB.h"
#include "DispGraph.h"
#include "graph/GraphEdit.h"
#include "graph/Graph.h"
#include "agent/LiterateA.h"
#include "agent/TimeOut.h"
#include "Command.h"
#include "base/cook.h"
#include "base/cwd.h"
#include "file.h"
#include "filetype.h"
#include "post.h"
#include "regexps.h"
#include "status.h"
#include "string-fun.h"
#include "tempfile.h"
#include "x11/verify.h"
#include "wm.h"
#include "x11/charsets.h"
#include "motif/MakeMenu.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <fstream>
#include <string.h>		// strerror()
#include <errno.h>
#include <unistd.h>

#include <Xm/Xm.h>
#include <Xm/ToggleB.h>
#include <Xm/RowColumn.h>
#include <Xm/Label.h>
#include <Xm/SelectioB.h>
#include <Xm/MessageB.h>
#include <Xm/Text.h>
#include <Xm/TextF.h>
#include <Xm/PushB.h>
#include <Xm/FileSB.h>

#ifndef R_OK
/* 3b2 doesn't define these according to jthomas@nmsu.edu. */
#define R_OK 04
#define W_OK 02
#endif


//-----------------------------------------------------------------------------
// Printing Dialog
//-----------------------------------------------------------------------------

static string msg(const string& path, bool displays, bool to_file)
{
    string m = "Printing ";
    if (displays)
	m += "graph ";
    else
	m += "plots ";
    if (to_file)
	m += "to ";
    m += quote(path);
    if (!to_file)
	m += " to printer";

    return m;
}

// Print to FILENAME according to given PrintGC
static int print_to_file(const string& filename, PrintGC& gc, 
			 bool selectedOnly, bool displays)
{
    string path = filename;
    if (!filename.contains('/', 0))
	path.prepend(cwd() + '/');

    StatusDelay delay(msg(path, displays, true));

    // Get the graph
    Graph *_graph = graphEditGetGraph(data_disp->graph_edit);
    DispGraph *graph = ptr_cast(DispGraph, _graph);

    if (graph->firstNode() == 0)
    {
	post_error("No displays to print.", "print_empty_graph_error", 
		   data_disp->graph_edit);
	delay.outcome = "failed";
	return -1;
    }

    // Get and set the GC
    GraphGC graphGC = graphEditGetGraphGC(data_disp->graph_edit);
    graphGC.printGC = &gc;
    graphGC.printSelectedNodesOnly = selectedOnly;

    std::ofstream os(filename.chars());
    if (os.bad())
    {
	FILE *fp = fopen(filename.chars(), "w");
	post_error(string("Cannot open ") 
		   + quote(filename) + ": " + strerror(errno), 
		   "print_failed_error", data_disp->graph_edit);
	if (fp)
	    fclose(fp);
	delay.outcome = strerror(errno);
	return -1;
    }

    if (displays)
    {
	// Print displays
	graph->print(os, graphGC);
    }
    else
    {
	// Print plots
	os.close();
	graph->print_plots(filename, graphGC);
    }

    return 0;
}

static void deletePrintAgent(XtPointer client_data, XtIntervalId *)
{
    // Delete agent after use
    Agent *edit_agent = (Agent *)client_data;
    delete edit_agent;
}

static void unlinkPrintFile(XtPointer client_data, XtIntervalId *)
{
    // Delete temp file after use
    string *tempfile = (string *)client_data;
    unlink(tempfile->chars());
    delete tempfile;
}

static string output_buffer;

static void printDoneHP(Agent *print_agent, void *client_data, void *)
{
    // Don't get called again
    print_agent->removeAllHandlers(InputEOF);
    print_agent->removeAllHandlers(Died);

    // Printing is done: remove temporary file
    XtAppAddTimeOut(XtWidgetToApplicationContext(gdb_w), 0, deletePrintAgent, 
		    XtPointer(print_agent));
    XtAppAddTimeOut(XtWidgetToApplicationContext(gdb_w), 0, unlinkPrintFile, 
		    XtPointer(client_data));

    if (!output_buffer.empty())
	set_status("");
}

static void printOutputHP(Agent *, void *, void *call_data)
{
    DataLength *input = (DataLength *)call_data;
    output_buffer += string(input->data, input->length);
    while (output_buffer.contains('\n'))
    {
	set_status(output_buffer.before('\n'));
	output_buffer = output_buffer.after('\n');
    }
    if (!output_buffer.empty())
	set_status(output_buffer);
}

// Print according to given PrintGC
static int print_to_printer(string command, PrintGC& gc, 
			    bool selectedOnly, bool displays)
{
    const string tmpfile = tempfile();
    int ret = print_to_file(tmpfile, gc, selectedOnly, displays);
    if (ret)
	return ret;

    StatusDelay delay(msg(tmpfile, displays, false));

    command += " " + tmpfile;

    LiterateAgent *print_agent = 
	new LiterateAgent(XtWidgetToApplicationContext(gdb_w), command);

    output_buffer = "";

    string *sp = new string(tmpfile);

    print_agent->removeAllHandlers(Died);
    print_agent->addHandler(InputEOF, printDoneHP, (void *)sp);
    print_agent->addHandler(Died,     printDoneHP, (void *)sp);
    print_agent->addHandler(Input, printOutputHP);
    print_agent->addHandler(Error, printOutputHP);
    print_agent->start();

    return 0;
}

// Local state of print dialog
enum PrintType   { PRINT_POSTSCRIPT, PRINT_FIG };
enum PrintTarget { TARGET_FILE, TARGET_PRINTER };

static bool            print_selected_only = false;
static bool            print_displays      = true;
static bool            print_target = TARGET_PRINTER;
static PostScriptPrintGC print_postscript_gc;
static FigPrintGC        print_xfig_gc;
static PrintType       print_type = PRINT_POSTSCRIPT;

static Widget          print_dialog = 0;
static Widget          print_command_field   = 0;
static Widget          print_file_name_field = 0;
static Widget 	       print_file_name_box   = 0;
static Widget          paper_size_dialog = 0;

static Widget          a4_paper_size;
static Widget          a3_paper_size;
static Widget          letter_paper_size;
static Widget          legal_paper_size;
static Widget          executive_paper_size;
static Widget          custom_paper_size;


// Go and print according to local state
void PrintAgainCB(Widget w, XtPointer client_data, XtPointer)
{
    const bool unmanage = ((int)(long)client_data & 1);
    const bool override = ((int)(long)client_data & 2);

    switch (print_target)
    {
    case TARGET_PRINTER:
    {
	static string command;
	command = app_data.print_command;

	if (print_command_field != 0)
	{
	    String c = XmTextFieldGetString(print_command_field);
	    command = c;
	    XtFree(c);
	}

	app_data.print_command = command.chars();
	if (print_to_printer(command, print_postscript_gc, 
			     print_selected_only, print_displays) == 0)
	{
	    if (unmanage && print_dialog != 0)
		XtUnmanageChild(print_dialog);
	}

	break;
    }
    
    case TARGET_FILE:
    {
	PrintGC *gc_ptr = 0;
	switch (print_type)
	{
	case PRINT_POSTSCRIPT:
	    gc_ptr = &print_postscript_gc;
	    break;

	case PRINT_FIG:
	    gc_ptr = &print_xfig_gc;
	    break;
	}
	PrintGC& gc = *gc_ptr;

	String file = XmTextFieldGetString(print_file_name_field);
	string f = file;
	XtFree(file);

	strip_trailing_space(f);
	if (f.empty())
	    return;

	if (access(f.chars(), W_OK) || !is_regular_file(f) || override)
	{
	    // File does not exist, is special, or override is on
	    if (print_to_file(f, gc, print_selected_only, print_displays) == 0)
	    {
		if (unmanage && print_dialog != 0)
		    XtUnmanageChild(print_dialog);
	    }
	}
	else
	{
	    // File exists - request confirmation
	    static Widget confirm_overwrite_dialog = 0;
	    if (confirm_overwrite_dialog)
		DestroyWhenIdle(confirm_overwrite_dialog);
	    confirm_overwrite_dialog = 
		verify(
		    XmCreateQuestionDialog(find_shell(w),
					   XMST("confirm_overwrite_dialog"), 
					   ArgList(0), 0));
	    Delay::register_shell(confirm_overwrite_dialog);
	    XtAddCallback(confirm_overwrite_dialog, 
			  XmNokCallback, PrintAgainCB, 
			  XtPointer(intptr_t((int)(long)client_data | 2)));
	    XtAddCallback(confirm_overwrite_dialog, 
			  XmNhelpCallback, ImmediateHelpCB, 0);

	    MString question = rm("Overwrite existing file " 
				  + quote(f) + "?");
	    XtVaSetValues (confirm_overwrite_dialog, XmNmessageString, 
			   question.xmstring(), XtPointer(0));
	    manage_and_raise(confirm_overwrite_dialog);
	}

	break;
    }
    }
}

static string suffix(PrintType print_type)
{
    switch (print_type)
    {
    case PRINT_POSTSCRIPT:
	return ".ps";

    case PRINT_FIG:
	return ".fig";
    }

    return "";
}

static void set_print_file_name(const string& name)
{
    XmTextFieldSetString(print_file_name_field, XMST(name.chars()));

    XmTextPosition last_pos = 
	XmTextFieldGetLastPosition(print_file_name_field);
    XmTextFieldSetInsertionPosition(print_file_name_field, last_pos);
    XmTextFieldShowPosition(print_file_name_field, 0);
    XmTextFieldShowPosition(print_file_name_field, last_pos);
}

static void SetPrintTypeCB(Widget w, XtPointer client_data, XtPointer)
{
    if (!XmToggleButtonGetState(w))
	return;

    string old_suffix = suffix(print_type);
    print_type = PrintType((int)(long)client_data);
    string new_suffix = suffix(print_type);

    String file_name_s = XmTextFieldGetString(print_file_name_field);
    string file_name(file_name_s);
    XtFree(file_name_s);

    if (file_name.contains(old_suffix, -1))
    {
	int idx = file_name.index(old_suffix, -1);
	file_name = file_name.before(idx) + new_suffix;

	set_print_file_name(file_name);
    }
}

static void SetSensitiveCB(Widget w, XtPointer client_data, XtPointer)
{
    if (XmToggleButtonGetState(w))
	set_sensitive(Widget(client_data), true);
}

static void TakeFocusCB(Widget w, XtPointer client_data, XtPointer)
{
    if (XmToggleButtonGetState(w))
	XmProcessTraversal(Widget(client_data), XmTRAVERSE_CURRENT);
}

static void UnsetSensitiveCB(Widget w, XtPointer client_data, XtPointer)
{
    if (XmToggleButtonGetState(w))
	set_sensitive(Widget(client_data), false);
}

static void SetPrintDisplaysCB(Widget w, XtPointer client_data, XtPointer)
{
    if (!XmToggleButtonGetState(w))
	return;

    print_displays = bool((int)(long)client_data);
}

static void SetPrintSelectedNodesCB(Widget w, XtPointer, XtPointer)
{
    print_selected_only = XmToggleButtonGetState(w);
}

static void SetPrintTargetCB(Widget w, XtPointer client_data, XtPointer)
{
    if (!XmToggleButtonGetState(w))
	return;

    print_target = PrintTarget((int)(long)client_data);
}

static void set_paper_size_string(const char *s)
{
    Widget text = XmSelectionBoxGetChild(paper_size_dialog, XmDIALOG_TEXT);
    XmTextSetString(text, XMST(s));

    static string current_paper_size;
    current_paper_size = s;
    app_data.paper_size = current_paper_size.chars();
}

static void SetGCColorCB(Widget w, XtPointer, XtPointer)
{
    print_postscript_gc.color = XmToggleButtonGetState(w);
}

static void SetGCA3(Widget w, XtPointer, XtPointer)
{
    if (XmToggleButtonGetState(w))
    {
	PostScriptPrintGC a4;

	print_postscript_gc.hsize = a4.vsize;
	print_postscript_gc.vsize = a4.hsize * 2;
	set_paper_size_string("297mm x 420mm");
    }
}

static void SetGCA4(Widget w, XtPointer, XtPointer)
{
    if (XmToggleButtonGetState(w))
    {
	PostScriptPrintGC a4;

	print_postscript_gc.hsize = a4.hsize;
	print_postscript_gc.vsize = a4.vsize;
	set_paper_size_string("210mm x 297mm");
    }
}

static void SetGCLetter(Widget w, XtPointer, XtPointer)
{
    PostScriptPrintGC gc;

    if (XmToggleButtonGetState(w))
    {
	print_postscript_gc.hsize = 72 * 8 + 72 / 2 - gc.hoffset * 2;
	print_postscript_gc.vsize = 72 * 11         - gc.voffset * 2;
	set_paper_size_string("8.5in x 11in");
    }
}

static void SetGCLegal(Widget w, XtPointer, XtPointer)
{
    PostScriptPrintGC gc;

    if (XmToggleButtonGetState(w))
    {
	print_postscript_gc.hsize = 72 * 8 + 72 / 2 - gc.hoffset * 2;
	print_postscript_gc.vsize = 72 * 14         - gc.voffset * 2;
	set_paper_size_string("8.5in x 14in");
    }
}

static void SetGCExecutive(Widget w, XtPointer, XtPointer)
{
    PostScriptPrintGC gc;

    if (XmToggleButtonGetState(w))
    {
	print_postscript_gc.hsize = 72 * 7 + 72 / 2 - gc.hoffset * 2;
	print_postscript_gc.vsize = 72 * 10         - gc.voffset * 2;
	set_paper_size_string("7.5in x 10in");
    }
}

// Convert single unit to points
static int points(string s)
{
    int points = 0;

    while (!s.empty())
    {
	const char *start = s.chars();
	char *tailptr;
	double value = strtod(start, &tailptr);
	int value_len = (int)(tailptr - start);
	if (value_len == 0)
	{
	    // No size
	    return -1;
	}
	s = s.from(value_len);

	// Read unit
	string unit = s;
	if (unit.contains(rxdouble))
	    unit = unit.before(rxdouble);

	strip_space(unit);
	unit.downcase();

	if (unit.contains("es", -1))
	    unit = unit.before("es");
	if (unit.contains("s", -1))
	    unit = unit.before("s");

	double factor = 0.0;
	if (unit == "point" || unit == "pt")
	    factor = 1.0;
	else if (unit == "inch" || unit == "in" || unit == "\"")
	    factor = 72.0;
	else if (unit == "foot" || unit == "feet" 
		 || unit == "ft" || unit == "\'")
	    factor = 72.0 * 12;
	else if (unit == "yard" || unit == "yd")
	    factor = 72.0 * 12 * 3;
	else if (unit == "mile" || unit == "mi")
	    factor = 72.0 * 12 * 5280;
	else if (unit == "mm" || unit == "millimeter")
	    factor = 72.0 * 1/2.54 * 1/10;
	else if (unit == "cm" || unit == "centimeter")
	    factor = 72.0 * 1/2.54;
	else if (unit == "m" || unit == "meter")
	    factor = 72.0 * 1/2.54 * 100;
	else if (unit == "km" || unit == "kilometer")
	    factor = 72.0 * 1/2.54 * 100000;
	else if (unit == "parsec")                              // ;-)
	    factor = 72.0 * 1/2.54 * 100000 * 3.085678e+13;
	else
	{
	    // Invalid unit
	    return -1;
	}

	if (s.contains(rxdouble))
	    s = s.from(rxdouble);
	else
	    s = "";

	if (double(points) + factor * value > double(INT_MAX))
	{
	    // Too large
	    return -1;
	}

	points += int(factor * value);
    }

    if (points <= 0)
    {
	// Too small
	return -1;
    }

    return points;
}

inline bool near(int i, int j)
{
    return abs(i - j) <= 2;
}

static void get_paper_size(const string& s, int& hsize, int& vsize)
{
    char delim = '\0';

    if (s.contains('\327'))	// \327 is the times symbol
	delim = '\327';
    else if (s.contains('x'))
	delim = 'x';
    else if (s.contains('X'))
	delim = 'X';

    if (delim == '\0')
    {
	// Bad spec
	hsize = -1;
	vsize = -1;
	return;
    }
    
    string s_hsize = s.before(delim);
    string s_vsize = s.after(delim);

    hsize = points(s_hsize);
    vsize = points(s_vsize);
}

static bool set_paper_size(const string& s)
{
    int hsize, vsize;
    get_paper_size(s, hsize, vsize);

    if (hsize <= 0 || vsize <= 0)
	return false;		// Error

    PostScriptPrintGC gc;

    print_postscript_gc.hsize = hsize - gc.hoffset * 2;
    print_postscript_gc.vsize = vsize - gc.voffset * 2;

    if (near(hsize, 594) && near(vsize, 840))
	XmToggleButtonSetState(a4_paper_size, True, True);
    else if (near(hsize, 840) && near(vsize, 1188))
	XmToggleButtonSetState(a3_paper_size, True, True);
    else if (hsize == 72 * 8 + 72 / 2 && vsize == 72 * 11)
	XmToggleButtonSetState(letter_paper_size, True, True);
    else if (hsize == 72 * 8 + 72 / 2 && vsize == 72 * 14)
	XmToggleButtonSetState(legal_paper_size, True, True);
    else if (hsize == 72 * 7 + 72 / 2 && vsize == 72 * 10)
	XmToggleButtonSetState(executive_paper_size, True, True);
    else
    {
	XmToggleButtonSetState(a4_paper_size, False, False);
	XmToggleButtonSetState(a3_paper_size, False, False);
	XmToggleButtonSetState(letter_paper_size, False, False);
	XmToggleButtonSetState(legal_paper_size, False, False);
	XmToggleButtonSetState(executive_paper_size, False, False);
	XmToggleButtonSetState(custom_paper_size, True, False);
    }

    set_paper_size_string(s.chars());

    return true;
}

static void SetPaperSizeCB(Widget w, XtPointer, XtPointer call_data)
{
    XmFileSelectionBoxCallbackStruct *cbs =
	(XmFileSelectionBoxCallbackStruct *)call_data;

    string s = "";
    char *value = 0;

    if (XmStringGetLtoR(cbs->value, MSTRING_DEFAULT_CHARSET, &value)
	&& value != 0)
    {
	s = value;
	XtFree(value);
    }

    int ok = true;
    if (!s.empty())
	ok = set_paper_size(s);

    if (ok)
	XtUnmanageChild(w);
}

static void CheckPaperSizeCB(Widget text, XtPointer client_data, XtPointer)
{
    Widget ok_button = Widget(client_data);
    String value;
    XtVaGetValues(text, XmNvalue, &value, XtPointer(0));
    string size(value);
    XtFree(value);

    int hsize, vsize;
    get_paper_size(size, hsize, vsize);

    set_sensitive(ok_button, hsize >= 0 && vsize >= 0);
}

static void ResetPaperSizeCB(Widget w, XtPointer, XtPointer)
{
    set_paper_size(app_data.paper_size);
    XtUnmanageChild(w);
}

static void SetGCCustom(Widget w, XtPointer, XtPointer)
{
    if (!XmToggleButtonGetState(w))
	return;

    manage_and_raise(paper_size_dialog);
}

static void SetGCOrientation(Widget w, XtPointer client_data, XtPointer)
{
    if (!XmToggleButtonGetState(w))
	return;

    print_postscript_gc.orientation = 
	PostScriptPrintGC::Orientation((int)(long)client_data);
}

static void SetPrintFileNameCB(Widget w,
			       XtPointer client_data, 
			       XtPointer call_data)
{
    string target = get_file(w, client_data, call_data);
    if (!target.empty())
    {
	set_print_file_name(target);
	XtUnmanageChild(w);
    }
}

static void BrowseNameCB(Widget w, XtPointer, XtPointer)
{
    Delay delay;

    static Widget dialog = 0;

    static MString pattern;

    String file = XmTextFieldGetString(print_file_name_field);
    string f = file;
    XtFree(file);

    if (f.contains('.'))
	pattern = "*" + f.from('.', -1);
    else
    {
	switch (print_type)
	{
	case PRINT_POSTSCRIPT:
	    pattern = "*.ps";
	    break;

	case PRINT_FIG:
	    pattern = "*.fig";
	    break;
	}
    }

    Arg args[10];
    Cardinal arg = 0;

    if (dialog == 0)
    {
	XtSetArg(args[arg], XmNpattern, pattern.xmstring()); arg++;
	dialog = 
	    verify(XmCreateFileSelectionDialog(find_shell(w), 
					       XMST("browse_print"), 
					       args, arg));

	Delay::register_shell(dialog);
	XtAddCallback(dialog, XmNokCallback, SetPrintFileNameCB, 0);
	XtAddCallback(dialog, XmNcancelCallback, UnmanageThisCB, 
		      XtPointer(dialog));
	XtAddCallback(dialog, XmNhelpCallback, ImmediateHelpCB, XtPointer(0));
    }
    else
    {
	XtSetArg(args[arg], XmNpattern, pattern.xmstring()); arg++;
	XtSetValues(dialog, args, arg);
    }

    manage_and_raise(dialog);
}

static void PrintCB(Widget parent, bool displays)
{
    print_displays = displays;

    static Widget print_displays_w;
    static Widget print_plots_w;
    static Widget print_selected_w;

    if (print_dialog != 0)
    {
	// Dialog already created -- pop it up again
	XmToggleButtonSetState(print_plots_w, !displays, True);
	XmToggleButtonSetState(print_displays_w, displays, True);
	XmToggleButtonSetState(print_selected_w, 
			       data_disp->have_selection(), True);
	manage_and_raise(print_dialog);
	return;
    }

    Arg args[10];
    Cardinal arg = 0;
    XtSetArg(args[arg], XmNautoUnmanage, False); arg++;
    print_dialog = 
	verify(XmCreatePromptDialog(find_shell(parent),
				    XMST("print"), 
				    args, arg));
    Delay::register_shell(print_dialog);

    XtAddCallback(print_dialog, XmNokCallback,
 		  PrintAgainCB, XtPointer(1));
    XtAddCallback(print_dialog, XmNapplyCallback,
		  PrintAgainCB, XtPointer(0));
    XtAddCallback(print_dialog, XmNcancelCallback, 
		  UnmanageThisCB, XtPointer(print_dialog));
    XtAddCallback(print_dialog, XmNhelpCallback,
		  ImmediateHelpCB, XtPointer(0));

    // Remove old prompt
    XtUnmanageChild(XmSelectionBoxGetChild(print_dialog, XmDIALOG_TEXT));
    XtUnmanageChild(XmSelectionBoxGetChild(print_dialog, 
					   XmDIALOG_SELECTION_LABEL));

    // Create menu
    static Widget print_to_printer_w;
    static Widget print_to_file_w;
    static MMDesc print_to_menu[] = 
    {
	{"printer", MMToggle, 
	    { SetPrintTargetCB, XtPointer(TARGET_PRINTER) }, 
	     0, &print_to_printer_w, 0, 0},
	{"file",    MMToggle, 
	    { SetPrintTargetCB, XtPointer(TARGET_FILE) }, 
	     0, &print_to_file_w, 0, 0 },
	MMEnd
    };

    static Widget postscript_w;
    static Widget fig_w;
    static MMDesc type2_menu[] = 
    {
	{"postscript", MMToggle, 
	 { SetPrintTypeCB, XtPointer(PRINT_POSTSCRIPT) }, 
	 0, &postscript_w, 0, 0 },
	{"xfig",       MMToggle,
	  { SetPrintTypeCB, XtPointer(PRINT_FIG) }, 0, &fig_w, 0, 0},
	MMEnd
    };

    static Widget print_color_w;
    static MMDesc type_menu[] = 
    {
	{"type2",    MMRadioPanel | MMUnmanagedLabel, 
	 MMNoCB, type2_menu, 0, 0, 0 },
	{"color",    MMToggle, { SetGCColorCB, 0 }, 
	             0, &print_color_w, 0, 0 },
	MMEnd
    };

    static MMDesc what2_menu[] = 
    {
	{"displays", MMToggle, { SetPrintDisplaysCB, XtPointer(true) }, 
	 0, &print_displays_w, 0, 0 },
	{"plots",    MMToggle, { SetPrintDisplaysCB, XtPointer(false) }, 
	 0, &print_plots_w, 0, 0 },
	MMEnd
    };

    static MMDesc what_menu[] = 
    {
	{"what2",    MMRadioPanel | MMUnmanagedLabel, 
	 MMNoCB, what2_menu, 0, 0, 0 },
	{"selected", MMToggle, { SetPrintSelectedNodesCB, 0 }, 
	             0, &print_selected_w, 0, 0 },
	MMEnd
    };

    static Widget print_portrait_w;
    static Widget print_landscape_w;
    static MMDesc orientation_menu[] = 
    {
	{"portrait",  MMToggle, 
	 { SetGCOrientation, XtPointer(PostScriptPrintGC::PORTRAIT) }, 
	     0, &print_portrait_w, 0, 0},
	{"landscape", MMToggle,
	 { SetGCOrientation, XtPointer(PostScriptPrintGC::LANDSCAPE) },
	     0, &print_landscape_w, 0, 0},
	MMEnd
    };

    static MMDesc paper_menu[] = 
    {
	{"a4",        MMToggle, 
	 {SetGCA4, 0},        0, &a4_paper_size, 0, 0},
	{"a3",        MMToggle, 
	 {SetGCA3, 0},        0, &a3_paper_size, 0, 0},
	{"letter",    MMToggle, 
	 {SetGCLetter, 0},    0, &letter_paper_size, 0, 0},
	{"legal",     MMToggle, 
	 {SetGCLegal, 0},     0, &legal_paper_size, 0, 0},
	{"executive", MMToggle, 
	 {SetGCExecutive, 0}, 0, &executive_paper_size, 0, 0},
	{"custom",    MMToggle, 
	 {SetGCCustom, 0},    0, &custom_paper_size, 0, 0},
	MMEnd
    };

    static MMDesc name_menu[] = 
    {
	{"name", 	MMTextField | MMUnmanagedLabel, MMNoCB, 
	 0, &print_file_name_field, 0, 0},
	{"browse",      MMPush, { BrowseNameCB, 0 }, 0, 0, 0, 0 },
	MMEnd
    };

    static MMDesc menu[] =
    {
	{"to",          MMRadioPanel, MMNoCB, print_to_menu, 0, 0, 0 },
	{"command",     MMTextField,  MMNoCB, 0, &print_command_field, 0, 0 },
	{"name", 	MMPanel,      MMNoCB, name_menu, 
	 &print_file_name_box, 0, 0},
	MMSep,
	{"type", 	MMPanel,      MMNoCB, type_menu, 0, 0, 0 },
	{"what",        MMPanel,      MMNoCB, what_menu, 0, 0, 0 },
	{"orientation", MMRadioPanel, MMNoCB, orientation_menu, 0, 0, 0 },
	{"size",        MMRadioPanel, MMNoCB, paper_menu, 0, 0, 0 },
	MMEnd
    };

    Widget panel = MMcreatePanel(print_dialog, "options", menu);
    (void) panel;		// Use it
    MMadjustPanel(menu);

    // Add callbacks
    MMaddCallbacks(menu);

    XtAddCallback(print_to_printer_w, XmNvalueChangedCallback,   
		  SetSensitiveCB,   XtPointer(print_command_field));
    XtAddCallback(print_to_printer_w, XmNvalueChangedCallback,   
		  SetSensitiveCB,   XtPointer(menu[1].label));

    XtAddCallback(print_to_printer_w, XmNvalueChangedCallback,
		  UnsetSensitiveCB, XtPointer(print_file_name_box));
    XtAddCallback(print_to_printer_w, XmNvalueChangedCallback,
		  UnsetSensitiveCB, XtPointer(menu[2].label));
    XtAddCallback(print_to_printer_w, XmNvalueChangedCallback,
		  UnsetSensitiveCB, XtPointer(postscript_w));
    XtAddCallback(print_to_printer_w, XmNvalueChangedCallback,
		  UnsetSensitiveCB, XtPointer(fig_w));

    XtAddCallback(print_to_printer_w, XmNvalueChangedCallback,
		  TakeFocusCB,      XtPointer(print_command_field));

    XtAddCallback(print_to_file_w, XmNvalueChangedCallback,   
		  UnsetSensitiveCB, XtPointer(print_command_field));
    XtAddCallback(print_to_file_w, XmNvalueChangedCallback,   
		  UnsetSensitiveCB, XtPointer(menu[1].label));

    XtAddCallback(print_to_file_w, XmNvalueChangedCallback,   
		  SetSensitiveCB,   XtPointer(print_file_name_box));
    XtAddCallback(print_to_file_w, XmNvalueChangedCallback,
		  SetSensitiveCB,   XtPointer(menu[2].label));
    XtAddCallback(print_to_file_w, XmNvalueChangedCallback,   
		  SetSensitiveCB,   XtPointer(postscript_w));
    XtAddCallback(print_to_file_w, XmNvalueChangedCallback,   
		  SetSensitiveCB,   XtPointer(fig_w));

    XtAddCallback(print_to_file_w, XmNvalueChangedCallback,
		  TakeFocusCB,      XtPointer(print_file_name_field));


    // Create size dialog
    arg = 0;
    XtSetArg(args[arg], XmNautoUnmanage, False); arg++;
    paper_size_dialog = 
	verify(XmCreatePromptDialog(find_shell(parent), 
				    XMST("paper_size_dialog"), 
				    args, arg));
    Delay::register_shell(paper_size_dialog);

    XtAddCallback(paper_size_dialog, XmNokCallback,
		  SetPaperSizeCB, XtPointer(0));
    XtAddCallback(paper_size_dialog, XmNcancelCallback, 
		  ResetPaperSizeCB, XtPointer(0));
    XtAddCallback(paper_size_dialog, XmNhelpCallback,   
		  ImmediateHelpCB, XtPointer(0));

    Widget size = XmSelectionBoxGetChild(paper_size_dialog,
					 XmDIALOG_TEXT);
    Widget ok_button = XmSelectionBoxGetChild(paper_size_dialog, 
					      XmDIALOG_OK_BUTTON);
    XtAddCallback(size, XmNvalueChangedCallback, 
		  CheckPaperSizeCB, XtPointer(ok_button));

    // Set initial state
    XmToggleButtonSetState(print_to_printer_w, True, True);
    XmToggleButtonSetState(postscript_w, True, True);
    XmToggleButtonSetState(print_color_w, False, True);
    XmToggleButtonSetState(print_selected_w, False, True);
    XmToggleButtonSetState(print_portrait_w, True, True);
    XmToggleButtonSetState(print_landscape_w, False, True);
    XmToggleButtonSetState(print_plots_w, !displays, True);
    XmToggleButtonSetState(print_displays_w, displays, True);
    XmToggleButtonSetState(print_selected_w, 
			   data_disp->have_selection(), True);

    bool ok = set_paper_size(app_data.paper_size);
    if (!ok)
	XmToggleButtonSetState(a4_paper_size, True, True);

    string command = string(app_data.print_command) + " ";
    XmTextFieldSetString(print_command_field, XMST(command.chars()));

    // Gofer it!
    manage_and_raise(print_dialog);
}

void PrintGraphCB(Widget w, XtPointer, XtPointer)
{
    PrintCB(w, true);
}

void PrintPlotCB(Widget w, XtPointer, XtPointer)
{
    PrintCB(w, false);
}
