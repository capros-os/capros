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

#include <assert.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>

/* GNU multiple precision library: */
#include <gmp.h>

#include <applib/xmalloc.h>
#include <applib/Intern.h>
#include <applib/Diag.h>
#include <applib/PtrVec.h>
#include "SymTab.h"
#include "ParseType.h"
#include "Lexer.h"
#include "util.h"

extern const char *target;
extern const char *execDir;
extern const char *archiveName;

static void
symdump(Symbol *s, FILE *out, int indent)
{
  do_indent(out, indent);

  switch(s->cls){
  case sc_absinterface:
    // enabling stubs for abstract interface.  This can be double checked later
  case sc_interface:
    {
      unsigned i;

      for(i = 0; i < vec_len(s->children); i++)
	symdump(symvec_fetch(s->children,i), out, indent);

      break;
    }
  case sc_package:
    {
      unsigned i;

      for(i = 0; i < vec_len(s->children); i++)
	symdump(symvec_fetch(s->children,i), out, indent);

      break;
    }
  case sc_operation:
    {
      extern InternedString lookup_containing_file(Symbol *s);
      InternedString uocFileName = lookup_containing_file(s);

      {
	const char *sqn = symbol_QualifiedName(s, '_');
	fprintf(out, "%s/%s.c: %s\n", target, sqn, uocFileName);
#if 0
	fprintf(out, "%s/%s.o: %s/%s.c\n", execDir, sqn, target, sqn);
#endif
	fprintf(out, "%s/%s(%s/%s.o): %s/%s.c\n", execDir, archiveName, execDir, sqn, target, sqn);

	fprintf(out, "%s/%s: %s/%s(%s/%s.o)\n", execDir, archiveName, execDir, archiveName, execDir, sqn);
      }

      break;
    }
  case sc_enum:
  case sc_struct:
  case sc_const:
  case sc_exception:
  case sc_typedef:

      break;

  default:
    {
      fprintf(out, "UNKNOWN/BAD SYMBOL CLASS %s FOR: %s\n", 
	      symbol_ClassName(s), s->name);
      break;
    }
  }
}

void
output_c_stub_depend(Symbol *s)
{
  if (archiveName == 0)
    diag_fatal(1, "Stub dependency generation requires an archive name\n");
  if (execDir == 0)
    diag_fatal(1, "Stub dependency generation requires an executable directory\n");

  if (s->isActiveUOC == false)
    return;

  symdump(s, stdout, 0);
}
