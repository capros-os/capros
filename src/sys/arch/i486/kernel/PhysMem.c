/*
 * Copyright (C) 1998, 1999, Jonathan S. Shapiro.
 * Copyright (C) 2005-2010, Strawberry Development Group
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

#include <kerninc/kernel.h>
#include <kerninc/util.h>
#include <kerninc/PhysMem.h>
#include <kerninc/multiboot.h>
#include <kerninc/heap.h>

#define dbg_init	0x1u

/* Following should be an OR of some of the above */
#define dbg_flags   ( 0u )

#define DEBUG(x) if (dbg_##x & dbg_flags)


kpa_t physMem_PhysicalPageBound = 0;	// highest physical address of RAM +1
kpg_t physMem_TotalPhysicalPages = 0;	// approximate

/* The area we will reserve as ROM: */
kpa_t ROMBase  =  0xc0000;
kpa_t ROMBound = 0x100000;

static void checkBounds(kpa_t base, kpa_t bound)
{
  if (bound > physMem_PhysicalPageBound) {        /* take max */
    physMem_PhysicalPageBound = align_up(bound, EROS_PAGE_SIZE);
  }
  kpg_t pgs = (bound - base) / EROS_PAGE_SIZE;
  physMem_TotalPhysicalPages += pgs;

  /* Check if it overlaps ROM area. */
  if (   base < ROMBound
      && bound > ROMBase ) {
    /* We don't allow memory at 0xFFFFF, only ROM. */
    if (bound >= ROMBound) {
      printf("base=%#" PRIxkpa ", bound=%#" PRIxkpa "\n", base, bound);
      fatal("ROM conflict\n");
    } else {
      /* Truncate ROM area so as not to overlap. */
      ROMBase = bound;
    }
  }
}

kpa_t maxMappedPA;
void
physMem_Init_MD()
{
  uint32_t mmapLength;
  struct grub_mmap * mp;

  /* Currently, all of physical memory needs to be mapped at KVA.
  (Yes, this needs to be fixed.)
  Compute the highest physical address we have room to map there. */
  maxMappedPA = (KVA_TOSPACE
                 - 2*1024*EROS_PAGE_SIZE // for two roundings
                 - heap_Size
                 - KTUNE_NCONTEXT * sizeof(Process) )
                & ~ EROS_PAGE_MASK;

  DEBUG (init) printf("MultibootInfoPtr = %x\n", MultibootInfoPtr);

  int ret;
  PmemInfo * pmi;	// value isn't used
  for (mmapLength = MultibootInfoPtr->mmap_length,
         mp = (struct grub_mmap *) MultibootInfoPtr->mmap_addr;
       mmapLength > 0;
       ) {
    kpa_t base = ((kpa_t)mp->base_addr_high << 32) + (kpa_t)mp->base_addr_low;
    kpsize_t size = ((kpsize_t)mp->length_high << 32)
                    + (kpsize_t)mp->length_low;
    kpsize_t bound = base + size;

    DEBUG (init) printf("0x%08lx 0x%08lx 0x%08lx 0x%08lx 0x%08lx 0x%08lx\n",
       mp->size,
       mp->base_addr_low,
       mp->base_addr_high,
       mp->length_low,
       mp->length_high,
       mp->type);

    if (mp->type == 1) {	/* available RAM */
      // Don't accept more physical memory than we can map:
      if (bound > maxMappedPA)
        bound = maxMappedPA;
      if (bound > base) {
        ret = physMem_AddRegion(base, bound, MI_MEMORY, &pmi);
        assert(!ret);
        checkBounds(base, bound);
      }
    }
    /* On to the next. */
    /* mp->size does not include the size of the size field itself,
       so add 4. */
    mmapLength -= (mp->size + 4);
    mp = (struct grub_mmap *) (((char *)mp) + (mp->size + 4));
  }

  /* Preloaded modules are contained in mmap memory,
     so no need to add regions for them. */

  ret = physMem_AddRegion(ROMBase, ROMBound, MI_BOOTROM, &pmi);
  assert(!ret);

  DEBUG(init) {
    /* Print status */
  }

  /* Reserve regions of physical memory that are already used. */

  /* Reserve kernel code and data and bss.
     The kernel stack is within the bss. */
  physMem_ReserveExact(VTOP(&_start), (kpa_t)((kva_t)&end - (kva_t)&_start));
                                                                                
  /* At the moment, we don't do anything to reserve the memory
  containing the initialized preloaded non-persistent objects.
  We simply hope that nothing clobbers those pages before we use them.
  The reason is that we need to have PageHeaders for those pages. */

  /* Reserve Multiboot information that we will need later. */
  /* The Multiboot structure itself. */
  physMem_ReserveExact(PtoKPA(MultibootInfoPtr),
                       sizeof(struct grub_multiboot_info));

  physMem_ReservePhysicalMemory();
}
