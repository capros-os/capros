/*
 * Copyright (C) 2002, The EROS Group, LLC.
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
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>

/* GNU multiple precision library: */
#include <gmp.h>

#include <applib/Intern.h>
#include <applib/Diag.h>
#include <applib/PtrVec.h>
#include <applib/xmalloc.h>
#include "SymTab.h"
#include "util.h"

#define REGISTER_BITS    32
#define ENUMERAL_SIZE    4
#define TARGET_LONG_SIZE 4

static void
fixup(Symbol *s)
{
  switch(s->cls){
  case sc_operation:
    {
      assert(symbol_IsVoidType(symbol_voidType));

      /* If the return type is non-void, stick it on the end of the
       * parameter list as an OUT parameter named "_retVal" */
      if (symbol_IsVoidType(s->type) == false) {
	Symbol *retVal = symbol_create_inScope("_retVal", s->isActiveUOC, sc_outformal, s);
	retVal->type = s->type;
	s->type = symbol_voidType;
      }

      /* Reorder the parameters so that all of the IN parameters
       * appear first. */
      {
	size_t first_out = UINT_MAX;
        size_t i;

	for (i = 0; i < s->children.size(); i++) {
	  Symbol * child = s->children[i];

	  if (child->cls == sc_outformal && i < first_out) {
	    first_out = i;
	    continue;
	  }

	  if (child->cls == sc_formal && i > first_out) {
	    size_t j;
	    for (j = i; j > first_out; j--)
              s->children[j] = s->children[j-1];
	    s->children[first_out]= child;
	    first_out++;
	  }
	}
      }

      break;
    }
  default:
    {
      /* Values do not need to be rewritten for C (ever) */
      /* Types need to be rewritten, but we rely on the fact that the
       * xformer is run over the entire input tree, and the type
       * symbol will therefore be caught as the child of some scope. */
      for (const auto eachChild : s->children)
	fixup(eachChild);

      break;
    }
  }
}

bool
c_typecheck(Symbol *s)
{
  bool result = true;
  Symbol *ty = s->type;

  if (ty) {
    static InternedString pure_int = 0;
    static InternedString pure_ws8 = 0;
    static InternedString pure_ws32 = 0;

    if (pure_int == 0) {
      pure_int = intern("#int");
      pure_ws8 = intern("#wstring8");
      pure_ws32 = intern("#wstring32");
    }

    if (ty->name == pure_int ||
	ty->name == pure_ws8 ||
	ty->name == pure_ws32) {
      diag_printf("%s \"%s\" specifies unbounded type  \"%s\"\n",
		   symbol_ClassName(s),
		   symbol_QualifiedName(s, '.'),
		   ty->name);
      result = false;
    }

    if (ty->cls == sc_seqType && ty->value == 0) {
      diag_printf("%s \"%s\" specifies unbounded sequence type, which is not (yet) supported\n",
		  symbol_ClassName(s),
		  symbol_QualifiedName(s, '.'));
      result = false;
    }

    if (ty->cls == sc_bufType && ty->value == 0) {
      diag_printf("%s \"%s\" specifies unbounded buffer type, which is not (yet) supported\n",
		  symbol_ClassName(s),
		  symbol_QualifiedName(s, '.'));
      result = false;
    }
  }

  if (s->cls == sc_exception && ! s->children.empty()) {
    diag_printf("%s \"%s\" EROS exceptions should not have members\n",
		symbol_ClassName(s),
		symbol_QualifiedName(s, '.'));
    result = false;
  }

  if (s->cls == sc_symRef)
    return result;

  for (const auto eachChild : s->children)
    result = result && c_typecheck(eachChild);

  return result;
}

void
rewrite_for_c(Symbol *s)
{
  fixup(s);
}
