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

static void
symdump(Symbol *s, FILE *out, int indent)
{
  do_indent(out, indent);

  switch(s->cls){
  case sc_absinterface:
    // enabling stubs for abstract interface.  This can be double checked later
  case sc_interface:
    {
      int i;
      extern const char *target;
      extern InternedString lookup_containing_file(Symbol *s);

      InternedString fileName;
      InternedString uocFileName = lookup_containing_file(s);

      {
	const char *sqn = symbol_QualifiedName(s, '/');
	char *tmp = VMALLOC(char, 
			    strlen(target) + strlen("/") + strlen(sqn) 
			    + strlen(".h") + 1);
    
	strcpy(tmp, target);
	strcat(tmp, "/");
	strcat(tmp, sqn);
	strcat(tmp, ".h");
	fileName = intern(tmp);

	free(tmp);
      }

      fprintf(stdout, "%s: %s\n", fileName, uocFileName);

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
output_c_hdr_depend(Symbol *s)
{
  if (s->isActiveUOC == false)
    return;

  symdump(s, stdout, 0);
}
