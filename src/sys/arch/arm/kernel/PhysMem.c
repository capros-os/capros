/*
 * Copyright (C) 1998, 1999, Jonathan S. Shapiro.
 * Copyright (C) 2006-2010, Strawberry Development Group
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
Research Projects Agency under Contract Nos. W31P4Q-06-C-0040 and
W31P4Q-07-C-0070.  Approved for public release, distribution unlimited. */

#include <kerninc/kernel.h>
#include <kerninc/util.h>
#include <kerninc/PhysMem.h>
#include <kerninc/multiboot.h>

#define dbg_init	0x1

/* Following should be an OR of some of the above */
#define dbg_flags   ( 0x0 )

#define DEBUG(x) if (dbg_##x & dbg_flags)

kpg_t physMem_TotalPhysicalPages = 0;	// approximate
unsigned int logDataCacheLineLength;
unsigned int logDataCacheAssociativity;	// rounded up to an integer
unsigned int logDataCacheNSets;
uint32_t cacheSetIndexIncrement;
uint32_t cacheSetIndexCarry;

void
physMem_Init_MD()
{
  int32_t mmapLength;
  struct grub_mmap * mp;

uint32_t mach_ReadCacheType(void);
  uint32_t cacheType = mach_ReadCacheType();
  assert((cacheType & 0x01000000));	// unified cache not supported
  assert((cacheType >> 25) >= 2);	// We only support write-back,
			// cleaning with register 7 operations.
  logDataCacheLineLength = ((cacheType >> 12) & 0x3) + 3;
  logDataCacheAssociativity = ((cacheType >> 15) & 0x7);
  logDataCacheNSets = ((cacheType >> 18) & 0x7) + 9
    - logDataCacheLineLength - logDataCacheAssociativity;
  if (cacheType & 0x4000)	// M bit is one, associativity is 50% higher
    logDataCacheAssociativity++;	// round log up

  // Calculate constants for the cleaning code in mach_DoCacheWork.
  cacheSetIndexCarry = (1ul << (32-logDataCacheAssociativity))
    - (1ul << (logDataCacheLineLength + logDataCacheNSets));
  cacheSetIndexIncrement = (1ul << logDataCacheLineLength)
    + cacheSetIndexCarry;

  DEBUG (init) printf("CacheType %x, lll=%d, lassoc=%d, lnsets=%d\n",
         cacheType, logDataCacheLineLength, logDataCacheAssociativity,
         logDataCacheNSets);
  DEBUG (init) printf("SetIndex incr=%x, carry=%x\n",
         cacheSetIndexIncrement, cacheSetIndexCarry);

  DEBUG (init) printf("MultibootInfoPtr = %x\n", MultibootInfoPtr);

  uint32_t totalRAM = 0;

  for (mmapLength = MultibootInfoPtr->mmap_length,
         mp = (struct grub_mmap *) MultibootInfoPtr->mmap_addr;
       mmapLength > 0;
       ) {
    DEBUG (init) printf("0x%08lx 0x%08lx 0x%08lx 0x%08lx 0x%08lx 0x%08lx 0x%08lx\n",
       mp,
       mp->size,
       mp->base_addr_low,
       mp->base_addr_high,
       mp->length_low,
       mp->length_high,
       mp->type);

    /* This machine has only 32 bits of phys addr. */
    assert(mp->base_addr_high == 0 && mp->length_high == 0);
    kpa_t base = (kpa_t)mp->base_addr_low;
    kpsize_t size = (kpsize_t)mp->length_low;
    kpsize_t bound = base + size;

    int ret;
    PmemInfo * pmi;	// value isn't used
    switch (mp->type) {
    case 1:		// RAM
    {
      uint32_t regionSize = bound - base;
      uint32_t limited = KTUNE_MaxRAMToUse - totalRAM;
      if (limited > regionSize)
        limited = regionSize;	// take min
      totalRAM += regionSize;
      if (limited) {
        ret = physMem_AddRegion(base, base + limited, MI_MEMORY, false, &pmi);
        assert(!ret);
        kpg_t pgs = limited / EROS_PAGE_SIZE;
        physMem_TotalPhysicalPages += pgs;
      }
      break;
    }

    case 4567:	// this is a private convention, not part of multiboot
      ret = physMem_AddRegion(base, bound, MI_DEVICEMEM, false, &pmi);
      assert(!ret);
      break;
    }

    /* On to the next. */
    /* mp->size does not include the size of the size field itself,
       so add 4. */
    mmapLength -= (mp->size + 4);
    mp = (struct grub_mmap *) (((char *)mp) + (mp->size + 4));
  }

  printf("Total RAM = %#x", totalRAM);
  if (KTUNE_MaxRAMToUse < totalRAM)
    printf(", using only %#x", KTUNE_MaxRAMToUse);
  printf("\n");

  /* Preloaded modules are contained in mmap memory,
     so no need to add regions for them. */

  /* Reserve regions of physical memory that are already used. */

  /* Reserve kernel code and rodata. */
  physMem_ReserveExact(KTextPA, (&_etext - &_start));
  kpa_t cursor = KTextPA + (&_etext - &_start);

  /* Reserve kernel data and bss.
     The kernel stack is within the data section. */
  cursor = align_up(cursor, 0x100000); /* 1MB boundary */
  physMem_ReserveExact(cursor, (&_end - &__data_start));
  cursor += (&_end - &__data_start);

  /* Reserve kernel mapping tables. */
  cursor = align_up(cursor, 0x4000);	/* 16KB boundary */
  physMem_ReserveExact(cursor, 0x4000 + 0x4000 + 0x1000);

  /* Multiboot information is in data/bss, no need to reserve. */
 
  /* At the moment, we don't do anything to reserve the memory
  containing the initialized preloaded non-persistent objects.
  We simply hope that nothing clobbers those pages before we use them.
  The reason is that we need to have PageHeaders for those pages. */
}
