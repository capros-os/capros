#ifndef __PTE_H__
#define __PTE_H__
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

#include <kerninc/Set.h>

#ifndef _ASM_U
#define _ASM_U(x) (x##u)
#endif

#define PTE_V	 _ASM_U(0x001)	/* valid (Intel: 'present') */
#define PTE_W    _ASM_U(0x002)	/* writable */
#define PTE_USER _ASM_U(0x004)	/* user-accessable page */
#define PTE_WT   _ASM_U(0x008)	/* write through */
#define PTE_CD   _ASM_U(0x010)	/* cache disable */
#define PTE_ACC  _ASM_U(0x020)	/* accessed */
#define PTE_DRTY _ASM_U(0x040)	/* dirty */
#define PTE_PGSZ _ASM_U(0x080)	/* large page  (PDE, >=Pentium only) */
#define PTE_GLBL _ASM_U(0x100)	/* global page (PDE,PTE, >= PPro only) */
#define PTE_SW0  _ASM_U(0x200)	/* SW use */
#define PTE_SW1  _ASM_U(0x400)	/* SW use */
#define PTE_SW2  _ASM_U(0x800)	/* SW use */

#define PTE_FRAMEBITS _ASM_U(0xfffff000)
#define PTE_INFOBITS  _ASM_U(0x00000fff)

/* The following value is used during mapping construction to detect
 * dependency zaps on the PTE under construction. When building a PTE,
 * if the existing PTE is invalid, first set it to this value, then do
 * the necessary translation, then check that the PTE is not
 * PTE_ZAPPED before overwriting. Note that this value represents an
 * INVALID PTE, but one that can be readily distinguished from the
 * result of a call to PTE::Invalidate(). */
#define PTE_IN_PROGRESS  _ASM_U(0xfffff000)
#define PTE_ZAPPED       _ASM_U(0x0)

#ifndef __ASSEMBLER__

/* This file requires #include <kerninc/kernel.hxx> (for assert) */

/* Be very careful about using PTE_CLR -- it is vitally important that
   PteZapped get set in all cases where the authority of a PTE has
   been reduced. */

extern bool PteZapped;

typedef struct PTE PTE;
struct PTE {
  /* Declared in IPC-vars.cxx: */

  uint32_t w_value;

#if 0
  void operator=(uint32_t w);
  bool operator==(const PTE& other);
  bool operator!=(const PTE& other);
#endif
};

INLINE bool
pte_is(PTE *pte, unsigned flg)
{
  return WSET_IS(pte->w_value, flg);
}

INLINE bool
pte_isnot(PTE *pte, unsigned flg)
{
  return WSET_ISNOT(pte->w_value, flg);
}

INLINE void
pte_set(PTE *pte, unsigned flg)
{
  WSET_SET(pte->w_value, flg);
}

INLINE void
pte_clr(PTE *pte, unsigned flg)
{
  WSET_CLR(pte->w_value, flg);
}

/* Defined in Mapping.c: */
extern PTE* KernPageDir;
extern kpmap_t KernPageDir_pa;

#ifdef KVA_PTEBUF
/* Defined in IPC-vars.c: */
extern PTE* pte_kern_ptebuf;
#endif

#define NPTE_PER_PAGE (EROS_PAGE_SIZE / sizeof (PTE))

/* Former member functions of PTE */

INLINE void 
pte_Invalidate(PTE* thisPtr)
{
  thisPtr->w_value = PTE_ZAPPED;
  PteZapped = true;
}

INLINE void 
pte_WriteProtect(PTE* thisPtr)
{
  pte_clr(thisPtr, PTE_W);
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

INLINE bool 
pte_CanMergeWith(PTE* thisPtr, PTE *pte)
{
  uint32_t w = ((uint32_t) thisPtr) ^ ((uint32_t) pte);
  if (w & ~EROS_PAGE_MASK)
    return false;
  return true;
}

INLINE bool
pte_NotEqualTo(PTE* thisPtr, const PTE* other) {
  uint32_t result = thisPtr->w_value ^ other->w_value;
  result &= ~(PTE_ACC | PTE_DRTY);
  return (result != 0);
}

/*void pte_ZapMappingPage(kva_t pva);*/

#ifdef OPTION_DDB
  /* Following lives in PageFault.cxx: */
void pte_ddb_dump(PTE* thisPtr);
#endif
  
#ifndef NDEBUG
/* Following lives in PageFault.cxx: */
struct ObjectHeader;
bool pte_ObIsNotWritable(struct ObjectHeader *pObj);
#endif

INLINE void 
pte_ZapMappingPage(kva_t pva)
{
  uint32_t entry = 0;
  PTE *pte = (PTE*) pva;
  for (entry = 0; entry < NPTE_PER_PAGE; entry++)
    pte_Invalidate(pte);
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
#endif /* __PTE_H__ */
