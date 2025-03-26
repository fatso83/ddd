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

#include "config_manager.h"

#include "AppData.h"
#include "base/assert.h"

#include <sys/stat.h>
#include <tinyxml2.h>
#include <vector>

// Configuration information in XML format
static Configuration cfg;

static std::vector<string> split(const char *str, const char* delimiter)
{
    std::vector<string> v;
    char copy[200];
    char *token;

    // strtok requires a modifyable char array
    assert(strlen(str) < sizeof(copy)-1);
    strcpy(copy, str);

    token = strtok(copy, delimiter);

    while (token != nullptr)
    {
        v.push_back(string(token));
        token = strtok(nullptr, delimiter);
    }
  
    return v;
}

// Create empty configuration data
Configuration::Configuration()
{
    settings.Clear();
    tinyxml2::XMLNode *pRoot = settings.NewElement("DDD");
    settings.InsertFirstChild(pRoot);
    error = tinyxml2::XML_SUCCESS;
}

Configuration::Configuration(const char *filename)
{
    read(filename);
}

// Read configuration data from file
void Configuration::read(const char *filename)
{
    struct stat sb;

    error = tinyxml2::XML_ERROR_FILE_NOT_FOUND;
    if ((stat(filename, &sb) == 0) && S_ISREG(sb.st_mode))
    {
        error = settings.LoadFile(filename);
    }
}

// Write configuration data to file
void Configuration::write(const char *filename)
{
    error = settings.SaveFile(filename);
}

// Add setting to the configuration
// Key is DOM style selector, where each node is separated by a dot.
int Configuration::set(const char *key, const char *value)
{
    tinyxml2::XMLNode *node;
    tinyxml2::XMLElement *elem;
    std::vector<string> tokens = split(key, ".");
    const char *tok = key;
    unsigned int num;

    node = settings.FirstChild();
    assert (node);

    for (num = 0; num < (tokens.size() - 1); num++)
    {
        tok = tokens[num].chars();

        if ((elem = node->FirstChildElement(tok)))
        {
            // Verify that intermediate nodes have no value set
            if (elem->GetText())
            {
                error = -1;     // Creating child of node with value
                return error;
            }
        }
        else
        {
            elem = settings.NewElement(tok);
            node->InsertEndChild(elem);
        }
        node = elem;
    }

    // Find leaf node or create it
    tok = tokens[tokens.size()-1].chars();
    elem = node->FirstChildElement(tok);
    if (!elem)
    {
        elem = settings.NewElement(tok);
        node->InsertEndChild(elem);
    }
    elem->SetText(value);

    return 0;
}


// Get setting from the configuration
// Key is DOM style selector, where each node is separated by a dot.
const char *Configuration::get(const char *key)
{
    tinyxml2::XMLNode *node;
    tinyxml2::XMLElement *elem;
    std::vector<string> tokens = split(key, ".");
    const char *tok = key;
    unsigned int num;

    node = settings.FirstChild();
    assert (node);

    for (num = 0; num < tokens.size()-1; num++)
    {
        tok = tokens[num].chars();

        // Return NULL if not found
        if (!(elem = node->FirstChildElement(tok)))
            return NULL;

        node = elem;
    }

    // Find leaf node or create it
    tok = tokens[tokens.size()-1].chars();
    elem = node->FirstChildElement(tok);
    if (!elem)
        return NULL;

    return elem->GetText();
}

/*****************************************************************/

// Set default values in app_data.
void config_set_defaults()
{
    // This will have settings moved from ddd_resources[];

    app_data.dddinit_version = DDD_VERSION;
}

// Read configuration file into app_data.
int config_set_app_data(const char *filename)
{
    const char *res;

    cfg.read(filename);
    if (cfg.valid())
    {
        // Extract configuration info from setting file.
        // Check that setting file version matches DDD version.
        res = cfg.get("version");
        if (strcmp (res, app_data.dddinit_version) != 0)
            return ERR_CONFIG_INCORRECT_VERSION;  

        return 1;
    }

    return 0;
}

// Create config file from app_data and write file
int config_write_file(const char *filename)
{
    // Set app_data default values if not initialized
    if (app_data.dddinit_version == 0)
        config_set_defaults();
    cfg.set ("version", app_data.dddinit_version);
    // Put configuration info into settings file.
    cfg.write (filename);
    assert (cfg.valid());

    return 0;
}
