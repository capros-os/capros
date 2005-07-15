/* -*- Mode: c -*- */
/*
 * Copyright (C) 2002, The EROS Group, LLC.
 *
 * This file is part of the EROS Operating System runtime library.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, 59 Temple Place - Suite 330 Boston, MA 02111-1307, USA.
 */

/* Scanner for new and improved mkimage program */

%{

/* GNU multiple precision library: */
#include <sys/types.h>
#include <gmp.h>
#include <string.h>
#include <stdbool.h>

#include <applib/xmalloc.h>
#include <applib/Intern.h>
#include <applib/App.h>

#include "SymTab.h"
#include "ParseType.h"
#define YYSTYPE ParseType

#include "Lexer.h"

#include "idl.h"
#define YY_NO_UNPUT

#define TOK(x)  do { InternedString is = internWithLength(yytext, yyleng); \
                  lvalp->tok.file = lexer->current_file;       \
                  lvalp->tok.line = lexer->current_line;       \
                  lvalp->tok.is = is;       \
                  return x; } while (0)

#undef YY_DECL
#define YY_DECL int yylex(YYSTYPE *lvalp, MyLexer *lexer)
%}

/* Following tokens exist purely so we can track correct file/lineno
 * for error reporting. They are not reported to the parser.
 */
%option noyywrap
%option prefix="mylexer_"
%x COMMENT DOC DOCSOL

/*
FILENAME    \"([^"]|\\\")+\"
*/
NAME    [_a-zA-Z][_A-Za-z0-9]*
RESERVED ANY|ATTRIBUTE|CONTEXT|FIXED|IN|INOUT|NATIVE|READONLY|SEQUENCE|TRUNCATABLE|VALUETYPE

%%
\n	       {  lexer->current_line++;      }
"="	       {  return '=';          }
"+"	       {  return '+';          }
"-"	       {  return '-';          }
"*"	       {  return '*';          }
"/"	       {  return '/';          }
";"	       {  return ';';          }
"."	       {  return '.';          }
"["	       {  return '[';          }
"]"	       {  return ']';          }

::             {  TOK(OPSCOPE);        }
:              {  return ':';          }
any            {  TOK(ANY);            }
array          {  TOK(ARRAY);          }
attribute      {  TOK(ATTRIBUTE);      }
boolean        {  TOK(BOOLEAN);        }
buffer         {  TOK(BUFFER);         }
byte           {  TOK(BYTE);           }
case           {  TOK(CASE);           }
char           {  TOK(CHAR);           }
const	       {  TOK(CONST);          }
context        {  TOK(CONTEXT);        }
default        {  TOK(DEFAULT);        }
double         {  TOK(DOUBLE);         }
enum           {  TOK(ENUM);           }
exception      {  TOK(EXCEPTION);      }
false          {  TOK(tFALSE);         }
fixed          {  TOK(FIXED);          }
float          {  TOK(FLOAT);          }
in             {  TOK(IN);             }
inout          {  TOK(INOUT);          }
integer	       {  TOK(INTEGER);        }
interface      {  TOK(INTERFACE);      }
long           {  TOK(LONG);           }
max            {  TOK(tMAX);           }
min            {  TOK(tMIN);           }
module         {  TOK(MODULE);         }
native         {  TOK(NATIVE);         }
object         {  TOK(OBJECT);         }
oneway         {  TOK(ONEWAY);         }
out            {  TOK(OUT);            }
raises         {  TOK(RAISES);         }
readonly       {  TOK(READONLY);       }
repr           {  TOK(REPR);           }
resume         {  TOK(RESUME);         }
sequence       {  TOK(SEQUENCE);       }
short          {  TOK(SHORT);          }
string         {  TOK(STRING);         }
struct         {  TOK(STRUCT);         }
switch         {  TOK(SWITCH);         }
true           {  TOK(tTRUE);          }
truncatable    {  TOK(TRUNCATABLE);    }
typedef        {  TOK(TYPEDEF);        }
union          {  TOK(UNION);          }
unsigned       {  TOK(UNSIGNED);       }
valuetype      {  TOK(VALUETYPE);      }
void           {  TOK(VOID);           }
wchar          {  TOK(WCHAR);          }
wstring        {  TOK(WSTRING);        }


abstract       {  TOK(ABSTRACT);       }
an             {  TOK(AN);             }
as             {  TOK(AS);             }
begin          {  TOK(tBEGIN);         }
behalf         {  TOK(BEHALF);         }
bind           {  TOK(BIND);           }
catch          {  TOK(CATCH);          }
class          {  TOK(CLASS);          }
nostub         {  TOK(NOSTUB);         }
client         {  TOK(CLIENT);         }
constructor    {  TOK(CONSTRUCTOR);    }
else           {  TOK(ELSE);           }
ensure         {  TOK(ENSURE);         }
eventual       {  TOK(EVENTUAL);       }
escape         {  TOK(ESCAPE);         }
eventually     {  TOK(EVENTUALLY);     }
export         {  TOK(EXPORT);         }
extends        {  TOK(EXTENDS);        }
facet          {  TOK(FACET);          }
finally        {  TOK(FINALLY);        }
for            {  TOK(FOR);            }
forall         {  TOK(FORALL);         }
function       {  TOK(FUNCTION);       }
implements     {  TOK(IMPLEMENTS);     }
is             {  TOK(IS);             }
lambda         {  TOK(LAMBDA);         }
let            {  TOK(LET);            }
loop           {  TOK(LOOP);           }
match          {  TOK(MATCH);          }
meta           {  TOK(META);           }
method         {  TOK(METHOD);         }
methods        {  TOK(METHODS);        }
namespace      {  TOK(NAMESPACE);      }
on             {  TOK(ON);             }
package        {  TOK(PACKAGE);        }
private        {  TOK(PRIVATE);        }
protected      {  TOK(PROTECTED);      }
public         {  TOK(PUBLIC);         }
reliance       {  TOK(RELIANCE);       }
reliant        {  TOK(RELIANT);        }
relies         {  TOK(RELIES);         }
rely           {  TOK(RELY);           }
reveal         {  TOK(REVEAL);         }
sake           {  TOK(SAKE);           }
signed         {  TOK(SIGNED);         }
static         {  TOK(STATIC);         }
supports       {  TOK(SUPPORTS);       }
suspect        {  TOK(SUSPECT);        }
suspects       {  TOK(SUSPECTS);       }
synchronize    {  TOK(SYNCHRONIZED);   }
this           {  TOK(THIS);           }
throws         {  TOK(THROWS);         }
to             {  TOK(TO);             }
transient      {  TOK(TRANSIENT);      }
try            {  TOK(TRY);            }
uses           {  TOK(USES);           }
using          {  TOK(USING);          }
utf8           {  TOK(UTF8);           }
utf16          {  TOK(UTF16);          }
virtual        {  TOK(VIRTUAL);        }
volatile       {  TOK(VOLATILE);       }
when           {  TOK(WHEN);           }
while          {  TOK(WHILE);          }

\<             {  TOK('<');            }
\>             {  TOK('>');            }
\(             {  TOK('(');            }
\)             {  TOK(')');            }
,              {  TOK(',');            }
\{             {  TOK('{');            }
\}             {  TOK('}');            }

[0-9]+         {  TOK(IntegerLiteral); }
{NAME}	       {  TOK(Identifier);     }

\/\/[^\n]*     {  /* ignore */         }
[ \t]+	       { /* ignore */ };

\/\*\*         { lexer->current_doc_comment = buffer_create();
                  buffer_appendString(lexer->current_doc_comment, intern(""));
		  BEGIN(DOCSOL); }
\/\*           { BEGIN(COMMENT);       }

.              { /* all else has failed */
		 diag_fatal(1, "Illegal input character '%c' at line %d\n",
				yytext[0], lexer->current_line);
	       }

<COMMENT>\*\/        { BEGIN(INITIAL);       }
<COMMENT>[^\n]       { /* ignore */          }
<COMMENT>\n          { lexer->current_line++;       }

<DOCSOL>[ \t\n]*\*\/ { /* Ignore white space at start of line, and a
			  leading '*' if present. */
                       BEGIN(INITIAL); }
<DOCSOL>[ \t\n]*\*?  { /* Ignore white space at start of line, and a
			  leading '*' if present. */
                       BEGIN(DOC); }
<DOC>\*\/            { BEGIN(INITIAL);       }
<DOC>[^\n]           { buffer_appendString(lexer->current_doc_comment, yytext); 
                    /* ignore */          }
<DOC>\n              { buffer_appendString(lexer->current_doc_comment, yytext); 
                       lexer->current_line++;
                       BEGIN(DOCSOL);
		     }

%%
InternedString
mylexer_grab_doc_comment(MyLexer *lexer)
{
  if (lexer->current_doc_comment) {
    char *s;
    const char *is;
    s = buffer_asString(lexer->current_doc_comment);
    buffer_destroy(lexer->current_doc_comment);
    lexer->current_doc_comment = 0;
    is = intern(s);
    free(s);
    return is;
  }
  else {
    return 0;
  }
}

void
mylexer_ReportParseError(MyLexer *lexer, const char * msg)
{
  diag_printf("%s:%d: syntax error (via yyerror)\n",
	       lexer->current_file, lexer->current_line);
  lexer->num_errors++;
}

MyLexer *
mylexer_create(FILE *fin, bool isActiveUOC)
{
  MyLexer *lx = MALLOC(MyLexer);

  lx->current_file = 0;
  lx->current_basename = 0;
  lx->current_line = 1;
  lx->num_errors = 0;
  lx->curlyDepth = 0;
  lx->current_doc_comment = 0;
  lx->isActiveUOC = isActiveUOC;

  yyrestart(fin);
  yy_init = 1;		/* whether we need to initialize */
  yy_start = 0;	/* start state number */
  yy_flush_buffer(YY_CURRENT_BUFFER);

  mylexer_in = fin;
  mylexer__flex_debug = false;

  return lx;
}

void 
mylexer_setDebug(bool showParse)
{
  mylexer__flex_debug = showParse;
}
