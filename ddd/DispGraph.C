// $Id$
// Store information about all displayed display expressions

// Copyright (C) 1995-1998 Technische Universitaet Braunschweig, Germany.
// Copyright (C) 2000 Universitaet Passau, Germany.
// Copyright (C) 2003 Free Software Foundation, Inc.
// Written by Dorothea Luetkehaus <luetke@ips.cs.tu-bs.de>
// and Andreas Zeller <zeller@gnu.org>
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

char DispGraph_rcsid[] =
    "$Id$";

//-----------------------------------------------------------------------------
// DispGraph stores information about all displayed display expressions.
//-----------------------------------------------------------------------------

#include "DispGraph.h"

#include <math.h>
#include "base/pi.h"
#include "base/hypot.h"
#include <X11/StringDefs.h>

#include "AppData.h"
#include "graph/GraphEdit.h"
#include "assert.h"
#include "AliasGE.h"
#include "graph/HintGraphN.h"
#include "regexps.h"
#include "graph/BoxEdgeA.h"
#include "annotation.h"
#include "DispBox.h"
#include "graph/EdgeAPA.h"

#include <vector>

DEFINE_TYPE_INFO_1(DispGraph, Graph)


//-------------------------------------------------------------------------
// Basics
//-------------------------------------------------------------------------

// Constructor
DispGraph::DispGraph()
    : Graph(), idMap(), handlers(DispGraph_NTypes),
      no_enabled(true), no_disabled(true)
{
    DispNode::addHandler(DispNode_Disabled,
			 disp_node_disabledHP,
			 (void*)this);
}

// Destructor
DispGraph::~DispGraph() 
{
    DispGraph::clear();
}

// Delete all
void DispGraph::clear()
{
    idMap.delete_all_contents();
}



//-------------------------------------------------------------------------
// Node management
//-------------------------------------------------------------------------

// Return number of nodes
int DispGraph::count_all (Displaying e) const
{
    if (e == Both)
	return idMap.length();

    int count = 0;
    MapRef ref;
    for (int k = idMap.first_key(ref); k != 0; k =
	     idMap.next_key(ref))
    {
	switch (e) 
	{
	case Both:
	    count++;
	    break;
	case Enabled:
	    if (idMap.get(k)->enabled())
		count++;
	    break;
	case Disabled:
	    if (!(idMap.get(k)->enabled()))
		count++;
	    break;
	default:
	    assert (0);
	    break;
	}
    }
    return count;
}

// Count selected nodes
int DispGraph::count_selected() const
{
    int count = 0;
    MapRef ref;
    for (int k = idMap.first_key(ref); k != 0; k = idMap.next_key(ref)) {
	    if (!idMap.get(k)->selected())
		count++;
    }
    return count;
}

// Add an event handler
void DispGraph::addHandler (unsigned    type,
			    HandlerProc proc,
			    void*       client_data)
{
    handlers.add(type, proc, client_data);
}

// Remove an event handler
void DispGraph::removeHandler (unsigned    type,
			       HandlerProc proc,
			       void        *client_data)
{
    handlers.remove(type, proc, client_data);
}

// Call event handlers
void DispGraph::callHandlers ()
{
    handlers.call(DispGraph_Empty,
		  this,
		  (void*)(count_all() == 0));
    handlers.call(NoEnabled,
		  this,
		  (void*)(no_enabled));
    handlers.call(NoDisabled,
		  this,
		  (void*)(no_disabled));
}


// Add a new edge
void DispGraph::add_edge(DispNode *from, DispNode *to)
{
    string a = annotation(from->name(), to->name());
    BoxEdgeAnnotation *ann = 0;
    if (!a.empty())
	ann = new BoxEdgeAnnotation(DispBox::eval(from->value(), 
						  "annotation", a));

    *this += new LineGraphEdge(from, to, ann);
}


// Insert NEW_DN into graph, return its number iff successful
int DispGraph::insert(int new_disp_nr, DispNode *new_dn, int depends_on)
{
    if (idMap.contains(new_disp_nr))
	return 0;
    if (idMap.length() == 0)
	handlers.call(DispGraph_Empty, this, (void*)false);

    *this += new_dn;

    if (depends_on != 0)
    {
	DispNode *old_dn = idMap.get(depends_on);
	add_edge(old_dn, new_dn);

	if (old_dn->clustered())
	{
	    DispNode *cluster = idMap.get(old_dn->clustered());
	    if (cluster != 0)
		add_edge(cluster, new_dn);
	}
    }
    assert (Graph::OK());

    idMap.insert (new_disp_nr, new_dn);

    if (no_enabled) {
	if (!( no_enabled = (count_all(Enabled) == 0) ))
	    handlers.call(NoEnabled, this, (void*)false);
    }
    if (no_disabled) {
	if (!( no_disabled = (count_all(Disabled) == 0) ))
	    handlers.call(NoDisabled, this, (void*)false);
    }

    refresh_titles();

    return new_disp_nr;
}


// Get a good position for NEW_NODE
BoxPoint DispGraph::adjust_position (DispNode *new_node,
				     Widget w,
				     BoxPoint pos,
				     const BoxPoint& offset,
				     BoxPoint grid) const
{
    const GraphGC& graphGC = graphEditGetGraphGC(w);

    // std::clog << "new node       at " << pos << "\n";

    BoxSize new_size(new_node->box()->size());

    // Leave GRID space around the box
    BoxRegion new_region(pos - (new_size + grid) / 2, new_size + grid);

    // Make sure the new node is fully visible
    while (new_region.origin()[X] <= 0)
    {
	pos                 += BoxPoint(grid[X], 0);
	new_region.origin() += BoxPoint(grid[X], 0);
	// std::clog << "new node now   at " << pos << "\n";
    }

    while (new_region.origin()[Y] <= 0)
    {
	pos                 += BoxPoint(0, grid[Y]);
	new_region.origin() += BoxPoint(0, grid[Y]);
	// std::clog << "new node now   at " << pos << "\n";
    }

    // Make sure the new node does not obscure existing nodes
    GraphNode *n = firstVisibleNode();
    while (n != 0)
    {
	const BoxRegion& region = n->region(graphGC);
	if (new_region <= region)
	{
	    pos                 += offset;
	    new_region.origin() += offset;

	    n = firstVisibleNode();
	    // std::clog << "new node now   at " << pos << "\n";
	}
	else
	    n = nextVisibleNode(n);
    }

    return pos;
}

// Return a default position for NEW_NODE
BoxPoint DispGraph::default_pos(DispNode *new_node, 
				Widget w, int depends_on) const
{
    Dimension grid_height = 16;
    Dimension grid_width  = 16;
    Cardinal rotation     = 0;
    XtVaGetValues(w,
		  XtNgridHeight, &grid_height,
		  XtNgridWidth,  &grid_width,
		  XtNrotation,   &rotation,
		  XtPointer(0));

    BoxPoint grid(max(grid_height, 1), max(grid_width, 1));
    BoxPoint delta(grid[X] * 3, grid[Y] * 2);

    bool horizontal = rotation % 180;

    BoxPoint pos;
    BoxPoint offset;

    if (depends_on == 0)
    {
	// Default offset: create new displays orthogonal to 
	// dereference direction
	offset = horizontal ? BoxPoint(grid[X], 0) : BoxPoint(0, grid[Y]);

	// New node: start with the top-level visible position
	Position x = 0;
	Position y = 0;
	XtVaGetValues(w, XtNx, &x, XtNy, &y, XtPointer(0));
	pos = BoxPoint(max(-x, grid[X]), max(-y, grid[Y] * 2));

	// Add size offset
	BoxSize new_size(new_node->box()->size());
	pos += new_size / 2;

	// Round to nearest grid position
	pos = graphEditFinalPosition(w, pos);
    }
    else
    {
	// Dependent node

	// Default offset: create new displays in dereference direction
	offset = horizontal ? BoxPoint(0, delta[Y]) : BoxPoint(delta[X], 0);

	// Place new node below/on the right of original node, depending
	// on last layout orientation.
	//
	// NODE -> (new)

	BoxGraphNode *node = idMap.get(depends_on);
	// std::clog << "node           at " << node->pos() << "\n";
	pos = node->pos() + offset;

	assert(pos.isValid());

	// Check if we already have a successor
	BoxGraphNode *max_child      = 0;
	BoxGraphNode *next_max_child = 0;

	// Find two greatest children
	for (GraphEdge *edge = node->firstFrom(); 
	     edge != 0; 
	     edge = node->nextFrom(edge))
	{
	    BoxDimension d = horizontal ? X : Y;

	    GraphNode *child = edge->to();
	    while (child->isHint())
		child = child->firstFrom()->to();
	    if (child->hidden())
		continue;
	    if (child == new_node)
		continue;
	    if (child->pos() == BoxPoint())
		continue;

	    BoxGraphNode *bgn = ptr_cast(BoxGraphNode, child);
	    if (bgn == 0)
		continue;

	    if (max_child == 0 || child->pos()[d] > max_child->pos()[d])
	    {
		next_max_child = max_child;
		max_child = bgn;
	    }
	    else if (next_max_child == 0 
		     || child->pos()[d] > next_max_child->pos()[d])
	    {
		next_max_child = bgn;
	    }
	}

	if (max_child && next_max_child)
	{
	    // Re-use offset between the two last children
	    //
	    //   NODE ->         .
	    //        \->        .
	    //         \->       .
	    //          \->   NEXT_MAX_CHILD
	    //           \->  MAX_CHILD
	    //            \-> (new)

	    // std::clog << "max_child      at " << max_child->pos() << "\n";
	    // std::clog << "next_max_child at " << next_max_child->pos() << "\n";

	    // Re-use offset between last two children
	    pos = max_child->pos() 
		+ (max_child->pos() - next_max_child->pos());

	    // If MAX_CHILD is on the right of NEXT_MAX_CHILD, place new
	    // node on the right; if MAX_CHILD is below NEXT_MAX_CHILD,
	    // place new node below.  If position is occupied, try later
	    // in the same direction.
	    bool horizontal = 
		(abs(max_child->pos()[X] - next_max_child->pos()[X]) >
		 abs(max_child->pos()[Y] - next_max_child->pos()[Y]));

	    offset = horizontal ? 
		BoxPoint(delta[X], 0) : BoxPoint(0, delta[Y]);
	}
	else if (max_child)
	{
	    // Place new child below last child
	    //
	    //   NODE ->     MAX_CHILD
	    //        \->    (new)

	    // std::clog << "child          at " << max_child->pos() << "\n";

	    // If MAX_CHILD is on the right of NODE, place new node below;
	    // if MAX_CHILD is below NODE, place new node on the right.
	    bool horizontal = 
		(abs(max_child->pos()[X] - node->pos()[X]) >
		 abs(max_child->pos()[Y] - node->pos()[Y]));
	    offset = horizontal ? 
		BoxPoint(0, delta[Y]) : BoxPoint(delta[X], 0);

	    pos = max_child->pos() + offset;
	}
	else
	{
	    GraphEdge *edge = node->firstTo();
	    if (edge)
	    {
		// We have a predecessor: use this offset instead
		//
		// PARENT -> NODE -> (new)
		//

		GraphNode *parent = edge->from();
		assert(parent->pos().isValid());

		// std::clog << "parent         at " << parent->pos() << "\n";

		// Re-use offset between parent and node
		pos = node->pos() + (node->pos() - parent->pos());

		// If NODE is on the right of PARENT, place new node on
		// the right; if NODE is below PARENT, place new node
		// below.
		bool horizontal = 
		    (abs(node->pos()[X] - parent->pos()[X]) >
		     abs(node->pos()[Y] - parent->pos()[Y]));

		offset = horizontal ? BoxPoint(delta[X], 0) 
		    : BoxPoint(0, delta[Y]);
	    }
	}
    }

    assert(pos.isValid());
    assert(offset.isValid());

    return adjust_position(new_node, w, pos, offset, grid);
}

// Find all hints in edges leading to NODE; store them in HINTS
void DispGraph::find_hints_to(GraphNode *node, GraphNodePointerArray& hints)
{
    for (GraphEdge *edge = node->firstTo();
	 edge != 0;
	 edge = node->nextTo(edge))
    {
	if (edge->from()->isHint())
	{
	    find_hints_to(edge->from(), hints);
	    hints.push_back(edge->from());
	}
    }
}

// Find all hints in edges coming from NODE; store them in HINTS
void DispGraph::find_hints_from(GraphNode *node, GraphNodePointerArray& hints)
{
    for (GraphEdge *edge = node->firstFrom();
	 edge != 0;
	 edge = node->nextFrom(edge))
    {
	if (edge->to()->isHint())
	{
	    find_hints_from(edge->to(), hints);
	    hints.push_back(edge->to());
	}
    }
}

// Remove an edge from graph and from memory
void DispGraph::delete_edge(GraphEdge *edge)
{
    *this -= edge;
    delete edge;
}

// Remove a node and all its edges
void DispGraph::delete_node(GraphNode *node)
{
    // Remove all edges
    GraphEdge *e;
    while ((e = node->firstFrom()) != 0)
	delete_edge(e);
    while ((e = node->firstTo()) != 0)
	delete_edge(e);

    // Remove node from graph
    *this -= node;
    delete node;
}

// Delete display
bool DispGraph::del (int disp_nr)
{
    if (!idMap.contains (disp_nr))
	return false;

    unalias(disp_nr);
    DispNode* dn = idMap.get(disp_nr);

    GraphNodePointerArray hints;

    find_hints_from(dn, hints);
    find_hints_to(dn, hints);
    for (int i = 0; i < int(hints.size()); i++)
	delete_node(hints[i]);

    delete_node(dn);
    idMap.del(disp_nr);

    if (idMap.length() == 0)
	handlers.call(DispGraph_Empty, this, (void*)true);
    if (!no_enabled)
	if ((no_enabled = (count_all(Enabled) == 0)))
	    handlers.call(NoEnabled, this, (void*)true);
    if (!no_disabled)
	if ((no_disabled = (count_all(Disabled) == 0)))
	    handlers.call(NoDisabled, this, (void*)true);

    refresh_titles();
    return true;
}

// Return display number of NODE
int DispGraph::get_nr(BoxGraphNode *node) const
{
    DispNode *dn = ptr_cast(DispNode, node);
    return dn == 0 ? 0 : dn->disp_nr();
}


// Get number of node NAME
int DispGraph::get_by_name(const string& name) const
{
    MapRef ref;
    int k;
    int nr = 0;

    if (name.matches(rxint))
	nr = atoi(name.chars());

    if (nr != 0)
    {
	// Check for a display with this number
	for (k = idMap.first_key(ref); k != 0; k = idMap.next_key(ref))
	    if (idMap.get(k)->disp_nr() == nr)
		return k;
    }

    // Check for a display with this name
    for (k = idMap.first_key(ref); k != 0; k = idMap.next_key(ref))
	if (idMap.get(k)->name() == name)
	    return k;

    return nr;
}


// Return first node; 0 if not found
DispNode* DispGraph::first (MapRef& ref, Displaying e) const
{
    for (DispNode* dn = idMap.first(ref); dn != 0; dn = idMap.next(ref)) {
	switch (e) {
	case Both:
	    return dn;

	case Enabled:
	    if (dn->enabled())
		return dn;
	    break;

	case Disabled:
	    if (!dn->enabled())
		return dn;
	    break;

	default:
	    assert (0);		// This can't happen
	    break;
	}
    }
    return 0;
}


// Return next node; 0 if not found
DispNode* DispGraph::next (MapRef& ref, Displaying e) const
{
    for (DispNode* dn = idMap.next(ref); dn != 0; dn = idMap.next(ref)) {
	switch (e) {
	case Both:
	    return dn;

	case Enabled:
	    if (dn->enabled())
		return dn;
	    break;

	case Disabled:
	    if (!dn->enabled())
		return dn;
	    break;

	default:
	    assert (0);		// This can't happen.
	    break;
	}
    }
    return 0;
}

// Return number of first node; 0 if not found
int DispGraph::first_nr (MapRef& ref, Displaying e) const
{
    for (int k = idMap.first_key(ref); k != 0; k = idMap.next_key(ref)) {
	switch (e) {
	case Both:
	    return k;

	case Enabled:
	    if (idMap.get (k)->enabled())
		return k;
	    break;

	case Disabled:
	    if (!idMap.get (k)->enabled())
		return k;
	    break;

	default:
	    assert (0);		// This can't happen.
	    break;
	}
    }
    return 0;
}

// Return next node number; 0 if not found
int DispGraph::next_nr (MapRef& ref, Displaying e) const
{
    for (int k = idMap.next_key(ref); k != 0; k = idMap.next_key(ref)) {
	switch (e) {
	case Both:
	    return k;

	case Enabled:
	    if (idMap.get(k)->enabled())
		return k;
	    break;

	case Disabled:
	    if (!idMap.get(k)->enabled())
		return k;
	    break;

	default:
	    assert (0);		// This can't happen
	    break;
	}
    }
    return 0;
}

// Helper for disabling nodes
void DispGraph::disp_node_disabledHP (void*,
				      void* client_data,
				      void* call_data)
{
    DispGraph* disp_graph = (DispGraph*) client_data;
    bool    disabled   = bool(call_data);

    if (disabled) {
	if (disp_graph->no_disabled) {
	    disp_graph->no_disabled = false;
	    disp_graph->handlers.call(NoDisabled, disp_graph, (void*)false);
	}
	if ((disp_graph->no_enabled = (disp_graph->count_all(Enabled) == 0)))
	    disp_graph->handlers.call(NoEnabled, disp_graph, (void*)true);
    }
    else {
	if (disp_graph->no_enabled) {
	    disp_graph->no_enabled = false;
	    disp_graph->handlers.call(NoEnabled, disp_graph, (void*)false);
	}
	if ((disp_graph->no_disabled = (disp_graph->count_all(Disabled) == 0)))
	    disp_graph->handlers.call(NoDisabled, disp_graph, (void*)true);
    }
}


//-------------------------------------------------------------------------
// Alias handling
//-------------------------------------------------------------------------

// Make DISP_NR an alias of ALIAS_DISP_NR.  Suppress ALIAS_DISP_NR.
bool DispGraph::alias(Widget w, int disp_nr, int alias_disp_nr)
{
    DispNode *d0 = get(disp_nr);
    DispNode *dn = get(alias_disp_nr);

    if (d0 == 0 || dn == 0)
	return false;

    if (!dn->active())
    {
	// Already hidden because it is out of scope
	return false;
    }

    if (dn->clustered())
    {
	// Already hidden because it is clustered
	return false;
    }

    if (dn->hidden() && dn->alias_of == disp_nr)
    {
	// Already hidden as alias of DISP_NR
	return false;
    }

    if (dn->hidden())
	unalias(alias_disp_nr);

    // Hide alias
    dn->hidden() = true;
    dn->alias_of = disp_nr;

    // Clear any special selections in this alias
    dn->select();

    // Hide ordinary hints and insert new alias edges
    GraphEdge *edge;
    GraphNodePointerArray from_nodes;
    GraphNodePointerArray to_nodes;
    EdgeAnnotationPointerArray from_annotations;
    EdgeAnnotationPointerArray to_annotations;
    int i;

    for (edge = dn->firstFrom(); edge != 0; edge = dn->nextFrom(edge))
    {
	GraphEdge *e = edge;
	while (e->to()->isHint())
	{
	    e->to()->hidden() = true;
	    e = e->to()->firstFrom();
	}
	to_nodes.push_back(e->to());

	EdgeAnnotation *anno = 0;
	LineGraphEdge *ge = ptr_cast(LineGraphEdge, e);
	if (ge != 0)
	    anno = ge->annotation();
	to_annotations.push_back(anno);
    }
    for (edge = dn->firstTo(); edge != 0; edge = dn->nextTo(edge))
    {
	GraphEdge *e = edge;
	while (e->from()->isHint())
	{
	    e->from()->hidden() = true;
	    e = e->from()->firstTo();
	}
	from_nodes.push_back(e->from());

	EdgeAnnotation *anno = 0;
	LineGraphEdge *ge = ptr_cast(LineGraphEdge, e);
	if (ge != 0)
	    anno = ge->annotation();
	from_annotations.push_back(anno);
    }

    for (i = 0; i < int(to_nodes.size()); i++)
	add_alias_edge(w, alias_disp_nr, 
		       d0, to_nodes[i], to_annotations[i]);
    for (i = 0; i < int(from_nodes.size()); i++)
	add_alias_edge(w, alias_disp_nr, 
		       from_nodes[i], d0, from_annotations[i]);

    // Propagate `selected' state to hints
    for (GraphNode *node = firstNode(); node != 0; node = nextNode(node))
    {
	if (!node->isHint())
	    continue;
	AliasGraphEdge *edge = ptr_cast(AliasGraphEdge, node->firstTo());
	if (edge == 0)
	    continue;
	if (edge->disp_nr() == alias_disp_nr)
	    node->selected() = dn->selected();
    }

    return true;
}


// Un-alias ALIAS_DISP_NR.  Unsuppress ALIAS_DISP_NR.  Return true iff
// changed.
bool DispGraph::unalias(int alias_disp_nr)
{
    DispNode *dn = get(alias_disp_nr);
    if (dn == 0 || !dn->active() || dn->clustered())
	return false;

    if (!dn->hidden())
	return false;

    // Unsuppress display
    dn->hidden() = false;

    // Delete all alias edges associated with this node
    std::vector<AliasGraphEdge*> kill_edges;
    GraphEdge *edge;
    for (edge = firstEdge(); edge != 0; edge = nextEdge(edge))
    {
	AliasGraphEdge *e = ptr_cast(AliasGraphEdge, edge);
	if (e != 0 && e->disp_nr() == alias_disp_nr)
	    kill_edges.push_back(e);
    }
    for (int i = 0; i < int(kill_edges.size()); i++)
    {
	AliasGraphEdge *e = kill_edges[i];
	if (e->to()->isHint())
	{
	    *this -= e->to();	// Also removes E from graph
	    delete e->to();
	}
	else
	{
	    *this -= e;
	}
	delete e;
    }

    // Unsuppress remaining (ordinary) hints
    for (edge = dn->firstFrom(); edge != 0; edge = dn->nextFrom(edge))
    {
	GraphEdge *e = edge;
	while (e->to()->isHint())
	{
	    e->to()->hidden() = false;
	    e = e->to()->firstFrom();
	}
    }
    for (edge = dn->firstTo(); edge != 0; edge = dn->nextTo(edge))
    {
	GraphEdge *e = edge;
	while (e->from()->isHint())
	{
	    e->from()->hidden() = false;
	    e = e->from()->firstTo();
	}
    }

    dn->alias_of = 0;
    return true;
}



//-----------------------------------------------------------------------------
// Routing
//-----------------------------------------------------------------------------

// True iff R->P1 and R->P2 have the same angle
bool DispGraph::same_angle(const BoxPoint& r,
			   const BoxPoint& p1,
			   const BoxPoint& p2)
{
    if (p1 == r || p2 == r)
	return false;		// No angle to determine

    double angle1 = atan2(double(r[Y] - p1[Y]), double(r[X] - p1[X]));
    double angle2 = atan2(double(r[Y] - p2[Y]), double(r[X] - p2[X]));

    const double epsilon = 0.1;
    return fabs(angle1 - angle2) < epsilon;
}

// True iff NODE is attached to an edge with the same angle as P
bool DispGraph::has_angle(PosGraphNode *node, const BoxPoint& p)
{
    GraphEdge *edge;
    for (edge = node->firstFrom(); edge != 0; edge = node->nextFrom(edge))
    {
	if (same_angle(node->pos(), edge->to()->pos(), p))
	    return true;
    }
    for (edge = node->firstTo(); edge != 0; edge = node->nextTo(edge))
    {
	if (same_angle(node->pos(), edge->from()->pos(), p))
	    return true;
    }

    return false;
}

// Add a new edge in existing graph
void DispGraph::add_alias_edge(Widget w, int alias_disp_nr, 
			       GraphNode *_from, GraphNode *_to,
			       EdgeAnnotation *anno)
{
    PosGraphNode *from = ptr_cast(PosGraphNode, _from);
    PosGraphNode *to   = ptr_cast(PosGraphNode, _to);

    if (anno != 0)
	anno = anno->dup();

    // Check whether the new edge may be hidden by existing edges
    if (from == to || (from == 0 || to == 0))
    {
	// Self-referring edge or bad nodes
	add_direct_alias_edge(w, alias_disp_nr, _from, _to, anno);
    }
    else
    {
	// Check for interferences with existing edge
	add_routed_alias_edge(w, alias_disp_nr, from, to, anno);
    }
}

// Add a direct edge from FROM to TO
void DispGraph::add_direct_alias_edge(Widget, int alias_disp_nr, 
				      GraphNode *from, GraphNode *to,
				      EdgeAnnotation *anno)
{
    *this += new AliasGraphEdge(alias_disp_nr, from, to, anno);
}

// Check whether P is obscured by any node
bool DispGraph::is_hidden(Widget w, const BoxPoint& p) const
{
    const GraphGC& graphGC = graphEditGetGraphGC(w);

    for (GraphNode *n = firstVisibleNode(); n != 0; n = nextVisibleNode(n))
    {
	RegionGraphNode *node = ptr_cast(RegionGraphNode, n);
	if (node == 0)
	    continue;

	if (p == node->pos() || p <= node->sensitiveRegion(graphGC))
	    return true;
    }

    return false;
}

// Rotate offset P by ANGLE (in degrees)
BoxPoint DispGraph::rotate_offset(const BoxPoint& p, int angle)
{
    if (p == BoxPoint(0, 0))
	return p;

    double length = hypot(p[X], p[Y]);
    double alpha  = atan2(double(p[X]), double(p[Y]));

    alpha += (2.0 * PI * angle / 360.0);

    return BoxPoint(BoxCoordinate(length * cos(alpha)), 
		    BoxCoordinate(length * sin(alpha)));
}


// Check whether POS1 and POS2 are okay as hint positions for FROM and TO
bool DispGraph::hint_positions_ok(Widget w,
				  PosGraphNode *from,
				  PosGraphNode *to,
				  const BoxPoint& pos1,
				  const BoxPoint& pos2) const
{
    BoxPoint p1 = graphEditFinalPosition(w, pos1);
    BoxPoint p2 = graphEditFinalPosition(w, pos2);

    if (p1[X] <= 0 || p2[X] <= 0 || p1[Y] <= 0 || p2[Y] <= 0)
	return false;		// Bad coordinates
	
    if (p1 == from->pos() && p2 == to->pos())
    {
	// Direct edge
	if (has_angle(from, to->pos()))
	    return false;	// New edge obscured by existing edge
	if (has_angle(to, from->pos()))
	    return false;	// New edge obscured by existing edge
    }
    else
    {
	// One or two hints
	if (has_angle(from, p1))
	    return false;	// New edge obscured by existing edge
	if (has_angle(to, p2))
	    return false;	// New edge obscured by existing edge
	if (is_hidden(w, p1))
	    return false;	// Hint obscured by existing node
	if (is_hidden(w, p2))
	    return false;	// Hint obscured by existing node
    }

    BoxPoint dist = p2 - p1;
    if (dist[X] > 0 || dist[Y] > 0)
    {
	BoxPoint center = graphEditFinalPosition(w, p1 + dist / 2);
	if (is_hidden(w, center))	// Center obscured by existing node
	    return false;
    }

    return true;
}


// Add edge from FROM to TO, inserting hints if required
void DispGraph::add_routed_alias_edge(Widget w, int alias_disp_nr, 
				      PosGraphNode *from, PosGraphNode *to,
				      EdgeAnnotation *anno)
{
    // Determine hint offsets
    Dimension grid_height = 16;
    Dimension grid_width  = 16;
    XtVaGetValues(w,
		  XtNgridHeight, &grid_height,
		  XtNgridWidth,  &grid_width,
		  XtPointer(0));

    BoxPoint dist   = to->pos() - from->pos();
    BoxPoint center = from->pos() + dist / 2;
    double angle    = atan2(double(dist[X]), double(dist[Y]));
    BoxPoint grid_offset(BoxCoordinate(grid_width  * cos(angle)),
			 BoxCoordinate(grid_height * sin(angle)));

    const int LEFT  = 1;
    const int RIGHT = 0;

    BoxPoint offsets[2];
    offsets[LEFT]  = rotate_offset(grid_offset, +90);
    offsets[RIGHT] = rotate_offset(grid_offset, -90);

#if 0
    std::clog << "offsets[LEFT]  = " << offsets[LEFT]  << "\n";
    std::clog << "offsets[RIGHT] = " << offsets[RIGHT] << "\n";
#endif

    // Try hint offsets
    BoxPoint pos1, pos2;
    bool found = false;

    const bool try_direct = false;

    const int max_iterations = 100;
    for (int i = 0; i < max_iterations && !found; i++)
    {
	for (int side = RIGHT; !found && side <= LEFT; side++)
	{
	    BoxPoint offset = offsets[side] * i;

	    if (try_direct && i == 0)
	    {
		// Try direct edge
		pos1 = from->pos() + offset;
		pos2 = to->pos()   + offset;
	    }		
	    else
	    {
		// Try one-hint edge
		pos1 = pos2 = center + offset;
	    }
#if 0
	    std::clog << "#" << i << " - "
		      << (side == LEFT ? "left side:  " : "right side: ")
		      << "trying pos1 = " << pos1 
		      << " and pos2 = " << pos2 << "\n";
#endif

	    found = hint_positions_ok(w, from, to, pos1, pos2);
	}
    }

    if (!found)
    {
	// Give up
	std::cerr << "Warning: could not find edge route after " 
	     << max_iterations << " iterations\n";
	pos1 = from->pos();
	pos2 = to->pos();
    }

    if (try_direct && pos1 == from->pos() && pos2 == to->pos())
    {
	// No need for hints - add direct edge
	add_direct_alias_edge(w, alias_disp_nr, from, to, anno);
    }
    else
    {
#if 0
	assert(!try_direct || pos1 == pos2);
#endif

	// Add single hint
	HintGraphNode *hint = new HintGraphNode(pos1);
	hint->hidden() = from->hidden() || to->hidden();
	*this += hint;

	// Add edges
	*this += new AliasGraphEdge(alias_disp_nr, from, hint, anno);
	*this += new AliasGraphEdge(alias_disp_nr, hint, to);
    }
}



//-----------------------------------------------------------------------------
// Display clustering
//-----------------------------------------------------------------------------

// Hide/Unhide all alias edges of NODE according to its status
void DispGraph::update_alias_edges(DispNode *node)
{
    for (GraphEdge *edge = firstEdge(); edge != 0; edge = nextEdge(edge))
    {
	AliasGraphEdge *e = ptr_cast(AliasGraphEdge, edge);
	if (e != 0 && e->disp_nr() == node->disp_nr())
	{
	    if (e->to()->isHint())
		e->to()->hidden() = node->hidden();
	    e->hidden() = node->hidden();
	}
    }
}

void DispGraph::cluster(DispNode *dn, int cluster)
{
    dn->cluster(cluster);
    update_alias_edges(dn);
}


//-----------------------------------------------------------------------------
// Display activation
//-----------------------------------------------------------------------------

bool DispGraph::hide_inactive_displays = true;

bool DispGraph::make_inactive(DispNode *dn)
{
    if (dn->active() && dn->enabled())
    {
	if (!hide_inactive_displays)
	{
	    dn->disable();
	}
	else
	{
	    dn->make_inactive();
	    update_alias_edges(dn);
	}
	return true;
    }

    return false;
}

bool DispGraph::make_active(DispNode *dn)
{
    if (!dn->active())
    {
	dn->make_active();
	update_alias_edges(dn);

	return true;
    }

    return false;
}



//-----------------------------------------------------------------------------
// Title stuff
//-----------------------------------------------------------------------------

bool DispGraph::refresh_titles() const
{
    bool changed = false;

    MapRef ref;
    for (DispNode *dn = first(ref); dn != 0; dn = next(ref))
    {
	bool is_dependent = false;
	for (GraphEdge *e = dn->firstTo(); e != 0; e = dn->nextTo(e))
	{
	    if (e->from() == dn)
		continue;	// Self edge
	    if (ptr_cast(AliasGraphEdge, e) != 0)
		continue;	// Alias edge
	    if (e->from()->hidden())
		continue;	// Invisible edge

	    LineGraphEdge *le = ptr_cast(LineGraphEdge, e);
	    if (le != 0 && le->annotation() != 0)
	    {
		is_dependent = true;
		break;
	    }
	}

	bool need_title = false;
	if (is_dependent && app_data.show_dependent_display_titles)
	    need_title = true;
	else if (!is_dependent && app_data.show_base_display_titles)
	    need_title = true;

	if (dn->set_title(need_title))
	    changed = true;
    }

    return changed;
}


//-----------------------------------------------------------------------------
// Plotting
//-----------------------------------------------------------------------------

// Print all plots
void DispGraph::print_plots(const string& filename, const GraphGC& gc) const
{
    for (GraphNode *node = firstVisibleNode(); node != 0; 
	 node = nextVisibleNode(node))
    {
	DispNode *dn = ptr_cast(DispNode, node);
	if (dn == 0)
	    continue;

	if (gc.printSelectedNodesOnly && !dn->selected())
	    continue;

	dn->print_plots(filename, gc);
    }
}
