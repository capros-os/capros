#ifndef __PTE_H__
#define __PTE_H__
/*
 * Copyright (C) 1998, 1999, Jonathan S. Shapiro.
 * Copyright (C) 2006, Strawberry Development Group.
 *
 * This file is part of the CapROS Operating System.
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

struct PTE;
typedef struct PTE PTE;

#include <kernel/PTE486.h>
/* PTE486.h has declarations private to the architecture (HAL). 
   This file, arch-kerninc/PTE.h, has declarations exported outside the HAL,
   where the (inline) implementation is HAL-dependent. */

extern bool PteZapped;

/* Defined in Mapping.c: */
extern PTE* KernPageDir;
extern kpmap_t KernPageDir_pa;

#define NPTE_PER_PAGE (EROS_PAGE_SIZE / sizeof (PTE))

INLINE bool
pte_isValid(PTE *pte)
{
  return pte->w_value & PTE_V;
}

INLINE void 
pte_Invalidate(PTE* thisPtr)
{
  thisPtr->w_value = PTE_ZAPPED;
  PteZapped = true;
}

INLINE kpa_t 
pte_PageFrame(PTE* thisPtr)
{
  return (thisPtr->w_value & PTE_FRAMEBITS);
}

INLINE uint32_t 
pte_AsWord(PTE* thisPtr)
{
  return thisPtr->w_value;
}

/* Return true iff the mapping table entry pointed to by MTE1,
   which is in a table whose level is mapLevel,
   is in the same table as MTE2. */
INLINE bool 
mte_InSameTable(void * MTE1, void * MTE2, int mapLevel)
{
  uint32_t w = ((uint32_t) MTE1) ^ ((uint32_t) MTE2);
  return !(w & ~EROS_PAGE_MASK);
}

#ifdef OPTION_DDB
  /* Following lives in PageFault.cxx: */
void pte_ddb_dump(PTE* thisPtr);
#endif
  
#ifndef NDEBUG
/* Following lives in PageFault.c: */
struct PageHeader;
bool pte_ObIsNotWritable(struct PageHeader * pObj);
#endif

INLINE void
UpdateTLB(void)
{
  if (PteZapped)
    mach_FlushTLB();
}

#endif /* __PTE_H__ */
