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

/* GNU multiple precision library: */
#include <gmp.h>

#include <applib/Intern.h>
#include <applib/Diag.h>
#include <applib/PtrVec.h>
#include "SymTab.h"

void
output_depend(Symbol *s)
{
  unsigned i;
  PtrVec *vec = ptrvec_create();

  diag_printf("Dependencies for \"%s\"\n", symbol_QualifiedName(s, '.'));

  symbol_ComputeDependencies(s, vec);

  ptrvec_sort_using(vec, symbol_SortByQualifiedName);

  for (i = 0; i < vec_len(vec); i++)
    diag_printf("  %s\n",
		symbol_QualifiedName(symvec_fetch(vec,i), '_'));
}
