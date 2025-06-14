// $Id$
// Own converters

// Copyright (C) 1995-1997 Technische Universitaet Braunschweig, Germany.
// Copyright (C) 2001, 2003 Free Software Foundation, Inc.
// Copyright (C) 2001 Universitaet des Saarlandes, Germany.
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

char converters_rcsid[] = 
    "$Id$";

#include <X11/Xlib.h>
#include <X11/Intrinsic.h>
#include <X11/IntrinsicP.h>
#include <X11/StringDefs.h>
#include <stdlib.h>
#include <unistd.h>
#include <pwd.h>
#include <ctype.h>

#include <X11/CoreP.h>

#include "base/assert.h"
#include "base/bool.h"
#include "base/cook.h"
#include "base/home.h"
#include "base/isid.h"
#include "BindingS.h"
#include "OnOff.h"
#include "base/strclass.h"
#include "charsets.h"
#include "template/StringSA.h"
#include "string-fun.h"
#include "motif/MString.h"

#include <Xm/Xm.h>

#define new new_w
#define class class_w

#if XmVersion < 1002
#include <Xm/bitmaps.h>
#else

// Some reasonable defaults...
static unsigned char bitmaps [3][32] =
{
    {  0x22, 0x22, 0x88, 0x88, 0x22, 0x22, 0x88, 0x88,
       0x22, 0x22, 0x88, 0x88, 0x22, 0x22, 0x88, 0x88,
       0x22, 0x22, 0x88, 0x88, 0x22, 0x22, 0x88, 0x88,
       0x22, 0x22, 0x88, 0x88, 0x22, 0x22, 0x88, 0x88  },

    {  0xAA, 0xAA, 0x55, 0x55, 0xAA, 0xAA, 0x55, 0x55,
       0xAA, 0xAA, 0x55, 0x55, 0xAA, 0xAA, 0x55, 0x55,
       0xAA, 0xAA, 0x55, 0x55, 0xAA, 0xAA, 0x55, 0x55,
       0xAA, 0xAA, 0x55, 0x55, 0xAA, 0xAA, 0x55, 0x55  },

    {  0xFF, 0xFF, 0xAA, 0xAA, 0xFF, 0xFF, 0x55, 0x55,
       0xFF, 0xFF, 0xAA, 0xAA, 0xFF, 0xFF, 0x55, 0x55,
       0xFF, 0xFF, 0xAA, 0xAA, 0xFF, 0xFF, 0x55, 0x55,
       0xFF, 0xFF, 0xAA, 0xAA, 0xFF, 0xFF, 0x55, 0x55  }
};

static const _XtString const bitmap_name_set[] =
{
   "25_foreground",
   "50_foreground",
   "75_foreground"
};

#endif

// Decl of XmIsSlowSubclass in Motif 1.1 is not C++-aware, hence extern "C"
extern "C" {
#include <Xm/XmP.h>

// <Xm/PrimitiveP.h> only exists in Motif 1.2 and later
#if XmVersion >= 1002
#include <Xm/PrimitiveP.h>
#endif
}

#undef new
#undef class

#include "converters.h"
#include "motif/MString.h"


// ANSI C++ doesn't like the XtIsRealized() macro
#ifdef XtIsRealized
#undef XtIsRealized
#endif


// Do we want to define our own @STRING@ to font converters?
#define OWN_FONT_CONVERTERS 0

// Declarations

// Convert String to Widget
static Boolean CvtStringToWidget(Display *display, 
				 XrmValue *args, Cardinal *num_args, 
				 XrmValue *fromVal, XrmValue *toVal,
				 XtPointer *converter_data);

// Convert String to Pixmap, converting 1s and 0s to fg/bg color
static Boolean CvtStringToPixmap(Display *display, 
				 XrmValue *args, Cardinal *num_args, 
				 XrmValue *fromVal, XrmValue *toVal,
				 XtPointer *converter_data);

// Convert String to Bitmap, leaving 1s and 0s untouched
static Boolean CvtStringToBitmap(Display *display, 
				 XrmValue *args, Cardinal *num_args, 
				 XrmValue *fromVal, XrmValue *toVal,
				 XtPointer *converter_data);

// Convert String to XmString, using `@' for font specs
static Boolean CvtStringToXmString(Display *display, 
				   XrmValue *args, Cardinal *num_args, 
				   XrmValue *fromVal, XrmValue *toVal,
				   XtPointer *converter_data);

// Convert String to Alignment
static Boolean CvtStringToAlignment(Display *display, 
				    XrmValue *args, Cardinal *num_args, 
				    XrmValue *fromVal, XrmValue *toVal,
				    XtPointer *converter_data);

// Convert String to Orientation
static Boolean CvtStringToOrientation(Display *display, 
				      XrmValue *args, Cardinal *num_args, 
				      XrmValue *fromVal, XrmValue *toVal,
				      XtPointer *converter_data);

// Convert String to Packing
static Boolean CvtStringToPacking(Display *display, 
				  XrmValue *args, Cardinal *num_args, 
				  XrmValue *fromVal, XrmValue *toVal,
				  XtPointer *converter_data);



// Return a value of given type
#define done(type, value) \
    do {                                                \
        if (toVal->addr != 0) {                         \
            if (toVal->size < sizeof(type)) {           \
                toVal->size = sizeof(type);             \
                return False;                           \
            }                                           \
            *(type *)(toVal->addr) = (value);           \
        }                                               \
        else {                                          \
            static type static_val;                     \
            static_val = (value);                       \
            toVal->addr = (XPointer)&static_val;        \
        }                                               \
                                                        \
        toVal->size = sizeof(type);                     \
        return True;                                    \
    } while(0)

#define donef(type,value,failure) \
    do {                                                \
        if (toVal->addr != 0) {                         \
            if (toVal->size < sizeof(type)) {           \
                toVal->size = sizeof(type);             \
                failure;                                \
                return False;                           \
            }                                           \
            *(type *)(toVal->addr) = (value);           \
        }                                               \
        else {                                          \
            static type static_val;                     \
            static_val = (value);                       \
            toVal->addr = (XPointer)&static_val;        \
        }                                               \
                                                        \
        toVal->size = sizeof(type);                     \
        return True;                                    \
    } while(0)

// Return string of value.  If STRIP is set, strip leading and
// trailing whitespace.
static string str(XrmValue *from, bool strip)
{
    // Use the length given in FROM->size; strip trailing '\0'
    const _XtString s = (String)from->addr;
    int sz   = from->size;
    if (sz > 0 && s[sz - 1] == '\0')
	sz--;
    string v(s, sz);

    if (strip)
    {
	// Strip leading and trailing space.  Pinwu Xu
	// <pxu@perigee.net> reports a `Warning: Cannot convert string
	// "false " to type OnOff'.  No idea where the trailing space in
	// `"false "' comes from, so remove it here.
	while (v.length() > 0 && isspace(v[0]))
	    v = v.after(0);
	while (v.length() > 0 && isspace(v[v.length() - 1]))
	    v = v.before(int(v.length()) - 1);
    }

    return v;
}

// Convert String to Widget
// This is based on Asente/Swick: The X Window System Toolkit,
// Digital Press, Example 3.9

Boolean CvtStringToWidget(Display *display, 
			  XrmValue *args, Cardinal *num_args, 
			  XrmValue *fromVal, XrmValue *toVal,
			  XtPointer *)
{
    // Convert first arg into parent    
    if (*num_args != 1) 
    {
	XtAppErrorMsg(XtDisplayToApplicationContext(display),
	    "wrongParameters", "CvtStringToWidget",
	    "XtToolkitError",
	    "String to Widget conversion needs parent arg",
	    (String *)0, (Cardinal *)0);
    }
    Widget parent = *(Widget *) args[0].addr;

    // Get widget
    const string value = str(fromVal, false);
    Widget w = XtNameToWidget(parent, value.chars());
    if (w == 0)
    {
	XtDisplayStringConversionWarning(display, fromVal->addr, XtRWidget);
	return False;
    }

    done(Widget, w);
}


// Convert String to Pixmap (using XmGetPixmap)
// A Pixmap will be read in as bitmap file
// 1 and 0 values are set according to the widget's 
// foreground/background colors.
static Boolean CvtStringToPixmap(Display *display, 
				 XrmValue *args, Cardinal *num_args, 
				 XrmValue *fromVal, XrmValue *toVal,
				 XtPointer *)
{
    // Default parameters
    Screen *screen   = DefaultScreenOfDisplay(display);
    Pixel background = WhitePixelOfScreen(screen);
    Pixel foreground = BlackPixelOfScreen(screen);

    if (*num_args >= 1)
    {
	// convert first arg into widget
	Widget w = *(Widget *) args[0].addr;
	background = w->core.background_pixel;
	
	screen = XtScreen(w);
	
	if (XtIsWidget(w) && XmIsPrimitive(w))
	{
	    // Get foreground color from widget
	    foreground = XmPrimitiveWidget(w)->primitive.foreground;
	}
	else
	{
	    // Ask Motif for a default foreground color
	    Pixel newfg, newtop, newbot, newselect;
	    XmGetColors(screen, w->core.colormap, background,
			&newfg, &newtop, &newbot, &newselect);
	    foreground = newfg;
	}
    }

    // Get pixmap
    Pixmap p = XmUNSPECIFIED_PIXMAP;

    string value = str(fromVal, false);

    // Some Motif versions use `unspecified_pixmap' and `unspecified pixmap'
    // as values for XmUNSPECIFIED_PIXMAP.  Check for this.
    string v = downcase(value);
    v.gsub(" ", "_");
    if (v.contains("xm", 0))
	v = v.after("xm");
    if (v != "unspecified_pixmap")
    {
	p = XmGetPixmap(screen, XMST(value.chars()), foreground, background);

	if (p == XmUNSPECIFIED_PIXMAP)
	{
	    XtDisplayStringConversionWarning(display, fromVal->addr, 
					     XmRPixmap);
	    return False;
	}
    }

    done(Pixmap, p);
}



static String locateBitmap(Display *display, const _XtString basename);

// Convert String to Bitmap
// A Bitmap will be read in as bitmap file -- 1 and 0 values remain unchanged.
static Boolean CvtStringToBitmap(Display *display, 
				 XrmValue *, Cardinal *, 
				 XrmValue *fromVal, XrmValue *toVal,
				 XtPointer *)
{
    // Fetch a drawable
    Window window = None;

#if 0
    if (*num_args >= 1)
    {
	// convert first arg into widget
	Widget w = *(Widget *) args[0].addr;
	if (XtIsRealized(w))
	    window = XtWindow(w);
    }
#endif

    if (window == None)
	window = DefaultRootWindow(display);

    // Locate file
    const string basename = str(fromVal, false);
    String filename = locateBitmap(display, basename.chars());
    if (filename == 0)
    {
	// Cannot find file -- check for predefined motif bitmaps
	for (Cardinal i = 0; i < XtNumber(bitmap_name_set); i++)
	{
	    if (basename == bitmap_name_set[i])
	    {
		Pixmap bitmap = XCreateBitmapFromData(display, window,
						      (char *)(bitmaps[i]),
						      16, 16);
		if (bitmap == None)
		    break;

		done(Pixmap, bitmap);
	    }
	}

	// Cannot find file and no predefined bitmap found
	XtDisplayStringConversionWarning(display, fromVal->addr, XtRBitmap);
	return False;
    }


    // create pixmap
    unsigned int width, height;
    Pixmap bitmap;
    int x_hot, y_hot;
    int success = XReadBitmapFile(display, window, filename, 
				  &width, &height, &bitmap, &x_hot, &y_hot);
    if (success != BitmapSuccess)
    {
	XtDisplayStringConversionWarning(display, fromVal->addr, XtRBitmap);
	XtFree(filename);
	return False;
    }

    done(Pixmap, bitmap);
}

// Note: <percent>B<percent> is expanded by SCCS -- thus inserting ""
static const string BASENAME = "%B""%S";
static const string DELIMITER = ":";

// add default search paths to path
static void addDefaultPaths(string& path, const char *root)
{
    path += DELIMITER + root + "/%L/%T/%N/" + BASENAME;
    path += DELIMITER + root + "/%l/%T/%N/" + BASENAME;
    path += DELIMITER + root + "/%T/%N/"    + BASENAME;
    path += DELIMITER + root + "/%L/%T/"    + BASENAME;
    path += DELIMITER + root + "/%l/%T/"    + BASENAME;
    path += DELIMITER + root + "/%T/"       + BASENAME;
}

// locate path
// this mimics XmGetPixmap's efforts to build a path
static string bitmapPath()
{
    static string path = "";

    if (!path.empty())
	return path;

    path = BASENAME;
    const char *xbmlangpath = getenv("XBMLANGPATH");
    if (xbmlangpath == 0)
    {
	const char *xapplresdir = getenv("XAPPLRESDIR");
	const string home = gethome();

	if (xapplresdir != 0)
	    addDefaultPaths(path, xapplresdir);
	else
	    addDefaultPaths(path, home.chars());

	path += DELIMITER + home + BASENAME;
	addDefaultPaths(path, "/usr/lib/X11");
	path += DELIMITER + "/usr/include/X11/%T/" + BASENAME;
    }
    else
	path = xbmlangpath;

    return path;
}

static const string PATH = bitmapPath();

// locate bitmap
// this mimics XmGetPixmap's efforts to locate a path
static String locateBitmap(Display *display, const _XtString basename)
{
    SubstitutionRec subst;
    subst.match        = 'B';
    subst.substitution = CONST_CAST(char*,basename);

    return XtResolvePathname(
	display,      // the display we use
	"bitmaps",    // %T = bitmaps
	String(0),    // %N = application class name
	"",           // %S = "" (suffix)
	PATH.chars(), // path to use
	&subst, 1,    // %B = basename
	XtFilePredicate(0)); // no checking for valid bitmap
}

// Macro tables
static StringStringAssoc conversionMacroTable;

// Return length of leading font id
static int font_id_len(const string& s)
{
    // Font ids have the syntax "[_A-Za-z][-_A-Za-z0-9]*"

    if (s.empty())
	return 0;

    if (s[0] != '_' && !isalpha(s[0]))
	return 0;

    for (int i = 1; i < int(s.length()); i++)
	if (s[i] != '-' && !isid(s[i]))
	    return i;

    return s.length();
}


static void CvtStringToXmStringDestroy(XtAppContext /* app */,
				       XrmValue* to,
				       XtPointer /* converter_data */,
				       XrmValue * /* args */,
				       Cardinal* /* num_args */
				       )
{
  XmStringFree(*(XmString*)to->addr);
  return;
}

// Convert String to XmString, using `@' for font specs: `@FONT TEXT'
// makes TEXT be displayed in font FONT; a single space after FONT is
// eaten.  `@ ' displays a single `@'.
static Boolean CvtStringToXmString(Display *display, 
				   XrmValue *, Cardinal *, 
				   XrmValue *fromVal, XrmValue *toVal,
				   XtPointer *)
{
    static const string font_esc = "@";

    // Get string
    const string source = str(fromVal, false);
    string charset = (String)MSTRING_DEFAULT_CHARSET;

    const int n_segments = source.freq(font_esc) + 1;
    string *segments = new string[n_segments];
    
    split(source, segments, n_segments, font_esc);

    MString buf(0, true);
    string txtbuf;
    for (int i = 0; i < n_segments; i++)
    {
        string segment;

	// set segment
	if (i == 0)
	{
	    // At beginning of text
	    segment = segments[i];
	}
	else
	{
	    int len = font_id_len(segments[i]);
	    if (len > 0)
	    {
		// Found @[font-id] <segment>: process it
		const string c = segments[i].before(len);
		segment = segments[i].from(int(c.length()));
		if (segment.empty())
		{
		    // Found @MACRO@
		    if (conversionMacroTable.has(c))
		    {
			// Replace @MACRO@ by value
		        segment = conversionMacroTable[c];
		        segment += segments[++i];
		    }
		    else
		    {
			// No such macro
			Cardinal num_params = 1;
			String params = CONST_CAST(char*,c.chars());
			XtAppWarningMsg(XtDisplayToApplicationContext(display),
					"noSuchMacro", "CvtStringToXmString",
					"XtToolkitError",
					"No such macro: @%s@",
					&params, &num_params);
			segment.prepend(font_esc);
			segment += font_esc;
		    }
		}
		else
		{
		    // Found @CHARSET: set new charset
		    charset = c;
		    if (segment.contains(' ', 0) || segment.contains('\t', 0))
			segment = segment.after(0);
		}
	    }
	    else if (segments[i].contains(' ', 0))
	    {
		// found @[space]: remove space
		segment = font_esc;
		segment += segments[i].from(1);
	    }
	    else
	    {
		// found @[anything-else]: ignore @, take remainder literally
		segment = segments[i];
	    }
	}

	// parse segment and set bud and textbuf
	while (segment.contains('\n'))
	{
	    const string seg = segment.before('\n');
	    segment = segment.after('\n');

	    buf    += MString(seg.chars(), 
			      CONST_CAST(XmStringCharSet,charset.chars()))
		   + cr();
	    txtbuf += seg + '\n';
	}

	if (segment.length() > 0)
	{
	    buf    += MString(segment, CONST_CAST(XmStringCharSet,charset.chars()));
	    txtbuf += segment;
	}
    }

    if (buf.isNull())
	buf = MString("");

    XmString target = XmStringCopy(buf.xmstring());
    delete[] segments;

    const string bufstr(buf.str());
    if (bufstr != txtbuf)
    {
	std::cerr << "Conversion error:\n"
	     << quote(source) << "\n"
	     << "should be\n"
	     << quote(txtbuf) << "\n"
	     << "but is"
	     << quote(bufstr) << "\n";
    }

    if (target == 0)
    {
	XtDisplayStringConversionWarning(display, fromVal->addr, XmRXmString);
	return False;
    }

    donef(XmString, target, XmStringFree(target));
}


#if OWN_FONT_CONVERTERS

static bool convert_fontspec(Display *display,
			     string& fontspec, const string& name)
{
    if (fontspec.contains('@', 0))
    {
	string c = fontspec.after('@');
	c = c.before('@');
	if (conversionMacroTable.has(c))
	{
	    // Replace macro by value
	    fontspec = conversionMacroTable[c];
	}
	else
	{
	    // No such macro
	    Cardinal num_params = 1;
	    String params = CONST_CAST(char*,c.chars());
	    XtAppWarningMsg(XtDisplayToApplicationContext(display),
			    "noSuchMacro", name.chars(),
			    "XtToolkitError",
			    "No such macro: @%s@",
			    &params, &num_params);
	    return false;
	}
    }

    return true;
}


// Convert String to FontStruct, relacing `@NAME@' by symbolic font specs.
static Boolean CvtStringToFontStruct(Display *display, 
				     XrmValue *, Cardinal *, 
				     XrmValue *fromVal, XrmValue *toVal,
				     XtPointer *)
{
    string fontspec = str(fromVal, false);
    strip_space(fontspec);

    if (!convert_fontspec(display, fontspec, "CvtStringToFontStruct"))
	return False;

    XFontStruct *font = XLoadQueryFont(display, fontspec.chars());
    if (font == 0)
    {
	Cardinal num_params = 1;
	String params = CONST_CAST(char*,fontspec.chars());
	XtAppWarningMsg(XtDisplayToApplicationContext(display),
			"noSuchFont", "CvtStringToFontStruct",
			"XtToolkitError",
			"No such font: %s",
			&params, &num_params);
	return False;
    }

    done(XFontStruct *, font);
}

static void
CvtStringToXmFontListDestroy(XtAppContext /* app */,
			     XrmValue* to,
			     XtPointer /* converter_data */,
			     XrmValue* /* args */,
			     Cardinal* /* num_args */
			     )
{
  XmFontListFree( *((XmFontList *) to->addr));
  return;
}

// Convert String to FontList, relacing `@NAME@' by symbolic font specs.
static Boolean CvtStringToXmFontList(Display *display, 
				     XrmValue *, Cardinal *, 
				     XrmValue *fromVal, XrmValue *toVal,
				     XtPointer *)
{
    // Get string
    const string source = str(fromVal, false);
    const int n_segments = source.freq(',') + 1;
    string *segments = new string[n_segments];
    
    split(source, segments, n_segments, ',');

    XmRenderTable target = 0;
    for (int i = 0; i < n_segments; i++)
    {
	const string& segment = segments[i];
	string fontspec = segment.before('=');
	string charset  = segment.after('=');

	if (!segment.contains('='))	// "fixed" occurs in Motif 1.1
	{
	    fontspec = segment;
	    charset  = MSTRING_DEFAULT_CHARSET;
	}

	strip_space(fontspec);
	strip_space(charset);

	if (fontspec.empty() || 
	    (charset.empty() && charset != MSTRING_DEFAULT_CHARSET))
	{
	    Cardinal num_params = 1;
	    String params = CONST_CAST(char*,segment.chars());
	    XtAppWarningMsg(XtDisplayToApplicationContext(display),
			    "syntaxError", "CvtStringToXmFontList",
			    "XtToolkitError",
			    "Syntax error in %s",
			    &params, &num_params);
	    continue;
	}

	if (!convert_fontspec(display, fontspec, "CvtStringToXmFontList"))
	    continue;

	XmFontListEntry entry = XmFontListEntryLoad(display, XMST(fontspec.chars()),
						    XmFONT_IS_FONT, (char*)charset.chars());
        //printf("fontspec %s  charset %s\n", fontspec.chars(), charset.chars());
	target = XmFontListAppendEntry(target, entry);
	XmFontListEntryFree(&entry);
    }

    delete[] segments;

    if (target == 0)
    {
	XtDisplayStringConversionWarning(display, fromVal->addr, XmRXmString);
	return False;
    }
    
    donef(XmFontList, target, XmFontListFree(target));
}

#endif


// Convert the strings 'beginning', 'center' and 'end' in any case to a value
// suitable for the specification of the XmNentryAlignment-resource in 
// RowColumn-widgets (or anything else using alignment-resources)
static Boolean CvtStringToAlignment(Display*   display, 
				    XrmValue*  ,
				    Cardinal*  , 
				    XrmValue*  fromVal,
				    XrmValue*  toVal,
				    XtPointer* )
{
    string theAlignment = str(fromVal, true);
    theAlignment.downcase();

    if (theAlignment.contains("xm", 0))
	theAlignment = theAlignment.after("xm");
    if (theAlignment.contains("alignment_", 0))
	theAlignment = theAlignment.after("alignment_");

    if      (theAlignment == "beginning")
	done(unsigned char, XmALIGNMENT_BEGINNING);
    else if (theAlignment == "center")
	done(unsigned char, XmALIGNMENT_CENTER);
    else if (theAlignment == "end")
	done(unsigned char, XmALIGNMENT_END);

    XtDisplayStringConversionWarning(display, fromVal->addr, XmCAlignment);
    return False;
}


// Convert the strings 'vertical' and 'horizontal' in any case to a value
// suitable for the specification of the XmNorientation-resource in 
// RowColumn-widgets (or anything else using orientation-resources)
static Boolean CvtStringToOrientation(Display*         display, 
				      XrmValue*        ,
				      Cardinal*        , 
				      XrmValue*        fromVal,
				      XrmValue*        toVal,
				      XtPointer*       )
{
    string theOrientation = str(fromVal, true);
    theOrientation.downcase();
    if (theOrientation.contains("xm", 0))
	theOrientation = theOrientation.after("xm");
  
    if      (theOrientation == "vertical")
	done(unsigned char, XmVERTICAL);
    else if (theOrientation == "horizontal")
	done(unsigned char, XmHORIZONTAL);
    else if (theOrientation == "no_orientation")
	done(unsigned char, XmNO_ORIENTATION);
    else
    {
	XtDisplayStringConversionWarning(display, fromVal->addr, 
					 XmCOrientation);
	return False;
    }
}


// Convert the strings 'tight', 'column' and 'none' in any case to a value
// suitable for the specification of the XmNpacking-resource in
// RowColumn-widgets (or anything else using packing-resources)
static Boolean CvtStringToPacking(Display*     display, 
				  XrmValue*    ,
				  Cardinal*    , 
				  XrmValue*    fromVal,
				  XrmValue*    toVal,
				  XtPointer*   )
{
    string thePacking = str(fromVal, true);
    thePacking.downcase();
    if (thePacking.contains("xm", 0))
	thePacking = thePacking.after("xm");
    if (thePacking.contains("pack_", 0))
	thePacking = thePacking.after("pack_");
  
    if      (thePacking == "tight")
	done(unsigned char, XmPACK_TIGHT);
    else if (thePacking == "column")
	done(unsigned char, XmPACK_COLUMN);
    else if (thePacking == "none")
	done(unsigned char, XmPACK_NONE);


    XtDisplayStringConversionWarning(display, fromVal->addr, XmRPacking);
    return False;
}

// Convert the strings 'pixels', '100th_millimeters' and so on
// to unit types.
static Boolean CvtStringToUnitType(Display*     display, 
				   XrmValue*    ,
				   Cardinal*    , 
				   XrmValue*    fromVal,
				   XrmValue*    toVal,
				   XtPointer*   )
{
    string theType = str(fromVal, true);
    theType.downcase();
    if (theType.contains("xm", 0))
	theType = theType.after("xm");
  
    if      (theType == "pixels")
	done(unsigned char, XmPIXELS);
    else if (theType == "100th_millimeters")
	done(unsigned char, Xm100TH_MILLIMETERS);
    else if (theType == "1000th_inches")
	done(unsigned char, Xm1000TH_INCHES);
    else if (theType == "100th_points")
	done(unsigned char, Xm100TH_POINTS);
    else if (theType == "100th_font_units")
	done(unsigned char, Xm100TH_FONT_UNITS);

    XtDisplayStringConversionWarning(display, fromVal->addr, XmRUnitType);
    return False;
}

// Convert the strings 'on', `off', `auto' to OnOff values
Boolean CvtStringToOnOff(Display*     display, 
			 XrmValue*    ,
			 Cardinal*    , 
			 XrmValue*    fromVal,
			 XrmValue*    toVal,
			 XtPointer*   )
{
    string value = str(fromVal, true);
    value.downcase();
    value.gsub('_', ' ');

    if (value == "on" || value == "true" || value == "yes")
	done(OnOff, On);
    else if (value == "off" || value == "false" || value == "no")
	done(OnOff, Off);
    else if (value == "when needed" || value == "automatic" || value == "auto")
	done(OnOff, Auto);

    XtDisplayStringConversionWarning(display, fromVal->addr, XtROnOff);
    return False;
}

// Convert the strings 'motif', `kde' to BindingStyle values
static Boolean CvtStringToBindingStyle(Display*     display, 
				       XrmValue*    ,
				       Cardinal*    , 
				       XrmValue*    fromVal,
				       XrmValue*    toVal,
				       XtPointer*   )
{
    string value = str(fromVal, true);
    value.downcase();

    if (value == "kde")
	done(BindingStyle, KDEBindings);
    else if (value == "motif")
	done(BindingStyle, MotifBindings);

    XtDisplayStringConversionWarning(display, fromVal->addr, XtRBindingStyle);
    return False;
}

// Convert a string to Cardinal.
static Boolean CvtStringToCardinal(Display*     display, 
				   XrmValue*    ,
				   Cardinal*    , 
				   XrmValue*    fromVal,
				   XrmValue*    toVal,
				   XtPointer*   )
{
    string value = str(fromVal, true);
    char *ptr = 0;
    long val = strtol(value.chars(), &ptr, 0);
    if (ptr == value.chars() || val < 0)
    {
	XtDisplayStringConversionWarning(display, fromVal->addr, XtRCardinal);
	return False;
    }

    done(Cardinal, val);
}


// Register all converters
void registerOwnConverters()
{
    static XtConvertArgRec parentCvtArgs[] = {
    { XtWidgetBaseOffset, XtPointer(XtOffsetOf(CoreRec, core.parent)),
	  sizeof(CoreWidget) }
    };

    static XtConvertArgRec thisCvtArgs[] = {
    { XtBaseOffset, XtPointer(0), 
	  sizeof(Widget) }
    };

    // String -> Widget
    XtSetTypeConverter(XtRString, XtRWidget, CvtStringToWidget,
		       parentCvtArgs, XtNumber(parentCvtArgs), XtCacheNone,
		       XtDestructor(0));

    // String -> Window
    // We use CvtStringToWidget for conversions to "Window" as well,
    // since Motif widgets want a widget id in their "Window" fields.
    XtSetTypeConverter(XtRString, XtRWindow, CvtStringToWidget,
		       parentCvtArgs, XtNumber(parentCvtArgs), XtCacheNone,
		       XtDestructor(0));

    // String -> Pixmap
    XtSetTypeConverter(XtRString, XmRPixmap, CvtStringToPixmap,
		       thisCvtArgs, XtNumber(thisCvtArgs), XtCacheNone,
		       XtDestructor(0));

    // String -> GadgetPixmap
    XtSetTypeConverter(XtRString, XmRGadgetPixmap, CvtStringToPixmap,
		       thisCvtArgs, XtNumber(thisCvtArgs), XtCacheNone,
		       XtDestructor(0));

    // String -> PrimForegroundPixmap
    XtSetTypeConverter(XtRString, XmRPrimForegroundPixmap, CvtStringToPixmap,
		       thisCvtArgs, XtNumber(thisCvtArgs), XtCacheNone,
		       XtDestructor(0));

    // String -> ManForegroundPixmap
    XtSetTypeConverter(XtRString, XmRManForegroundPixmap, CvtStringToPixmap,
		       thisCvtArgs, XtNumber(thisCvtArgs), XtCacheNone,
		       XtDestructor(0));

    // String -> BackgroundPixmap
    XtSetTypeConverter(XtRString, XmRBackgroundPixmap, CvtStringToPixmap,
		       thisCvtArgs, XtNumber(thisCvtArgs), XtCacheNone,
		       XtDestructor(0));

    // String -> PrimHighlightPixmap
    XtSetTypeConverter(XtRString, XmRPrimHighlightPixmap, CvtStringToPixmap,
		       thisCvtArgs, XtNumber(thisCvtArgs), XtCacheNone,
		       XtDestructor(0));

    // String -> PrimTopShadowPixmap
    XtSetTypeConverter(XtRString, XmRPrimTopShadowPixmap, CvtStringToPixmap,
		       thisCvtArgs, XtNumber(thisCvtArgs), XtCacheNone,
		       XtDestructor(0));

    // String -> PrimBottomShadowPixmap
    XtSetTypeConverter(XtRString, XmRPrimBottomShadowPixmap, CvtStringToPixmap,
		       thisCvtArgs, XtNumber(thisCvtArgs), XtCacheNone,
		       XtDestructor(0));

    // String -> ManTopShadowPixmap
    XtSetTypeConverter(XtRString, XmRManTopShadowPixmap, CvtStringToPixmap,
		       thisCvtArgs, XtNumber(thisCvtArgs), XtCacheNone,
		       XtDestructor(0));

    // String -> ManBottomShadowPixmap
    XtSetTypeConverter(XtRString, XmRManBottomShadowPixmap, CvtStringToPixmap,
		       thisCvtArgs, XtNumber(thisCvtArgs), XtCacheNone,
		       XtDestructor(0));

    // String -> ManHighlightPixmap
    XtSetTypeConverter(XtRString, XmRManHighlightPixmap, CvtStringToPixmap,
		       thisCvtArgs, XtNumber(thisCvtArgs), XtCacheNone,
		       XtDestructor(0));

#if XmVersion >= 1002
    // String -> AnimationPixmap
    XtSetTypeConverter(XtRString, XmRAnimationPixmap, CvtStringToPixmap,
		       thisCvtArgs, XtNumber(thisCvtArgs), XtCacheNone,
		       XtDestructor(0));
#endif

    // String -> Bitmap
    XtSetTypeConverter(XtRString, XtRBitmap, CvtStringToBitmap,
		       thisCvtArgs, XtNumber(thisCvtArgs), XtCacheByDisplay,
		       XtDestructor(0));

    // String -> XmString
    XtSetTypeConverter(XmRString, XmRXmString, CvtStringToXmString,
		       XtConvertArgList(0), 0, (XtCacheAll | XtCacheRefCount),
		       CvtStringToXmStringDestroy);

#if OWN_FONT_CONVERTERS
    // String -> FontList
    XtSetTypeConverter(XmRString, XmRFontList, CvtStringToXmFontList,
		       XtConvertArgList(0), 0, (XtCacheAll | XtCacheRefCount),
		       CvtStringToXmFontListDestroy);

    // String -> FontStruct
    XtSetTypeConverter(XmRString, XtRFontStruct, CvtStringToFontStruct,
		       XtConvertArgList(0), 0, XtCacheAll,
		       XtDestructor(0));
#endif

    // String -> UnitType
    XtSetTypeConverter(XmRString, XmRUnitType, CvtStringToUnitType,
		       XtConvertArgList(0), 0, XtCacheAll, 
		       XtDestructor(0));

    // String -> OnOff
    XtSetTypeConverter(XmRString, XtROnOff, CvtStringToOnOff,
		       XtConvertArgList(0), 0, XtCacheAll, 
		       XtDestructor(0));

    // String -> BindingStyle
    XtSetTypeConverter(XmRString, XtRBindingStyle, CvtStringToBindingStyle,
		       XtConvertArgList(0), 0, XtCacheAll, 
		       XtDestructor(0));

    // String -> Cardinal
    XtSetTypeConverter(XmRString, XtRCardinal, CvtStringToCardinal,
		       XtConvertArgList(0), 0, XtCacheAll,
		       XtDestructor(0));

    // The following three were contributed by Thorsten Sommer
    // <sommer@ips.cs.tu-bs.de>

    // String -> Alignment
    XtSetTypeConverter(XtRString, XmNentryAlignment, CvtStringToAlignment,
		       parentCvtArgs, XtNumber(parentCvtArgs), XtCacheNone,
		       XtDestructor(0));

    // String -> Orientation
    XtSetTypeConverter(XtRString, XmNorientation, CvtStringToOrientation,
		       parentCvtArgs, XtNumber(parentCvtArgs), XtCacheNone,
		       XtDestructor(0));

    // String -> Packing
    XtSetTypeConverter(XtRString, XmNpacking, CvtStringToPacking,
		       parentCvtArgs, XtNumber(parentCvtArgs), XtCacheNone,
		       XtDestructor(0));
}

// Define a macro: @NAME@ will be replaced by VALUE in CvtStringToXmString
void defineConversionMacro(const _XtString name, const _XtString value)
{
    conversionMacroTable[name] = value;
}
