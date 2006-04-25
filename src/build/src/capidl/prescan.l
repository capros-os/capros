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

/* CPP_LINE lexemes are retained for backwards compatibility
   support. */

%{

/* GNU multiple precision library: */
#include <sys/types.h>
#include <gmp.h>
#include <string.h>
#include <stdbool.h>
#include <stdio.h>

#include <applib/xmalloc.h>
#include <applib/Intern.h>
#include <applib/App.h>

#include "SymTab.h"
#include "ParseType.h"
#define YYSTYPE ParseType

#include "Lexer.h"

#include "idl.h"
#define YY_NO_UNPUT

#undef YY_DECL
#define YY_DECL int yylex(YYSTYPE *lvalp, PrescanLexer *lexer)
%}

%option noyywrap
%option prefix="prescan_"

/* Following tokens exist purely so we can track correct file/lineno
 * for error reporting. They are not reported to the parser.
 */
%x COMMENT TOPDEF TOSEMI

IDENT   [_a-zA-Z][_A-Za-z0-9]*
NAME    [_a-zA-Z][_A-Za-z0-9]*(\.[_a-zA-Z][_A-Za-z0-9]*)*

/* I have left out support for top-level const, typedef, because these 
 * truly require a full parse. */
%%
\n                { lexer->current_line++;       }
[ \t]+            { /* ignore */          }
package           {  /* ignore */         }
{NAME}            {  lexer->pkgName = intern(yytext);
                     BEGIN(TOPDEF);       }
<TOPDEF>;         {  /* skip */;          }
<TOPDEF>[ \t]+    {  /* skip */;          }
<TOPDEF>\n        {  lexer->current_line++;      }
<TOPDEF>abstract  {  /* skip */;          }
<TOPDEF>interface {  /* skip */;          }
<TOPDEF>struct    {  /* skip */;          }
<TOPDEF>union     {  /* skip */;          }
<TOPDEF>exception {  /* skip */;          }
<TOPDEF>enum      {  /* skip */;          }
<TOPDEF>namespace {  /* skip */;          }

<TOPDEF>{IDENT}     {  
                       InternedString topName = 
			 intern_concat(lexer->pkgName,
				       intern_concat(".", yytext));
		       ptrvec_append(lexer->map, topsym_create(topName, lexer->fileName, lexer->isCmdLine));
// diag_printf("Found %s %s %s\n", lexer->pkgName, yytext, lexer->fileName);

                       BEGIN(TOSEMI);
                    }

<TOPDEF>\/\/[^\n]*  {  /* ignore */         }

<TOPDEF>\/\*        { lexer->commentCaller = YYSTATE;
                      BEGIN(COMMENT);       }

<TOSEMI>\{         {  lexer->curlyDepth++;        }

<TOSEMI>\}         {  lexer->curlyDepth--;        }

<TOSEMI>\/\/[^\n]*  {  /* ignore */         }

<TOSEMI>\/\*        { lexer->commentCaller = YYSTATE;
                      BEGIN(COMMENT);       }

<TOSEMI>;           { if (lexer->curlyDepth == 0)
                      BEGIN(TOPDEF);        }

<TOSEMI>.           { /* ignore */          }
<TOSEMI>\n          { /* ignore */          }

\/\/[^\n]*          {  /* ignore */         }

\/\*                { lexer->commentCaller = YYSTATE;
                      BEGIN(COMMENT);       }

.                   { /* all else has failed */
		      diag_fatal(1, "Illegal input character '%c' at line %d\n",
				     yytext[0], lexer->current_line);
	            }

<COMMENT>\*\/        { BEGIN(lexer->commentCaller); }
<COMMENT>[^\n]       { /* ignore */           }
<COMMENT>\n          { lexer->current_line++;        }
%%

PrescanLexer *
prescanlexer_create(const char *inputFileName, 
		    PtrVec *uocMap, bool isCmdLine, FILE *fin)
{
  PrescanLexer *lx = MALLOC(PrescanLexer);
  lx->current_line = 1;
  lx->curlyDepth = 0;

  lx->map = uocMap;

  lx->pkgName = 0;
  lx->fileName = intern(inputFileName);
  lx->isCmdLine = isCmdLine;

  yyrestart(fin);
  yy_init = 1;		/* whether we need to initialize */
  yy_start = 0;	/* start state number */
  yy_flush_buffer(YY_CURRENT_BUFFER);

  prescan__flex_debug = false;
  prescan_in = fin;

  return lx;
}

TopsymMap *
topsym_create(InternedString s, InternedString f, bool isCmdLine)
{
  TopsymMap *ts = MALLOC(TopsymMap);
  ts->symName = s;
  ts->fileName = f;
  ts->isCmdLine = isCmdLine;
  ts->isUOC = true;

  return ts;
}

void 
prescanlexer_setDebug(bool showParse)
{
  prescan__flex_debug = showParse;
}
