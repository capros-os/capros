#ifndef __PROCESS486_H__
#define __PROCESS486_H__
/*
 * Copyright (C) 1998, 1999, 2001, Jonathan S. Shapiro.
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

#include <arch-kerninc/PTE.h>
//#include <eros/Invoke.h>
//#include <kerninc/Invocation.h>
//#include <kerninc/Activity.h>

/* Machine-specific functions for process operations private to the HAL: */

#ifdef USES_MAPPING_PAGES
void mach_SetMappingTable(kpmap_t pAddr);
kpmap_t mach_GetMappingTable();
#endif

#ifdef NEW_KMAP
struct MappingWindow;
#define WM_NOKVA ~0u
extern struct MappingWindow *PageDirWindow;
extern struct MappingWindow *TempMapWindow;

kva_t mach_winmap(struct MappingWindow* mw, kva_t lastva, kpa_t pa);
void mach_winunmap(struct MappingWindow* mw, kva_t va);
#else
kva_t mach_winmap(int mw, kva_t lastva, kpa_t pa);
void mach_winunmap(int mw, kva_t va);
#endif

bool proc_HasDevicePriveleges(Process* thisPtr);

/* Return 0 if no mapping can be found with the desired access,
 * otherwise return the kernel *virtual* PAGE address containing the
 * virtual BYTE address uva.
 * 
 * Note that successive calls to this, after inlining, should
 * successfully CSE the page directory part of the computation.
 */
INLINE PTE*
proc_TranslatePage(Process *p, ula_t ula, uint32_t mode, bool forWriting)
{
  PTE *pte = 0;
#ifdef OPTION_SMALL_SPACES
  if (p->MappingTable == KernPageDir_pa && p->smallPTE == 0)
    return 0;
#else
  if (p->MappingTable == KernPageDir_pa)
    return 0;
#endif
  
  assert(p->MappingTable);
  
  if (p->MappingTable) {
    PTE* pde = (PTE*) PTOV(p->MappingTable);
    uint32_t ndx0 = (ula >> 22) & 0x3ffu;
    uint32_t ndx1 = (ula >> 12) & 0x3ffu;

    pde += ndx0;

    if (pte_is(pde, mode)) {
      if (forWriting && pte_isnot(pde, PTE_W))
	goto fail;


      pte = (PTE*) PTOV( (pte_AsWord(pde) & ~EROS_PAGE_MASK) );

      pte += ndx1;
  
      if (pte_is(pte, mode)) {
	if (forWriting && pte_isnot(pte, PTE_W))
	  goto fail;

	return pte;
      }
    }
  }
fail:
  return 0;
}

#ifdef OPTION_SMALL_SPACES
INLINE void              
proc_SwitchToLargeSpace(Process* thisPtr)
{
  thisPtr->smallPTE = 0;
  thisPtr->bias = 0;
  thisPtr->limit = UMSGTOP;
  thisPtr->MappingTable = KernPageDir_pa;
}
#endif

#endif /* __PROCESS486_H__ */
