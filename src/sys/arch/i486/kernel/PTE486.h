#ifndef __PTE486_H__
#define __PTE486_H__
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

#include <eros/machine/target-asm.h>
#include <kerninc/ObjectHeader.h>

#define PTE_V	 0x001	/* valid (Intel: 'present') */
#define PTE_W    0x002	/* writable */
#define PTE_USER 0x004	/* user-accessable page */
#define PTE_WT   0x008	/* write through */
#define PTE_CD   0x010	/* cache disable */
#define PTE_ACC  0x020	/* accessed */
#define PTE_DRTY 0x040	/* dirty */
#define PTE_PGSZ 0x080	/* large page  (PDE, >=Pentium only) */
#define PTE_GLBL 0x100	/* global page (PDE,PTE, >= PPro only) */

#define PTE_FRAMEBITS 0xfffff000

/* The following value is used during mapping construction to detect
 * dependency zaps on the PTE under construction. When building a PTE,
 * if the existing PTE is invalid, first set it to this value, then do
 * the necessary translation, then check that the PTE is not
 * PTE_ZAPPED before overwriting. Note that this value represents an
 * INVALID PTE, but one that can be readily distinguished from the
 * result of a call to PTE::Invalidate(). */
#define PTE_IN_PROGRESS  0xfffff000
#define PTE_ZAPPED       0x0

#ifndef __ASSEMBLER__

extern bool PteZapped;

struct PTE {
  /* Declared in IPC-vars.cxx: */

  uint32_t w_value;
};

INLINE bool
pte_is(PTE *pte, unsigned flg)
{
  return pte->w_value & flg;
}

INLINE bool
pte_isnot(PTE *pte, unsigned flg)
{
  return !(pte->w_value & flg);
}

INLINE void
pte_set(PTE *pte, unsigned flg)
{
  pte->w_value |= flg;
}

/* Be very careful about using pte_clr -- it is vitally important that
   PteZapped get set in all cases where the authority of a PTE has
   been reduced. */

INLINE void
pte_clr(PTE *pte, unsigned flg)
{
  pte->w_value &= ~flg;
}

#ifdef KVA_PTEBUF
/* Defined in IPC-vars.c: */
extern PTE* pte_kern_ptebuf;
#endif

#if 0
INLINE bool
pte_NotEqualTo(PTE* thisPtr, const PTE* other) {
  uint32_t result = thisPtr->w_value ^ other->w_value;
  result &= ~(PTE_ACC | PTE_DRTY);
  return (result != 0);
}
#endif

INLINE void
pte_WriteProtect(PTE* thisPtr)
{
  pte_clr(thisPtr, PTE_W);
  PteZapped = true;
}

struct PageHeader;
struct MapTabHeader;
INLINE struct PageHeader *
MapTab_ToPageH(struct MapTabHeader * mth)
{
  return (struct PageHeader *)mth;
}

INLINE struct PTE *
MapTabHeaderToKVA(struct MapTabHeader * mth)
{
  struct PageHeader * pageH = MapTab_ToPageH(mth);
  return (struct PTE *) pageH_GetPageVAddr(pageH);
}

INLINE void
mach_FlushTLB()
{
  __asm__ __volatile__("mov %%cr3,%%eax;mov %%eax,%%cr3"
		       : /* no output */
		       : /* no input */
		       : "eax");
}

INLINE void
mach_FlushTLBWith(klva_t lva)
{
#ifdef SUPPORT_386
  if (CpuType > 3)
    __asm__ __volatile__("invlpg  (%0)"
			 : /* no output */
			 : "r" (lva)
			 );
  else
    mach_FlushTLB();
#else
  __asm__ __volatile__("invlpg  (%0)"
		       : /* no output */
		       : "r" (lva)
		       );
#endif
}

/* Notes on EROS page map usage:
 * 
 * Page is present if either AuxPresent or Present is set. Present is
 * sometimes turned off during mapping page aging as a way of finding
 * out if the mapping page is, in fact, being used.  The AuxPresent
 * bit provides a mechanism to efficiently restore the Present bit in
 * this case.  Setting Present without setting AuxPresent is always a
 * bug.
 * 
 * A page that in principle writable should have it's AuxWritable bit
 * set to '1'. If the page is not write hazarded, the Writable bit
 * will also be set to '1'.  If the page is write hazarded, the
 * Writable bit will be set to 0.  The discrepancy between these bits
 * is the tipoff that the issue is a hazard.
 */

#endif /* __ASSEMBLER__  */
#endif /* __PTE486_H__ */
