/*
 * Copyright (C) 2001, Jonathan S. Shapiro.
 * Copyright (C) 2006, 2008, 2009, Strawberry Development Group.
 *
 * This file is part of the CapROS Operating System,
 * and is derived from the EROS Operating System.
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
Research Projects Agency under Contract No. W31P4Q-07-C-0070.
Approved for public release, distribution unlimited. */

/* Implementation of kernel malloc. */

#include <kerninc/kernel.h>
#include <kerninc/util.h>
#include <kerninc/ObjectCache.h>
#include <kerninc/ObjectHeader.h>
#include <kerninc/PhysMem.h>
#include <kerninc/Machine.h>
#include <kerninc/Node.h>
#include <kerninc/Depend.h>
#include <kerninc/heap.h>
     
#define dbg_init	0x1u
#define dbg_avail	0x2u
#define dbg_alloc	0x4u
#define dbg_new		0x8u

/* Following should be an OR of some of the above */
#define dbg_flags   ( 0u )

#define DEBUG(x) if (dbg_##x & dbg_flags)


extern kpa_t align_up(kpa_t addr, uint32_t alignment);

/* The following are initialized as part of mach_BootInit. */
kva_t heap_start;
kva_t heap_end;		/* pointer to next byte to allocate */
kva_t heap_defined;	/* physical pages are allocated to here */
kva_t heap_bound;	/* virtual addresses are allocated to here */

void
heap_init()
{
  /* We do not need to allocate the PAGES for the heap content, but we
   * *do* need to allocate the page *tables* that will map those pages.
   * Every page directory needs to have these page tables mapped.
   * If we allocated them later, we would have to find all page
   * directories to update them.
   *
   * The heap size is not dependent on the RAM size; if it were,
   * the calculation of maxMappedPA in physMem_Init_MD would need to change.
   */

  printf("Heap size is %#" PRIxkpsize " bytes.\n", (kpsize_t)heap_Size);

  mach_HeapInit(heap_Size);

  /* Heap end should always be word aligned. */
  assert((heap_end & 0x3u) == 0);

  dprintf(false, "heap_start, heap_bound = 0x%08x, 0x%08x\n",
	  (unsigned)heap_start, (unsigned) heap_bound);
}

/* If possible, it's always better to grab a real physical page
 * rather than steal one from the page cache. This ensures that
 * the early allocations work.  Failing that, we'll try grabbing a
 * page from the page cache instead, in which case we may Yield.
 */
kpa_t
heap_AcquirePage(void)
{
  if (physMem_ChooseRegion(EROS_PAGE_SIZE, &physMem_pages)) {
    return physMem_Alloc(EROS_PAGE_SIZE, &physMem_pages);
  }
  else {
    printf("Trying to allocate from object heap.\n");

    PageHeader * pageH = objC_GrabPageFrame();

    kpa_t pa = pageH_GetPhysAddr(pageH);
    pageH_ToObj(pageH)->obType = ot_PtKernelUse;
    // objH_SetFlags(pageH, OFLG_DIRTY);	/* always */

    return pa;
  }
}

/* FIX: This implementation of malloc() is a placeholder, since free()
 * clearly would not work correcly without maintaining some sort of
 * record of what was allocated where.
 */
// May Yield.
void *
malloc(size_t nBytes)
{
  void *vp = 0;
  /* We are going to allocate the nBytes bytes beginning at heap_end. */

  DEBUG(alloc)
    printf("malloc: heap_end, heap_def, heap_limit now 0x%08x 0x%08x 0x%08x\n",
		   (unsigned) heap_end,
		   (unsigned) heap_defined,
		   (unsigned) heap_bound);

  /* Make sure there are enough virtual addresses available. */
  if (heap_bound - heap_end < nBytes)
    fatal("Heap space exhausted. %d wanted %d avail\n", nBytes, heap_bound - heap_end);

  /* Make sure there are enough physical pages allocated. */
  mach_EnsureHeap(heap_end + nBytes);

  /* FIX: get the alignment right! */
  vp = (void *) heap_end;

  *((char *) vp) = 0;		/* cause kernel to crash if page not present */
  heap_end += nBytes;
  heap_end = align_up(heap_end, 4);
    
  DEBUG(alloc)
    dprintf(false,
		    "malloc() returns nBytes=%d at 0x%08x\n", nBytes,
		    vp);

  return vp;
}
