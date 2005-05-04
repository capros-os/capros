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


/* -*- Mode: c -*- */
/* Scanner for new and improved mkimage program */

%{
#include <sys/types.h>

#include <eros/target.h>
#include <erosimg/Intern.h>

#include <eros/target.h>
#include <erosimg/App.h>
#include <erosimg/ErosImage.h>
#include <erosimg/DiskKey.h>

#include "ParseType.h"
#define YYSTYPE ParseType

#include "gram.c.h"
#define YY_NO_UNPUT

extern int current_line;
extern const char *current_file;
void SetFile(char *name, int len);
void SetLine(int line);
bool GetMiscKeyType(const char *s, uint32_t *ty);

%}

%x CPP_FILE CPP_LINE CPP_EOL

NAME    [_a-zA-Z][_A-Za-z0-9\.]*
STRINGLIT   \"([^"]|\\\")+\"

DEC     [1-9][0-9]*[Uu]?
HEX     0[xX][0-9a-fA-F]+[Uu]?
OCT     0[0-7]*[Uu]?
BIN     0[Bb][0-1]+[Uu]?

%%
\n	{ current_line++; }
"|="	{ return OREQ; }
"="	{ return '='; }
"("	{ return '('; }
")"	{ return ')'; }
"["	{ return '['; }
"]"	{ return ']'; }
","	{ return ','; }
":"	{ return ':'; }
";"	{ return ';'; }
"+"	{ return '+'; }
"-"	{ return '-'; }
"*"	{ return '*'; }
"%"	{ return '*'; }
"/"	{ return '/'; }
"^"	{ return '^'; }
"~"	{ return '~'; }
"|"	{ return '|'; }
"&"	{ return '&'; }
ro	{ return RO; }
nc	{ return NC; }
weak	{ return WEAK; }
constituents	{ return CONSTITUENTS; }
sense	{ return SENSE; }
prime	{ return PRIME; }
physmem	{ return PHYSMEM; }
node	{ return NODE; }
chain	{ return CHAIN; }
append	{ return APPEND; }
domain	{ return DOMAIN; }
process	{ return PROCESS; }
new	{ return NEW; }
as	{ return AS; }
hide	{ return HIDE; }
program	{ return PROGRAM; }
small	{ return SMALL; }
null	{ return NULLKEY; }
void	{ return VOIDKEY; }
zero	{ return ZERO; }
empty	{ return EMPTY; }
with	{ return WITH; }
pages	{ return PAGES; }
segment	{ return SEGMENT; }
directory	{ return DIRECTORY; }
capability	{ return CAPABILITY; }
segtree	{ return SEGTREE; }
blss	{ return KW_BLSS; }
lss	{ return KW_LSS; }
arch	{ return ARCH; }
print	{ return PRINT; }
red	{ return RED; }
wrapper	{ return WRAPPER; }
reg	{ return REG; }
space	{ return SPACE; }
symtab	{ return SYMTAB; }
iospace	{ return IOSPACE; }
keeper	{ return KEEPER; }
start	{ return START; }
misc	{ return MISC; }
clone	{ return CLONE; }
copy	{ return COPY; }
number	{ return NUMBER; }
background|bg	{ return BACKGROUND; }
priority	{ return PRIORITY; }
schedule	{ return SCHEDULE; }
brand	{ return BRAND; }
symbol	{ return SYMBOL; }
range	{ return RANGE; }
slot	{ return SLOT; }
genreg	{ return GENREG; }
root	{ return ROOT; }
key	{ return KEY; }
subseg	{ return SUBSEG; }
at	{ return AT; }
word	{ return WORD; }
pc	{ return PC; }
sp	{ return SP; }
all	{ return ALL; }
slots	{ return SLOTS; }
keys	{ return KEYS; }
string	{ return STRING; }
limit	{ return LIMIT; }
run	{ return RUN; }
ipl	{ return IPL; }
page	{ return PAGE; }
sched	{ return SCHED; }
{DEC}	{
          const char *is = internWithLength(yytext, yyleng);
          yylval.is = is;
          return DEC;
        }
{OCT}	{
          const char *is = internWithLength(yytext, yyleng);
          yylval.is = is;
          return OCT;
        }
{HEX}	{
          const char *is = internWithLength(yytext, yyleng);
          yylval.is = is;
          return HEX;
        }
{BIN}	{
          const char *is = internWithLength(yytext, yyleng);
          yylval.is = is;
          return BIN;
        }
{NAME}	{
	  uint32_t miscType;

          const char *is = internWithLength(yytext, yyleng);
          yylval.is = is;

	  return GetMiscKeyType(is, &miscType) ? MISC_KEYNAME : NAME;
        }
{STRINGLIT}  {
          const char *is = internWithLength(yytext+1, yyleng-2);
          yylval.is = is;
          return STRINGLIT;
        }
[ \t]+	{ /* ignore */ };

^#      BEGIN(CPP_LINE);

.       { /* all else has failed */
          fflush(stdout);
          diag_fatal(1, "Illegal input character '%c' at line %d\n",
                          yytext[0], current_line);
        }

<CPP_LINE>[ \t]+    { };
<CPP_LINE>[0-9]+    { SetLine(strtoul(yytext, 0, 0));
                      BEGIN(CPP_FILE); };
<CPP_FILE>[ \t]+    { };
<CPP_FILE>\"[^"]+\" { SetFile(yytext+1, yyleng-2);
                        BEGIN(CPP_EOL); }
<CPP_EOL>[ \t0-9]*\n   { BEGIN(INITIAL); }
%%
void
SetFile(char *name, int len)
{
  current_file = internWithLength(name, len);
}

void
SetLine(int line)
{
  current_line = line;
}

