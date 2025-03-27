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

// Encode non-ascii characters
static void encode(const char *src, char *dest, int len)
{
    char c;
    char *end = dest + len - 2;

    while ((c = *src++))
    {
        switch (c)
        {
            case '\a':  *dest++ = '\\';  *dest++ = 'a'; break;
            case '\b':  *dest++ = '\\';  *dest++ = 'b'; break;
            case '\t':  *dest++ = '\\';  *dest++ = 't'; break;
            case '\n':  *dest++ = '\\';  *dest++ = 'n'; break;
            case '\v':  *dest++ = '\\';  *dest++ = 'v'; break;
            case '\f':  *dest++ = '\\';  *dest++ = 'f'; break;
            case '\r':  *dest++ = '\\';  *dest++ = 'r'; break;
            case '\\':  *dest++ = '\\';  *dest++ = '\\'; break;
            case '\'':  *dest++ = '\\';  *dest++ = '\''; break;
            case '\"':  *dest++ = '\\';  *dest++ = '\"'; break;
            default:    *dest++ = c; break;
        }

        if (dest >= end) 
            break;
    }

    *dest = '\0';
}

// Decode non-ascii characters
static void decode(const char *src, char *dest, int len)
{
    char c;
    char *end = dest + len - 2;

    while ((c = *src++))
    {
        if (c == '\\')
        {
            switch ((c = *src++))
            {
                case 'a':  *dest++ = '\a'; break;
                case 'b':  *dest++ = '\b'; break;
                case 't':  *dest++ = '\t'; break;
                case 'n':  *dest++ = '\n'; break;
                case 'v':  *dest++ = '\v'; break;
                case 'f':  *dest++ = '\f'; break;
                case 'r':  *dest++ = '\r'; break;
                case '\\':  *dest++ = '\\'; break;
                case '\'': *dest++ = '\''; break;
                case '\"': *dest++ = '\"'; break;
            }
        }
        else
            *dest++ = c;

        if (dest >= end) 
            break;
    }
    
    *dest = '\0';
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
    static char evalue[1024];

    // Ignore if value is null
    if (value == NULL || *value == '\0')
        return 0;

    encode(value, evalue, sizeof evalue);

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
    elem->SetText(evalue);

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
    static char value[1024];

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

    decode (elem->GetText(), value, sizeof value);
    return value;
}

/*****************************************************************/

// Set default values in app_data.
void config_set_defaults()
{
    // This will have settings moved from ddd_resources[];

    app_data.dddinit_version = DDD_VERSION;

    app_data.gdb_init_commands = 
        "set prompt (gdb) \n"
        "set height 0\n"
        "set width 0\n"
        "set annotate 1\n"
        "set print repeats unlimited\n"
        "set print sevenbit-strings off\n"
        "set verbose off\n";
    app_data.gdb_settings = 
        "set print asm-demangle on\n";
    app_data.dbx_init_commands =
        "sh stty -echo -onlcr\n"
        "set $page = 1\n";
    app_data.dbx_settings = "";
    app_data.xdb_init_commands =
        "sm\n"
        "def run r\n"
        "def cont c\n"
        "def next S\n"
        "def step s\n"
        "def quit q\n"
        "def finish { bu \\\\1t ; c ; L }\n";
    app_data.xdb_settings = "";
    app_data.bash_init_commands =
        "set prompt bashdb$_Dbg_less$_Dbg_greater$_Dbg_space \n";
    app_data.bash_settings = "";
    app_data.dbg_init_commands = "";
    app_data.dbg_settings = "";
    app_data.jdb_init_commands = "";
    app_data.jdb_settings = "";
    app_data.make_init_commands = "";
    app_data.make_settings = "";
    app_data.perl_init_commands = 
        "o CommandSet=580\n";
    app_data.perl_settings = "";
    app_data.pydb_init_commands = "";
    app_data.pydb_settings = "";
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

        // Set app_data from settings file
        app_data.gdb_init_commands = cfg.get ("gdb.init.commands");
        app_data.gdb_settings = cfg.get ("gdb.settings");
        app_data.dbx_init_commands = cfg.get ("dbx.init.commands");
        app_data.dbx_settings = cfg.get ("dbx.settings");
        app_data.xdb_init_commands = cfg.get ("xdb.init.commands");
        app_data.xdb_settings = cfg.get ("dbx.settings");
        app_data.bash_init_commands = cfg.get ("bash.init.commands");
        app_data.bash_settings = cfg.get ("bash.settings");
        app_data.dbg_init_commands = cfg.get ("dbg.init.commands");
        app_data.dbg_settings = cfg.get ("dbg.settings");
        app_data.jdb_init_commands = cfg.get ("jdb.init.commands");
        app_data.jdb_settings = cfg.get ("jdb.settings");
        app_data.make_init_commands = cfg.get ("make.init.commands");
        app_data.make_settings = cfg.get ("make.settings");
        app_data.perl_init_commands = cfg.get ("perl.init.commands");
        app_data.perl_settings = cfg.get ("perl.settings");
        app_data.pydb_init_commands = cfg.get ("pydb.init.commands");
        app_data.pydb_settings = cfg.get ("pydb.settings");

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

    // Put configuration info into settings.
    cfg.set ("gdb.init.commands", app_data.gdb_init_commands);
    cfg.set ("gdb.settings", app_data.gdb_settings);
    cfg.set ("dbx.init.commands", app_data.dbx_init_commands);
    cfg.set ("dbx.settings", app_data.dbx_settings);
    cfg.set ("xdb.init.commands", app_data.xdb_init_commands);
    cfg.set ("xdb.settings", app_data.dbx_settings);
    cfg.set ("bash.init.commands", app_data.bash_init_commands);
    cfg.set ("bash.settings", app_data.bash_settings);
    cfg.set ("dbg.init.commands", app_data.dbg_init_commands);
    cfg.set ("dbg.settings", app_data.dbg_settings);
    cfg.set ("jdb.init.commands", app_data.jdb_init_commands);
    cfg.set ("jdb.settings", app_data.jdb_settings);
    cfg.set ("make.init.commands", app_data.make_init_commands);
    cfg.set ("make.settings", app_data.make_settings);
    cfg.set ("perl.init.commands", app_data.perl_init_commands);
    cfg.set ("perl.settings", app_data.perl_settings);
    cfg.set ("pydb.init.commands", app_data.pydb_init_commands);
    cfg.set ("pydb.settings", app_data.pydb_settings);

    cfg.write (filename);
    assert (cfg.valid());

    return 0;
}

// Get value from configuration
const char *config_get_string(const char *key)
{
    return cfg.get(key);
}
