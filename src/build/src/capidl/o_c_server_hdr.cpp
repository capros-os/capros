/*
 * Copyright (C) 2003, The EROS Group, LLC.
 * Copyright (C) 2022, Charles Landau.
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
#include <string.h>
#include <stdbool.h>
#include <stdio.h>
#include <errno.h>

/* GNU multiple precision library: */
#include <gmp.h>

#include <applib/xmalloc.h>
#include <applib/Intern.h>
#include <applib/Diag.h>
#include <applib/PtrVec.h>
#include <applib/path.h>
#include <applib/buffer.h>
#include "SymTab.h"
#include "util.h"
#include "backend.h"
#include "o_c_util.h"

static Buffer *preamble;

static void
calc_sym_depend(Symbol *s, PtrVec *vec)
{
  switch(s->cls) {
  case sc_absinterface:
  case sc_interface:
    {
      symbol_ComputeDependencies(s, vec);

      {
	Symbol *targetUoc = symbol_UnitOfCompilation(s);
	if (!ptrvec_contains(vec, targetUoc))
	  ptrvec_append(vec, targetUoc);
      }

      for (const auto eachChild : s->children)
	calc_sym_depend(eachChild, vec);
    }

  case sc_operation:
    {
      symbol_ComputeDependencies(s, vec);
      return;
    }

  default:
    return;
  }

  return;
}

static void
compute_server_dependencies(Symbol *scope, PtrVec *vec)
{
  /* Export subordinate packages first! */
  for (const auto eachChild : scope->children) {
    if (eachChild->cls != sc_package && eachChild->isActiveUOC)
      calc_sym_depend(eachChild, vec);

    if (eachChild->cls == sc_package)
      compute_server_dependencies(eachChild, vec);
  }

  return;
}

static void
emit_op_dispatcher(Symbol *s, FILE *outFile)
{
  fprintf(outFile, "\nfixreg_t implement_%s(",
	  symbol_QualifiedName(s, '_'));

  bool first = true;
  for (const auto eachChild : s->children) {
    FormalSym * fsym = dynamic_cast<FormalSym*>(eachChild);
    assert(fsym);

    Symbol *argType = symbol_ResolveRef(eachChild->type);
    Symbol *argBaseType = symbol_ResolveType(argType);
    
    if (!first)
      fprintf(outFile, ", ");
    else
      first = false;
    
    if (! fsym->isOutput) {
      output_c_type(argBaseType, outFile, 0);
      fprintf(outFile, " %s", eachChild->name);
    } else {
      output_c_type(argBaseType, outFile, 0);
      fprintf(outFile, " * %s /* OUT */", eachChild->name);
    }
  }
  fprintf(outFile, ");\n");

}

static void
emit_decoders(Symbol *s, FILE *outFile)
{
  if (s->mark)
    return;

  s->mark = true;

  switch(s->cls) {
  case sc_absinterface:
  case sc_interface:
    {
      if (s->baseType)
	emit_decoders(symbol_ResolveRef(s->baseType), outFile);

      for (const auto eachChild : s->children)
	emit_decoders(eachChild, outFile);

      return;
    }

  case sc_operation:
    {
      if (s->isActiveUOC)
	emit_op_dispatcher(s, outFile);

      return;
    }

  default:
    return;
  }

  return;
}

void
emit_server_header_decoders(Symbol *scope, FILE *outFile)
{
  /* Export subordinate packages first! */
  for (const auto eachChild : scope->children) {
    if (eachChild->cls != sc_package && eachChild->isActiveUOC)
      emit_decoders(eachChild, outFile);

    if (eachChild->cls == sc_package)
      emit_server_header_decoders(eachChild, outFile);
  }

  return;
}

void 
output_c_server_hdr (Symbol *universalScope, BackEndFn fn)
{
  extern const char *outputFileName;
  FILE *outFile;
  unsigned i;

  PtrVec *vec = ptrvec_create();

  if (outputFileName == 0)
    diag_fatal(1, "Need to provide output file name.\n");

  if (strcmp(outputFileName, "-") == 0 )
    outFile = stdout;
  else {
    outFile = fopen(outputFileName, "w");
    if (outFile == NULL)
      diag_fatal(1, "Could not open output file \"%s\" -- %s\n",
		 outputFileName, strerror(errno));
  }

  compute_server_dependencies(universalScope, vec);

  preamble = buffer_create();

  buffer_appendString(preamble, "#include <stdbool.h>\n");
  buffer_appendString(preamble, "#include <stddef.h>\n");
  buffer_appendString(preamble, "#include <eros/target.h>\n");

  for (i = 0; i < vec_len(vec); i++) {
    buffer_appendString(preamble, "#include <idl/");
    buffer_appendString(preamble, symbol_QualifiedName(symvec_fetch(vec,i), '/'));
    buffer_appendString(preamble, ".h>\n");
  }

  buffer_freeze(preamble);

  {
    BufferChunk bc;
    off_t pos = 0;
    off_t end = buffer_length(preamble);

    while (pos < end) {
      bc = buffer_getChunk(preamble, pos, end - pos);
      fwrite(bc.ptr, 1, bc.len, outFile);
      pos += bc.len;
    }
  }

  symbol_ClearAllMarks(universalScope);

  emit_server_header_decoders(universalScope, outFile);

  if (outFile != stdout) 
    fclose(outFile);
}
