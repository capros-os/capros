/* -*- Mode: c -*- */
/*
 * Copyright (C) 1998, 1999, Jonathan S. Shapiro.
 *
 * This file is part of the EROS Operating System.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2,
 * or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */


/* The purpose of this program is to strip out extraneous white space
   from HTML files.  Stripping white space simultaneously reduces the
   file size and works around bugs in the netscape parser that break
   style sheets.

   Rules applied by this file:

   1. Newline => white space
   2. Multiple white space collapses into single white space.
   3. White space after a leading element is suppressed.
   4. White space before a trailing element is suppressed.
   NOTE CLOSELY:
   5. White space before and after ANY non-inline element is suppressed.
   */

%{
  /* #define YY_NO_UNPUT */

#if 0
extern int current_line;
extern InternedString current_file;
void SetFile(char *name, int len);
void SetLine(int line);
#endif
#ifndef output
#define output(x) fputc(x, yyout);
#endif

#undef yyunput
%}

%x WHITE BEGIN_ELEMENT END_ELEMENT  END_INLINE_ELEMENT

INLINE  (([Ee][Mm])|[Aa]|[Bb]|[Ii]|[Uu]|([Ss][Tt][Rr][Oo][Nn][Gg])|([Cc][Oo][Dd][Ee])|([Tt][Tt]))
PRE     \<[Pp][Rr][Ee]
ENDPRE  \/[Pp][Rr][Ee][^>]*\>
OCOMMENT \<\!\-\-.*\-\-\>
COMMENT \<\!--(\n|[^-]|(-[^-])|(--[^>]))*--\>

%%
{PRE}(.|\n)*{ENDPRE} {ECHO;}
{COMMENT} { /* do nothing */;}
\<\/{INLINE}/[ \t\n>]  { ECHO; BEGIN(END_INLINE_ELEMENT); }
\<\/	{ ECHO; BEGIN(END_ELEMENT);}
\<	{ ECHO; BEGIN(BEGIN_ELEMENT);}
\n	{ /* current_line++; */ BEGIN(WHITE); }
[ \t]	{ BEGIN(WHITE);}
.	{ ECHO; }

<WHITE>\<\/{INLINE}/[ \t\n>] { ECHO; BEGIN(END_INLINE_ELEMENT); }
<WHITE>\<\/         { ECHO; BEGIN(END_ELEMENT); }
<WHITE>\<{INLINE}/[ \t\n>]   { output(' '); ECHO; BEGIN(BEGIN_ELEMENT); }
<WHITE>\<           { ECHO; BEGIN(BEGIN_ELEMENT); }
<WHITE>[ \t\n]*     { /* do nothing */}
<WHITE>.            { output(' '); ECHO; BEGIN(0);}
<WHITE>{PRE}        { output('\n'); ECHO; BEGIN(0); }
<WHITE>{COMMENT}    { ; }

<END_ELEMENT>\"[^"]*\" { ECHO; }
<END_ELEMENT>[ \t\n]+  { output(' '); }
<END_ELEMENT>\>[ \t\n]* { /* discard trailing space */ output('>'); BEGIN(0);}
<END_ELEMENT>.          { ECHO; }

<END_INLINE_ELEMENT>\"[^"]*\" { ECHO; }
<END_INLINE_ELEMENT>[ \t\n]+  { output(' '); }
<END_INLINE_ELEMENT>\>        { /* preserve trailing space */ output('>'); BEGIN(0);}
<END_INLINE_ELEMENT>.          { ECHO; }

<BEGIN_ELEMENT>\"[^"]*\" { ECHO; }
<BEGIN_ELEMENT>[ \t\n]+  { output(' '); }
<BEGIN_ELEMENT>\>[ \t\n]* { /* discard trailing space */ output('>'); BEGIN(0);}
<BEGIN_ELEMENT>.          { ECHO; }
%%
