/* $Id$ -*- C++ -*- */
/* DDD regexp scanner */
%option prefix="ddd"
%option outfile="rxscan.C"
%option prefix="ddd"
%option 8bit
%option noyywrap

%{
// Copyright (C) 1997-1999 Technische Universitaet Braunschweig, Germany.
// Copyright (C) 2001 Universitaet Passau, Germany.
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

#include "regexps.h"
#include <algorithm>
    
char rxscan_rcsid[] = 
    "$Id$";

// Maximal length of a token when using LEX.
#define MAX_LEX_TOKEN_SIZE 8192

// I think that even a low limit of, say, 200 characters or less will
// make DDD work just fine.  Please note that MAX_LEX_TOKEN_SIZE
// effectively limits the length of the input to be matched, so if you
// set it to 4000, only the first 4000 characters of GDB output will be
// matched; the remaining ones will be ignored (by the regexp matcher,
// not by DDD).
//
// In most cases, it does not matter whether the first 80 or 200 or 8192
// characters of some GDB output are matched by the regular expression.
// I have run DDD with a deliberately low limit of 80 characters, and run
// into no problems whatsoever, even if only the first 80 characters were
// ever matched.  The reason is that most DDD regexps that go this far
// have the form `<something>.*', and it does not matter whether the
// trailing `.*' matches 80 or 8192 characters.  So, in practice, raising
// or lowering the MAX_LEX_TOKEN_SIZE won't make a big difference unless
// you make it lower than 20 characters or so.  (8192 is just to stay on
// the safe side; I don't have the time to verify all DDD regexps.)


static const char *the_prefix;	// Prefix
static const char *the_text;	// Pointer to next text character
static int the_length;		// Number of characters to read

// Input routines

#ifdef FLEX_SCANNER
// The way FLEX wants input
#undef YY_INPUT
#define YY_INPUT(buf, result, max_size) \
{\
    int k = 0; \
    while (*the_prefix != '\0' && k < max_size) \
        buf[k++] = *the_prefix++; \
\
    int len = std::min(max_size, the_length); \
    if (len == 0) \
        result = YY_NULL; \
    else \
    {\
        int j = 0;\
        char *bb = buf + k;\
	while (j < len) \
	    bb[j++] = *the_text++; \
        the_length -= len; \
	result = k + len; \
    }\
}
#define YY_NO_UNPUT

// Reset the scanner
void reset_scanner(void) { yyrestart(yyin); }

#else // !defined(FLEX_SCANNER)

// Input routines for SUN lex and likewise
static char pushback[BUFSIZ];
static char *pushback_ptr = pushback;

inline int do_unput(char c)
{
    if (c != 0)
	*pushback_ptr++ = c;

    return c;
}

static int do_input(void) 
{
    if (pushback_ptr != pushback)
	return *--pushback_ptr;

    if (*the_prefix != '\0')
	return *the_prefix++;

    if (the_length == 0)
	return 0;

    the_length--;
    return *the_text++;
}

#ifdef input
#undef input
#define input do_input
#endif

#ifdef unput
#undef unput
#define unput do_unput
#endif

// Reset the scanner
static void reset_scanner(void) { pushback_ptr = pushback; }

#endif // !defined(FLEX_SCANNER)


// #define YY_SKIP_YYWRAP
// extern "C" int yywrap(void)
// {
//     return 1;
// }

// Returning values
#ifdef FLEX_SCANNER

#define YY_DECL const regex *yylex ( void )
#define RETURN(x) return x

#else

// Ordinary LEX uses `int yylex()'
#define RETURN(x) return x ? 1 : 0

#endif



// Buffer size.  FLEX can match tokens of arbitrary length (all praise
// FLEX!), but classical LEX has a limit of YYLMAX characters.
// Attempt to raise this to some larger value.
#if !defined(FLEX_SCANNER) && defined(YYLMAX)
#if YYLMAX < MAX_LEX_TOKEN_SIZE
#undef YYLMAX
#define YYLMAX MAX_LEX_TOKEN_SIZE
#endif
#endif



// Anything not in the list is `not matched'
#ifdef ECHO
#undef ECHO
#endif
#define ECHO                 RETURN(0)

#ifdef FLEX_SCANNER
// #defining YY_BREAK to empty disables warnings about unreachable breaks.
#define YY_BREAK DO_NOT_BREAK
#define DO_NOT_BREAK
#define BREAK break;
#endif // FLEX_SCANNER
%}

/* Addresses -- in various formats */
A	(0x[0-9a-fA-F]+|0[0-9a-fA-F]+[hH]|H\'[0-9a-fA-F]+|00+|[(]nil[)]|NIL|null|@[0-9a-fA-F]+|16_[0-9a-f]+|instance[ ]of[ ][^(]*[(][^)]*[)])

/* Simple prefix of address */
a	[0H@]

/* Whitespace */
w	[ \f\t]

/* Optional Whitespace */
_	[ \f\t]*

/* Non-empty sequence of Whitespace */
W	[ \f\t]+

/* Digit */
d	[0-9]

/* Non-empty sequence of digits */
D	[0-9]+

/* Positive number */
P	[1-9][0-9]*

/* Hex address */
x	(0(0|x)[0-9a-f]+|[(]nil[)]|NIL|null)

/* An arbitrary character */
c	(.|\n)

/* A sequence of arbitrary characters */
C	(.|\n)*

/* `virtual table', in all variants */
V	v(irtual{W})?t(a)?bl(e)?

/* A `standard' identifier.  Also includes Perl identifiers. */
I	([A-Za-z_$@%][A-Za-z0-9_$]*|[$]([^ \n\t\f]|[0-9]+))

/* A Java class identifier */
J	[A-Za-z_$][A-Za-z0-9_$.]*

/* A non-identifier */
z       [^a-zA-Z0-9_$]

/* This lexer is quite huge.  SunOS lex and AIX lex wants some extra sizes. */
%e 5000
%k 10000
%p 15000
%a 25000
%n 5000
%o 35000

%%
01address{W}{A}				RETURN(&rxaddr);
02{A} 		 			RETURN(&rxaddress);
03{A}{w}in{w}	  			RETURN(&rxaddress_in);
04{a}	        	 		RETURN(&rxaddress_start);
05[A-Za-z]+				RETURN(&rxalpha);
06[0-9A-Za-z]+				RETURN(&rxalphanum);
07[(][^0-9][^)]*[)]			RETURN(&rxarglist);
08[)]?{d}*[1-9]-?{_}\,{_}{d}*[1-9]-?[(]?{W}ta{W}{C}	RETURN(&rxat);
09{w}?					RETURN(&rxblank);
10[ ]+		 			RETURN(&rxblanks);
11{W}					RETURN(&rxblanks_or_tabs);
12{_}([th]*(b|bre|brea|break|b[a-z])|cl|cle|clea|clear|d[a-z]|info{W}(li|lin|line)|ignore|cond|condition|when|stop)({W}{C})?	RETURN(&rxbreak_cmd);
13{_}cd({W}{C})?			RETURN(&rxcd_cmd);
14[^:]*:[^:]*:{_}{D}{C}			RETURN(&rxcolons);
15{C}:{C}core{C}		 	RETURN(&rxcore);
16{_}(attach|core(-file)?)({W}{C})?	RETURN(&rxcore_cmd);
17\r\r*\n				RETURN(&rxcrlf);
18{_}[a-z ]*display({W}{C})?		RETURN(&rxdata);
19[^ \t\n)}][^=\n]*{w}={w}		RETURN(&rxdbx_begin_of_display);
20[(]{P}[)]{w}				RETURN(&rxdbx_begin_of_display_info);
21[a-zA-Z_$0-9]*`			RETURN(&rxdbx_scope);
22[a-zA-Z_$][a-zA-Z_$0-9]*[(]{C}[)]{C}([[]{C}[]]|\,{w}line{w}{C})	RETURN(&rxdbxframe);
23[a-zA-Z_$][^[]*:{_}{P}{W}{C}		RETURN(&rxdbxfunc);
24{C}line{W}{P}{W}in{W}(file{W})?\"[^\"]*\"\n{C}	RETURN(&rxdbxfunc2);
25[[][^\133]*:{P}[^\133]*[]]{C}		RETURN(&rxdbxpos);
26dbx:{W}warning:.*option{W}only{W}recognized{W}for.*\n	RETURN(&rxdbxwarn1);
27dbx:{W}warning:.*unknown{W}language.*\n	RETURN(&rxdbxwarn2);
28{_}(debug|givenfile)({W}{C})?		RETURN(&rxdebug_cmd);
29{W}no{W}tnedneped{W}			RETURN(&rxdep);
30{_}disable{W}display({W}{C})?		RETURN(&rxdisable);
31{_}(disp(lay)?|plot)({W}{C})?		RETURN(&rxdisplay);
32{_}(disp(lay)?|plot){W}		RETURN(&rxdisplay_cmd);
33{_}(disp(lay)?|plot){W}{C}		RETURN(&rxdisplay_cmd_and_args);
34Do{_}n['o]t				RETURN(&rxdont);
35\/[^/]*[^/.]\/[.][.]\/		RETURN(&rxdotdot);
36-?(({D}\.{d}*)|({D})|(\.{D}))([eE][+-]?{D})?	RETURN(&rxdouble);
37{_}do(wn)?({W}{C})?			RETURN(&rxdown_cmd);
38{_}enable{W}display({W}{C})?	RETURN(&rxenable);
39[^{};,\n= ]+{W}={W}[^{}();,\n= ]+{W}={W}{C} RETURN(&rxeqeq);
40{_}(file|load)({W}{C})?		RETURN(&rxfile_cmd);
41[^ "'`/]*\/				RETURN(&rxfilepath);
42{_}use{W}{C}				RETURN(&rxuse_cmd);
43#{D}{W}{A}				RETURN(&rxframe_addr);
44{_}(up|do|down|f|fra|fram|frame|suspend|resume|top|V)({W}{C})? RETURN(&rxframe_cmd);
45[a-zA-Z0-9_$][(]			RETURN(&rxfunction_call);
46{P}:{W}[^ ]				RETURN(&rxgdb_begin_of_display);
47{P}:{w}{w}{w}				RETURN(&rxgdb_begin_of_display_info);
48{P}		 			RETURN(&rxgdb_disp_nr);
49{C}((class|interface)[(]{J}[)]|[(]{J}:{P}[)]) RETURN(&rxjdbpos);
50{_}graph{W}{C}			RETURN(&rxgraph_cmd);
51{I}					RETURN(&rxidentifier);
52[[]-?{D}]{C}				RETURN(&rxindex);
53-?{D}		 			RETURN(&rxint);
54([a-zA-Z]+{W}[a-zA-Z]+{C}|{C}[a-zA-Z]+{W}[a-zA-Z]+(\.|>)?)\n?  RETURN(&rxinvalid_value);
55{_}(file|func|v){W}{C}	 	RETURN(&rxlookup_cmd);
56[a-z]+		 		RETURN(&rxlowercase);
57\(\*{C}\*\){C}			RETURN(&rxm3comment);
58{_}(sh{W}|!)?make({W}{C})?		RETURN(&rxmake_cmd);
59members{w}of{w}.+:{w}?\n		RETURN(&rxmembers_of_nl);
60-?{D}\.\.-?{D}			RETURN(&rxmore_than_one);
61[^ ]+:{D}\n				RETURN(&rxname_colon_int_nl);
62{W}\n{W}				RETURN(&rxnl);
63\n[1-9]		 		RETURN(&rxnl_int);
64\n{_}{A}	 			RETURN(&rxnladdress);
65\n{_}{A}{w}in{w} 			RETURN(&rxnladdress_in);
66\n{c}[*]		 		RETURN(&rxnlstar);
67no[nt][ -]?(0|zero|null)		RETURN(&rxnonzero1);
68!={_}(0|zero|null)			RETURN(&rxnonzero2);
69{_}(echo|help|show|info|where|shell|sh|x)({W}{C})?	RETURN(&rxnop_cmd);
70@{D}@					RETURN(&rxnum);
71{_}(info{W}line)({W}{C})?		RETURN(&rxop_cmd);
72-[bcdeiIopPrRsx]			RETURN(&rxoptions);
73Line{W}number{W}{D}{W}is{W}out{W}of{W}range{W}for{W} RETURN(&rxout_of_range);
74{_}(dir|directory|path)({W}{C})?	RETURN(&rxpath_cmd);
75\$pc{_}={_}{A}			RETURN(&rxpc);
76(>|[a-zA-Z][-a-zA-Z0-9 ]*[-a-zA-Z0-9][[][1-9][0-9]*[]])[ ] RETURN(&rxjdbprompt);
77[ ](>|[]][0-9]*[1-9][[][-a-zA-Z0-9][-a-zA-Z0-9 ]*[a-zA-Z]) RETURN(&rxjdbprompt_reverse);
78Process{W}{D}:{_}			RETURN(&rxprocess1);
79Process{W}{D}:			RETURN(&rxprocess2);
80[(][^ )]*de?b[^)]*[)]{w}		RETURN(&rxprompt);
81{C}[(]END[)].*			RETURN(&rxq);
82@{_}{x}{_}:{C} 			RETURN(&rxreference);
83{_}refresh({W}{C})?			RETURN(&rxrefresh_cmd);
84{C}([(]press{W}RETURN[)]|Hit{W}RETURN{W}to{W}continue|Type{W}<return>{W}to{W}continue|More{W}[(]n{W}if{W}no[)][?]|{W}cont:{W}).* RETURN(&rxreturn);
85{_}(r|rer|rerun|ru|run|R|exec)({W}{C})? RETURN(&rxrun_cmd);
86{_}(r|ru|run|rer|rerun|c|cont|contin|continu|continue|sig|sign|signa|signal|u|unt|unti|until|s|si|step|stepi|n|ni|next|nexti|j|ju|jump|k|ki|kill|fin|fini|finis|finish|R|S|exec|rc|reverse-continue|reverse-step|reverse-stepi|reverse-next|reverse-nexti|python)({W}{C})?	        RETURN(&rxrunning_cmd);
87Select{w}one{w}of{w}\[{D}{w}-{w}{D}\]:{w}	RETURN(&rxselect);
88;{_}[}]				RETURN(&rxsemicolon_and_brace);
89[^-_a-zA-Z0-9]			RETURN(&rxsep);
90{_}(set{W}var[a-z]*|assign|pq)({W}{C})?	RETURN(&rxset1_cmd);
91{_}(set|p|print|output){W}[^=]+=[^=]{C}	RETURN(&rxset2_cmd);
92{_}set{W}args({W}{C})?		RETURN(&rxset_args_cmd);
93{_}(set|dbxenv|O){W}{C} 		RETURN(&rxsetting_cmd);
94([][a-zA-Z0-9_$@%().`]|->|::)*	RETURN(&rxsimple);
95{_}disp(lay){W}[^ ]+			RETURN(&rxsingle_display_cmd);
96{C}(--More--|line{w}{d}).* 		RETURN(&rxspace);
97Breakpoint{W}{P}\,{W}{A}		RETURN(&rxstopped_addr);
98{x}?{_}([(]|[{]|record\n|RECORD\n|RECORD{w}|OBJECT{w}|struct{z}|class{z}|interface{z}|union{z}){C}	RETURN(&rxstruct_begin);
99(>|[[][1-9][0-9]*[]])[ ]          	RETURN(&rxjdbprompt_nothread);
A0([)]|[}]|end\n|END\n|end;|END;)	RETURN(&rxstruct_end);
A1{x}?{_}(record\n|RECORD\n|RECORD{w}|OBJECT{w}){C} RETURN(&rxstruct_keyword_begin);
A2([Tt]he{W})?[Pp]rogram{W}(exited|terminated|is{W}not{W}being{W}run|no{W}longer{W}exists){C} RETURN(&rxterminated);
A3{_}(thread|threadgroup|suspend|resume)({W}{C})? RETURN(&rxthread_cmd);
A4{_}(delete{W}|un)display({W}{C})?	RETURN(&rxundisplay);
A5{_}up({W}{C})?			RETURN(&rxup_cmd);
A6[A-Z]+				RETURN(&rxuppercase);
A7{A}{W}<[^ \n>]*{W}{V}>[^{},]*[{]{C} 	RETURN(&rxvtable);
A8{D}{W}{V}{W}entries\,	        	RETURN(&rxvtable_entries);
A9[ \n\t\r\v\f]+		 	RETURN(&rxwhite);
B0{C}Standard{W}input:{W}END{C} 	RETURN(&rxxdb);
B1[^ \t]*:.*:{w}{P}[: ]{C} 	        RETURN(&rxxdbpos);
B2[<]repeats{W}{P}{W}times[>]       	RETURN(&rxrepeats);
B3(::|`)?{I}(([.]|->|::|`){I})*		RETURN(&rxchain);
B4{W}ni{W}nehw{W}(ro{W}won{W})?     	RETURN(&rxwhen);
B5Breakpoint{W}{P}\,{W}			RETURN(&rxstopped_func);
B6#{D}{W}{I}[^ \f\t\n]*{W}[(]		RETURN(&rxframe_func);
B7{_}(define|document){W}{C}		RETURN(&rxdefine_cmd);
B8{_}(l|li|lis|list)({W}{C})?	 	RETURN(&rxlist_cmd);
B9{D}{_}\,{_}{D}	 		RETURN(&rxlist_range);
C0[^:]*:{P}	 			RETURN(&rxfilepos);
C1{_}{I}[^({\n=]*:[(]			RETURN(&rxdbx_baseclass);
C2{_}[^=]+=[^=]{C}			RETURN(&rxset3_cmd);
C3({w}|\001|\002)*DB<+[0-9]*>+({w}|\001|\002)* RETURN(&rxperlprompt);
C4[^(]*::[^(]*[(][^:]*:{P}[)]:.*	RETURN(&rxperlpos);
C5(({I}::)*{I}=)?{I}[(]{A}[)]		RETURN(&rxperlref);
C6enter{W}.*file{W}name                 RETURN(&rxenter_file_name);
C7[[]-?{D}]{W}*={W}[[]-?{D}]{C}         RETURN(&rxdoubleindex);
C8{_}(r|ru|run|rer|rerun|c|cont|contin|continu|continue|R|exec)({W}{C})? RETURN(&rxcont_cmd);
C9bashdb<+[(]*[0-9]*[)]*>+{w}		RETURN(&rxbashprompt);
D0[(][^:]*:{P}[)]:.*	                RETURN(&rxbashpos);
D1[^ \t]*:{_}{P}{_}<.*>       		RETURN(&rxdbgpos);
D2#{D}{_}<.*>{_}at{_}[^ \t]*:{_}{P}	RETURN(&rxdbgframepos);
D3remake<+[(]*[.0-9]*[)]*>+[.]*{w}		RETURN(&rxmakeprompt);
D4(<-|->|\+\+|--)?{_}[(][^:]*:{P}[)].*	                RETURN(&rxmakepos);
D5[(]+Pydb[)]+{w}		        RETURN(&rxpyprompt);
D6[(][^:]*:{P}[)]:.*	                RETURN(&rxpypos);
D7{D}(\.{D})?	                        RETURN(&rxfixednum);
D8".*"	                                RETURN(&rxstring);
D9{D}\.{D}                              RETURN(&rxint_dot_int);
(.|\n)				        RETURN(0);  // Anything else
%%
