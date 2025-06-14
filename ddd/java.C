// $Id$ -*- C++ -*-
// Java class lookup

// Copyright (C) 1998 Technische Universitaet Braunschweig, Germany.
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

char java_rcsid[] = 
    "$Id$";

#include "java.h"

#include "SmartC.h"
#include "SourceView.h"
#include "template/StatArray.h"
#include "assert.h"
#include "base/basename.h"
#include "base/cook.h"
#include "ddd.h"
#include "filetype.h"
#include "base/glob.h"
#include "base/isid.h"
#include "regexps.h"
#include "status.h"



//-----------------------------------------------------------------------------
// Constants
//-----------------------------------------------------------------------------

// Suffixes for Java sources and classes
#define JAVA_SRC_SUFFIX   ".java"
#define JAVA_CLASS_SUFFIX ".class"


//-----------------------------------------------------------------------------
// Helpers
//-----------------------------------------------------------------------------

static bool is_archive(const string& loc)
{
    return loc.contains(".jar", -1) || 
	   loc.contains(".zip", -1) ||
	   loc.contains(".JAR", -1) ||
	   loc.contains(".ZIP", -1);
}

static string concat_dir(const string& dir, const string& file)
{
    string mask;
    if (dir.empty())
	mask = file;
    else if (dir.contains('/', -1))
	mask = dir + file;
    else
	mask = dir + "/" + file;

    return mask;
}

// Store all classes in DIR in CLASSES_LIST.  BASE is the initial dir.  If
// WITH_SOURCE_ONLY is set, consider only classes with loadable sources.
static void get_java_classes(const string& dir, const string& base,
			     std::vector<string>& classes_list, bool with_source_only,
			     StatArray& dirs_seen)
{
    struct stat sb;
    if (stat(dir.chars(), &sb))
	return;			// Cannot stat

    for (int i = 0; i < int(dirs_seen.size()); i++)
    {
	if (dirs_seen[i].st_ino == sb.st_ino &&
	    dirs_seen[i].st_dev == sb.st_dev)
	{
	    return;		// Already considered
	}
    }

    dirs_seen.push_back(sb);

    StatusDelay delay("Scanning " + quote(dir) + " for classes");

    assert((base.empty() && dir.empty()) || dir.contains(base, 0));

    string mask = concat_dir(dir, "*" JAVA_CLASS_SUFFIX);

    // Check for `.class' files in this directory.
    char **files = glob_filename(mask.chars());
    if (files == (char **)0)
    {
	delay.outcome = mask + ": glob failed";
    }
    if (files == (char **)-1)
    {
	// No `*.class' in this directory
    }
    else
    {
	for (int i = 0; files[i] != 0; i++)
	{
	    string file = files[i];

	    // Build a class name from file name
	    string class_name;
	    if (base.empty())
	    {
		class_name = file;
	    }
	    else
	    {
		class_name = file.after((int)base.length());
		if (class_name.contains('/', 0))
		    class_name = class_name.after('/');
	    }

	    strip_java_suffix(class_name);
	    class_name.gsub('/', '.');

	    bool have_source = true;
	    if (with_source_only)
	    {
		// Check whether we have some .java source for
		// this class
		string class_file = java_class_file(class_name);
		have_source = !class_file.empty();
	    }

	    if (have_source)
	    {
		// Okay - we have a class and a corresponding
		// .java source file.  Go for it.
		classes_list.push_back(class_name);
	    }

	    free(files[i]);
	}

	free((char *)files);
    }

    // Check for `.class' files in subdirectories.
    mask = concat_dir(dir, "*");
    files = glob_filename(mask.chars());
    if (files == (char **)0)
    {
	delay.outcome = mask + ": glob failed\n";
    }
    else if (files == (char **)-1)
    {
	// No `*' in this directory
    }
    else
    {
	for (int i = 0; files[i] != 0; i++)
	{
	    string file = basename(files[i]);
	    free(files[i]);

	    if (file.matches(rxidentifier))
	    {
		file = concat_dir(dir, file);
		if (is_directory(file))
		    get_java_classes(file, base, classes_list, 
				     with_source_only, dirs_seen);
	    }
	}

	free((char *)files);
    }
}


//-----------------------------------------------------------------------------
// Main functions
//-----------------------------------------------------------------------------

// Store all classes in current use path in CLASSES_LIST.  If
// WITH_SOURCE_ONLY is set, consider only classes with loadable sources.
void get_java_classes(std::vector<string>& classes_list, bool with_source_only)
{
    string use = source_view->class_path();
    while (!use.empty())
    {
	// Determine current USE entry
	string loc;
	if (use.contains(':'))
	    loc = use.before(':');
	else
	    loc = use;
	use = use.after(':');

	if (is_archive(loc))
	{
	    // Archive file.
	    // Should we search this for classes? (FIXME)
	    continue;
	}

	if (!is_directory(loc))
	{
	    // Not a directory.
	    continue;
	}

	// Get all classes in this directory
	StatArray dirs_seen;
	get_java_classes(loc, loc, classes_list, with_source_only, dirs_seen);
    }

    smart_sort(classes_list);
    uniq(classes_list);
}

// Remove `.java' and `.class' suffix from S
void strip_java_suffix(string& s)
{
    string s_down = downcase(s);

    if (s_down.contains(JAVA_SRC_SUFFIX, -1))
	s = s.before(int(int(s.length()) - strlen(JAVA_SRC_SUFFIX)));
    if (s_down.contains(JAVA_CLASS_SUFFIX, -1))
	s = s.before(int(int(s.length()) - strlen(JAVA_CLASS_SUFFIX)));
}

// Read FILE into S
static void slurp_file(const string& file, string& s)
{
    s = "";

    FILE *fp = fopen(file.chars(), "r");
    if (fp == 0)
	return;

    while (!feof(fp))
    {
	char buffer[BUFSIZ];
	int nitems = fread(buffer, sizeof(char), BUFSIZ, fp);
	s += string(buffer, nitems);
    }

    fclose(fp);
}

// Return the position of CLASS_NAME definition in TEXT; -1 if none
int java_class_start(const string& text, const string& class_name, 
		     bool first_line)
{
    // We search for `class [whitespace] CLASS_NAME [whitespace]'.
    int i = -1;
    while ((i = text.index("class", i + 1)) >= 0)
    {
	while (i < int(text.length()) && !isspace(text[i]))
	    i++;
	while (i < int(text.length()) && isspace(text[i]))
	    i++;

	if (text.contains(class_name, i))
	{
	    int start = i;
	    i += class_name.length();
	    if (i < int(text.length()) && isspace(text[i]))
	    {
		// Be sure the class name is followed by `{'
		while (i < int(text.length()) && 
		       text[i] != ';' && text[i] != '{')
		    i++;
		if (i < int(text.length()) && text[i] == '{')
		{
		    // Okay, we got it.
		    if (first_line)
		    {
			// Return first line after `{'
			while (i < int(text.length()) && 
			       text[i - 1] != '\n' && text[i - 1] != '}')
			    i++;
			return i;
		    }
		    else
		    {
			// Return start of class definition.
			return start;
		    }
		}
	    }
	}
    }

    return -1;
}

// Check if FILE_NAME has a definition of CLASS_NAME
static bool has_class(const string& file_name, const string& class_name)
{
    string src_file;
    slurp_file(file_name, src_file);
    return java_class_start(src_file, class_name) > 0;
}

// Return source file of CLASS_NAME; "" if none.
static string _java_class_file(const string& class_name, bool search_classes)
{
    // We use 4 iterations:
    // Trial 0.  Search for CLASS_NAME.java; make sure it defines CLASS_NAME.
    // Trial 1.  Search for CLASS_NAME.class and its .java file, as in 0.
    // Trial 2.  Search for CLASS_NAME.java, even if CLASS_NAME is not def'd.
    // Trial 3.  Search for CLASS_NAME.class and its .java file, as in 2.
    for (int trial = 0; trial < 4; trial++)
    {
	string base = class_name;
	strip_java_suffix(base);
	base.gsub(".", "/");

	switch (trial)
	{
	case 0:
	case 2:
	    base += JAVA_SRC_SUFFIX;
	    break;

	case 1:
	case 3:
	    if (!search_classes)
		continue;
	    base += JAVA_CLASS_SUFFIX;
	    break;

	default:
	    return "";		// No such trial
	}

	string use = source_view->class_path();
	while (!use.empty())
	{
	    string loc;
	    if (use.contains(':'))
		loc = use.before(':');
	    else
		loc = use;
	    use = use.after(':');

	    if (is_archive(loc))
	    {
		// Archive file.
		// Should we search this for classes? (FIXME)
		continue;
	    }

	    if (!is_directory(loc))
	    {
		// Not a directory.
		continue;
	    }

	    string file_name;
	    if (loc.empty() || loc == ".")
	    {
		file_name = base;
	    }
	    else
	    {
		if (!loc.contains('/', -1))
		    loc += '/';
		file_name = loc + base;
	    }

	    switch (trial)
	    {
	    case 0:
	    case 2:
		// Look for CLASS_NAME.java 
		if (is_source_file(file_name) && 
		    (trial > 0 || has_class(file_name, class_name)))
		    return file_name;
		break;

	    case 1:
	    case 3:
	    {
		// Look for CLASS_NAME.class and scan it for
		// `SRC.java' occurrences.
		if (is_regular_file(file_name))
		{
		    string class_file;
		    slurp_file(file_name, class_file);

		    // We begin searching at the end of the file,
		    // because that's where debugging information is
		    // usually placed.
		    int i = class_file.length();
		    while ((i = 
			    class_file.index(JAVA_SRC_SUFFIX, 
					     i - class_file.length() - 1)) > 0)
		    {
			// The class file contains `.java' at I
			string src_class;
			string c;

			// Find start of name
			int start = i;
			while (start > 0 && isid(class_file[start - 1]))
			    start--;
			src_class = class_file.at(start, i - start);

			// Search for this class file instead.
			c = java_class_file(src_class, false);
			if (!c.empty())
			    return c;

			// JDK javac places source file names as \001
			// LENGTH NAME, where LENGTH is the length of
			// NAME in 2-byte (lo/hi) format.  Try this.

			while (start >= 0 && class_file[start] != '\001')
			    start--;
			start += 3;	// Skip \001 LENGTH
			src_class = class_file.at(start, i - start);
			    
			c = java_class_file(src_class, false);
			if (!c.empty())
			    return c;
		    }
		}
	    }
	    }
	}
    }

    return "";			// Not found
}

// Same, but with diagnostics
string java_class_file(const string& class_name, bool search_classes)
{
    StatusDelay delay("Searching for " + quote(class_name) + " source");

    string c = _java_class_file(class_name, search_classes);
    if (c.empty())
	delay.outcome = "failed";
    else
	delay.outcome = quote(c);

    return c;
}
