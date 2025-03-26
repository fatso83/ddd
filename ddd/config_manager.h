// Manage DDD configuration

// Copyright (C) 2055 Free Software Foundation, Inc.
// Written by Michael Eager <eager@gnu.org>
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

#ifndef _DDD_config_manager_h
#define _DDD_config_manager_h

#include "base/bool.h"
#include "base/strclass.h"

#include <tinyxml2.h>

// Configuration store data in an XML file using TinyXML2
class Configuration {
    private:
        tinyxml2::XMLDocument settings;
        string filename;
        int error;
    public:
        Configuration();
        Configuration(const char *filename);

        int set(const char *key, const char *value);
        const char *get(const char *key);
        void read(const char *filename);
        void write(const char *filename);
        bool valid() { return error == tinyxml2::XML_SUCCESS; }
};

void config_set_defaults();
int config_set_app_data(const char *filename);
int config_write_file(const char *filename);

#define ERR_CONFIG_INCORRECT_VERSION -2

#endif // _DDD_config_manager_h
// DON'T ADD ANYTHING BEHIND THIS #endif
