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

#include <stdlib.h>
#include <memory.h>

#include <eros/target.h>
#include <erosimg/DiskKey.h>

#include "PtrMap.h"

typedef struct vw vw;
struct vw {
  const void *ptr;
  KeyBits value;
};

struct PtrMap {
  unsigned nPtrs;
  unsigned maxPtrs;
  struct vw *ptrs;
} ;

PtrMap *
ptrmap_create()
{
  PtrMap *pm = (PtrMap *) malloc(sizeof(PtrMap));

  pm->ptrs = 0;
  pm->nPtrs = 0;
  pm->maxPtrs = 0;

  return pm;
}

void
ptrmap_destroy(PtrMap *pm)
{
  free(pm->ptrs);
  free(pm);
}

/* Inefficient as hell, but it works: */
bool 
ptrmap_Contains(PtrMap *pm, const void *ptr)
{
  unsigned i;

  for (i = 0; i < pm->nPtrs; i++)
    if (pm->ptrs[i].ptr == ptr)
      return true;

  return false;
}

/* Inefficient as hell, but it works: */
bool
ptrmap_Lookup(PtrMap *pm, const void *ptr, KeyBits *w)
{
  unsigned i;
  for (i = 0; i < pm->nPtrs; i++)
    if (pm->ptrs[i].ptr == ptr) {
      *w = pm->ptrs[i].value;
      return true;
    }

  return false;
}

void
ptrmap_Add(PtrMap *pm, const void *ptr, KeyBits value)
{
  if (ptrmap_Contains(pm, ptr))
    return;

  if (pm->nPtrs >= pm->maxPtrs) {
    vw *newptrs;

    pm->maxPtrs += 1024;

    newptrs = (vw *) malloc(sizeof(vw) * pm->maxPtrs);
    if (pm->ptrs)
      memcpy(newptrs, pm->ptrs, pm->nPtrs * sizeof(void *)); 

    free(pm->ptrs);
    pm->ptrs = newptrs;
  }

  pm->ptrs[pm->nPtrs].ptr = ptr;
  pm->ptrs[pm->nPtrs++].value = value;
}
