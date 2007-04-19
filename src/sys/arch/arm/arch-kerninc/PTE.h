#ifndef __PTE_H__
#define __PTE_H__
/*
 * Copyright (C) 1998, 1999, Jonathan S. Shapiro.
 * Copyright (C) 2006, 2007, Strawberry Development Group.
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
/* This material is based upon work supported by the US Defense Advanced
Research Projects Agency under Contract Nos. W31P4Q-06-C-0040 and
W31P4Q-07-C-0070.  Approved for public release, distribution unlimited. */

struct PTE;
typedef struct PTE PTE;

#include <kernel/PTEarm.h>
/* PTEarm.h has declarations private to the architecture (HAL). 
   This file, arch-kerninc/PTE.h, has declarations exported outside the HAL. */

extern bool PteZapped, flushCache;

INLINE bool
pte_isValid(PTE *pte)
{
  return pte->w_value & PTE_VALIDBITS;
}

INLINE void 
pte_Invalidate(PTE* thisPtr)
{
  const uint32_t pteval = thisPtr->w_value;
  if (pteval & PTE_VALIDBITS) {	// it was valid
    PteZapped = flushCache = true;
  } else {
    // It was invalid, but could there be cache entries dependent on it?
    if (pteval == PTE_ZAPPED) return;
    if (pteval != PTE_IN_PROGRESS)
      flushCache = true;
    // Even if it was invalid, we want to change it to PTE_ZAPPED.
  }
  thisPtr->w_value = PTE_ZAPPED;
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
  kva_t w = ((kva_t) MTE1) ^ ((kva_t) MTE2);
  kva_t mask = (mapLevel == 1) ? 0x3fff
               : CPT_SIZE-1;
  return !(w & ~mask);
}

#ifdef OPTION_DDB
void pte_ddb_dump(PTE* thisPtr);
#endif
  
#ifndef NDEBUG
struct PageHeader;
bool pte_ObIsNotWritable(struct PageHeader * pObj);
#endif

INLINE void
UpdateTLB(void)
{
  if (PteZapped)
    mach_FlushBothTLBs();
  if (flushCache)
    mach_FlushBothCaches();
}

#endif /* __PTE_H__ */
