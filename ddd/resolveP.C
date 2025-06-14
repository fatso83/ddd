// $Id$ -*- C++ -*-
// Search DDD resources

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

char resolvePath_rcsid[] = 
    "$Id$";

#include "resolveP.h"
#include "root.h"
#include "filetype.h"
#include "session.h"
#include "base/cook.h"
#include "status.h"
#include "version.h"
#include "x11/ExitCB.h"
#include "base/strclass.h"

#include <vector>

#include <stdlib.h>

// Return directory name
static string dirname(const string& file)
{
    if (file.contains('/'))
	return file.before('/', -1);
    else
	return ".";
}

// Return full path of current program
static string myFile()
{
    const char *my_name = saved_argv()[0];
    if (my_name == 0)
	my_name = ddd_NAME;
    return cmd_file(my_name);
}

// Return my individual prefix: If I'm installed as `/usr/local/bin/ddd',
// return `/usr/local'.
static string myPrefix()
{
    string my_dir = dirname(myFile());
    if (my_dir.contains('/'))
	return dirname(my_dir);
    else
	return my_dir + "/..";
}

// Return full path of FILE, searching in a number of predefined places.
// If not found, return "".
string resolvePath(const string& file, bool include_user)
{
    static std::vector<string> prefixes;
    static int sys_index = 0;

    if (prefixes.size() == 0)
    {
	// Look in $DDD_HOME.
	if (getenv(DDD_NAME "_HOME") != 0)
	    prefixes.push_back(getenv(DDD_NAME "_HOME"));

	// Look in ~/.ddd.
	prefixes.push_back(session_state_dir());
	sys_index = prefixes.size();

	string prefix = myPrefix();

	prefixes.push_back(prefix + "/share/" ddd_NAME "-" DDD_VERSION);
	prefixes.push_back(prefix + "/share/" ddd_NAME);

	prefixes.push_back(prefix + "/" ddd_NAME "-" DDD_VERSION);
	prefixes.push_back(prefix + "/" ddd_NAME);
    }

    StatusDelay delay("Searching " + quote(file));

    for (int i = include_user ? 0 : sys_index; i < int(prefixes.size()); i++)
    {
	string path = prefixes[i] + "/" + file;
	set_status("Trying " + quote(path));
	if (is_regular_file(path) || is_directory(path))
	{
	    delay.outcome = quote(path);
	    return path;
	}
    }

    delay.outcome = "not found";
    return "";
}
