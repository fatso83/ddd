// $Id$
// Store information about a single display espression

// Copyright (C) 1995 Technische Universitaet Braunschweig, Germany.
// Written by Dorothea Luetkehaus <luetke@ips.cs.tu-bs.de>.
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

#ifndef _DDD_DispNode_h
#define _DDD_DispNode_h

//-----------------------------------------------------------------------------
// A DispNode keeps all information about a single data display
//-----------------------------------------------------------------------------

// Misc includes
#include "base/strclass.h"
#include "base/assert.h"
#include "base/bool.h"
#include "graph/GraphNode.h"
#include "agent/HandlerL.h"
#include "graph/BoxGraphN.h"
#include "DispValue.h"

class DispBox;

// Event types
const unsigned DispNode_Disabled   = 0;

const unsigned DispNode_NTypes  = DispNode_Disabled + 1;

//-----------------------------------------------------------------------------
// Non-class functions
//-----------------------------------------------------------------------------

// Return true iff S is a user command display (`status display')
inline bool is_user_command(const string& s)
{
    return s.length() >= 2 && s[0] == '`' && s[s.length() - 1] == '`';
}

// Return user command from S
string user_command(const string& s);

//-----------------------------------------------------------------------------
// The DispNode class
//-----------------------------------------------------------------------------

class DispNode: public BoxGraphNode {
public:
    DECLARE_TYPE_INFO

private:
    int           m_disp_nr;	      // Display number
    string        m_name;	      // Display expression
    string        m_addr;	      // Location of expression
    string        m_scope;	      // Program location where created
    string        m_depends_on;	      // Display we depend upon (when deferred)
    bool          m_active;	      // Flag: is display active (in scope)?
    bool          m_saved_node_hidden;  // Saved `hidden' flag of node
    bool          m_deferred;	      // Flag: is display deferred?
    int           m_clustered;	      // Flag: is display clustered?
    bool          m_plotted;	      // Flag: is display plotted?
    bool          m_constant;	      // Flag: is display constant?
    DispValue*    m_disp_value;	      // Associated value
    DispValue*    m_selected_value;   // Selected value within DISP_VALUE
    DispBox*      m_disp_box;	      // Associated box within DISP_VALUE
    int           m_last_change;      // Last value or address change
    int           m_last_refresh;     // Last refresh

    static int m_tics;		      // Shared change and refresh counter

public:
    int           alias_of;	      // Alias of another display

protected:
    static HandlerList handlers;
    static class TagBox *findTagBox(const Box *box, DispValue *dv);
    
    virtual string str() const { return m_name; }

    DispNode(const DispNode& node);

    virtual void refresh_plot_state() const;

private:
    // Prohibit assignment
    DispNode& operator = (const DispNode&);

public:
    // Create a new display numbered DISP_NR, named NAME, created at
    // SCOPE (a function name or "") with a value of VALUE.
    DispNode(int disp_nr,
	     const string& name,
	     const string& scope,
	     const string& value,
	     bool plotted);

    // Destructor
    ~DispNode();

    // Duplication
    GraphNode *dup() const
    {
	return new DispNode(*this);
    }

    // Resources
    int disp_nr()  const             { return m_disp_nr; }
    const string& name() const       { return m_name; }
    const string& addr() const       { return m_addr; }
    const string& scope() const      { return m_scope; }
    const string& depends_on() const 
    {
	assert(deferred()); 
	return m_depends_on;
    }
    string& depends_on()
    { 
	assert(deferred()); 
	return m_depends_on;
    }

    bool enabled()  const { return value() != 0 && value()->enabled(); }
    bool disabled() const { return !enabled(); }
    bool active() const   { return m_active; }
    bool deferred() const { return m_deferred; }
    bool& deferred()      { return m_deferred; }
    int clustered() const { return m_clustered; }
    bool plotted() const  { return m_plotted; }
    bool& plotted()       { return m_plotted; }
    bool constant() const { return m_constant; }
    bool& constant()      { return m_constant; }

    int last_change() const  { return m_last_change; }
    int last_refresh() const { return m_last_refresh; }

    // These can be used to force updates
    void set_last_change(int value = 0)  { m_last_change  = value; }
    void set_last_refresh(int value = 0) { m_last_refresh = value; }

    bool is_user_command() const { return ::is_user_command(name()); }
    string user_command() const  { return ::user_command(name()); }

    // Return `true' if this expression can be aliased
    bool alias_ok() const;

    DispValue* value()          const { return m_disp_value; }
    DispValue* selected_value() const { return m_selected_value; }

    // Handlers
    static void addHandler (unsigned    type,
			    HandlerProc proc,
			    void*       client_data = 0);

    static void removeHandler (unsigned    type,
			       HandlerProc proc,
			       void        *client_data = 0);



    // Update with NEW_VALUE; return false if value is unchanged
    bool update (string& new_value);

    // Update address with NEW_ADDR
    void set_addr(const string& new_addr);

    // Re-create the box value from old DISP_VALUE
    void refresh();

    // Re-create the entire box from old DISP_VALUE
    void reset();

    // Highlights the box related to the display value DV
    void select (DispValue *dv = 0);

    // Copy selection state from SRC
    void copy_selection_state(const DispNode& src);

    // Disable and enable manually
    void disable();
    void enable();

    // Show and hide manually
    void make_active();
    void make_inactive();

    // Cluster display into TARGET
    void cluster(int target);
    inline void uncluster() { cluster(0); }

    // Toggle titles.  Return true iff changed.
    bool set_title(bool set);

    // Print plots to FILENAME
    void print_plots(const string& filename, 
		     const GraphGC& gc = GraphGC()) const;
};

// Clustering stuff

// The command to use for clusters
#define CLUSTER_COMMAND "displays"

inline bool is_cluster(const DispNode *dn)
{
    return dn->is_user_command() && 
	dn->user_command().contains(CLUSTER_COMMAND, 0);
}

#endif // _DDD_DispNode_h
// DON'T ADD ANYTHING BEHIND THIS #endif
