/*
 * Copyright (C) 2001, Jonathan S. Shapiro.
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

/* Implementation of kernel malloc for EROS. */

#include <kerninc/kernel.h>
/*#include <kerninc/util.h>*/
#include <kerninc/ObjectCache.h>
#include <kerninc/ObjectHeader.h>
#include <kerninc/PhysMem.h>
#include <kerninc/Machine.h>
     
#define dbg_init	0x1u
#define dbg_avail	0x2u
#define dbg_alloc	0x4u
#define dbg_new		0x8u

/* Following should be an OR of some of the above */
#define dbg_flags   ( 0u )

#define DEBUG(x) if (dbg_##x & dbg_flags)


extern int end;
extern kpa_t align_up(kpa_t addr, uint32_t alignment);


/* /heap_end/ identifies the first available byte in the heap.
 * /heap_bound/ identifies the first unmapped byte following the heap.
 *
 * THESE VALUES ARE RESET IN THE MACHINE-DEPENDENT KERNEL MAP BUILDING
 * CODE.
 */
kva_t heap_start = (kva_t) &end;
kva_t heap_end = (kva_t) &end;	/* pointer to next alloc ignoring freelist */
kva_t heap_defined = (kva_t) &end; /* validly mapped map out to here */
kva_t heap_bound = (kva_t) &end;

void
heap_init()
{
  /* Heap end should always be word aligned. */
  assert((heap_end & 0x3u) == 0);

  /* Initialization of heap_defined, heap_bound, heap_end is handled
     in the machine-specific MapKernel code, which is called very
     early in mach_BootInit(). */

  dprintf(false, "heap_start, heap_bound = 0x%08x, 0x%08x\n",
	  (unsigned)heap_start, (unsigned) heap_bound);
}

kpa_t
acquire_heap_page()
{
  if (physMem_ChooseRegion(EROS_PAGE_SIZE, &physMem_pages)) {
    return physMem_Alloc(EROS_PAGE_SIZE, &physMem_pages);
  }
  else {
    ObjectHeader *pHdr = 0;
    kpa_t pa;

    printf("Trying to allocate from object heap.\n");


    pHdr = objC_GrabPageFrame();

    pa = VTOP(objC_ObHdrToPage(pHdr));
    pHdr->obType = ot_PtKernelHeap;
    objH_SetFlags(pHdr, OFLG_DIRTY);	/* always */

    return pa;
  }
}

/* grow_heap() must find (or clear) an available physical page and
 * cause it to become mapped at the end of the physical memory map.
 */
void
grow_heap(kva_t target)
{
  assert((heap_defined & EROS_PAGE_MASK) == 0);

  while (heap_defined < target) {
    /* If possible, it's always better to grab a real physical page
     * rather than steal one from the page cache. This ensures that
     * the early allocations work.  Failing that, we'll try grabbing a
     * page from the page cache instead.
     */
    kpa_t pa = acquire_heap_page();

    mach_MapHeapPage(heap_defined, pa);

    heap_defined += EROS_PAGE_SIZE;
  }
}

/* FIX: This implementation of malloc() is a placeholder, since free()
 * clearly would not work correcly without maintaining some sort of
 * record of what was allocated where.
 */
void *
malloc(size_t nBytes)
{
  void *vp = 0;

  DEBUG(alloc)
    printf("malloc: heap_end, heap_def, heap_limit now 0x%08x 0x%08x 0x%08x\n",
		   (unsigned) heap_end,
		   (unsigned) heap_defined,
		   (unsigned) heap_bound);

  if (heap_bound - heap_end < nBytes)
    fatal("Heap space exhausted. %d wanted %d avail\n", nBytes, heap_bound - heap_end);

  if (heap_defined - heap_end < nBytes)
    grow_heap(heap_end + nBytes);

  assert(heap_defined - heap_end >= nBytes);

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
