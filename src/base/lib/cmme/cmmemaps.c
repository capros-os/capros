/*
 * Copyright (C) 2008-2010, Strawberry Development Group.
 *
 * This file is part of the CapROS Operating System runtime library.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, 59 Temple Place - Suite 330 Boston, MA 02111-1307, USA.
 */
/* This material is based upon work supported by the US Defense Advanced
Research Projects Agency under Contract No. W31P4Q-07-C-0070.
Approved for public release, distribution unlimited. */

/* Procedures for the maps area of our address space.

We implement a modified buddy system to allocate the address space
at LK_MAPS_BASE.
We allocate only whole pages.

The address of a block of size s is always a multiple of 2**(ceiling(log2(s))).
*/

#include <eros/target.h>
#include <eros/container_of.h>
#include <eros/Invoke.h>	// get RC_OK
#include <string.h>
#include <eros/Link.h>
#include <eros/fls.h>
#include <eros/ffs.h>
#include <idl/capros/Void.h>
#include <idl/capros/Node.h>
#include <idl/capros/Page.h>
#include <idl/capros/GPT.h>
#include <idl/capros/SpaceBank.h>
#include <idl/capros/Process.h>
#include <domain/CMME.h>
#include <domain/assert.h>

#define logMaxBlockSize 7 // in pages

// numpages is the number of pages we could allocate. It can be as large as
// ((LK_DATA_BASE - LK_MAPS_BASE) >> EROS_LGPAGE_SIZE)
// but to save space in mpages[], use a more reasonable maximum:
#define numpages 128

#define dbg_alloc 0x1

/* Following should be an OR of some of the above */
#define dbg_flags   ( 0u )

#define DEBUG(x) if (dbg_##x & dbg_flags)

struct mpage {
  Link listLink;	// valid if isFree
  unsigned short kval;	// valid if isFree
  bool isFree;
} mpages[numpages];

// avail_list[n] is for blocks of size 2**n pages.
Link avail_list[logMaxBlockSize + 1];

static void
liberateOneBlock(unsigned long jAddr, unsigned int j)
{
  DEBUG(alloc) kprintf(KR_OSTREAM, "maps liberating 2**%d at 0x%x\n", j, jAddr);
  struct mpage * jmp = &mpages[jAddr];
  jmp->isFree = true;
  jmp->kval = j;
  link_insertAfter(&avail_list[j], &jmp->listLink);
}

void
maps_liberate_locked(unsigned long pageAddr, unsigned long numPages)
{
  do {
    DEBUG(alloc) kprintf(KR_OSTREAM,
                         "maps liberating %d at 0x%x\n", numPages, pageAddr);
    // Free the smallest block at the end.
    unsigned int j = ffs32(numPages);
    if (j > logMaxBlockSize) {
      j = logMaxBlockSize;
    }
    unsigned long jSize = 1UL << j;
    unsigned long jAddr = pageAddr + numPages - jSize;

    assert(! mpages[jAddr].isFree);

    // Buddy system liberation per Knuth v.1 sec.2.5
recheck:
    if (j < logMaxBlockSize) {
      unsigned long pAddr = jAddr ^ jSize;	// potential buddy
      if (pAddr < numpages) {
        struct mpage * pmp = &mpages[pAddr];
        if (pmp->isFree
            && pmp->kval == j) {
          // Merge with buddy.
          link_UnlinkUnsafe(&pmp->listLink);	// remove from free list
          j++;
          if (pAddr < jAddr)
            jAddr = pAddr;
          goto recheck;
        }
      }
    }
    // No buddy to merge with.
    assert(numPages >= jSize);
    liberateOneBlock(jAddr, j);

    numPages -= jSize;
  } while (numPages);
}

void *
maps_pgOffsetToAddr(unsigned long pageAddr)
{
  return (void *) ((pageAddr << EROS_PAGE_LGSIZE) + LK_MAPS_BASE);
}

unsigned long
maps_addrToPgOffset(unsigned long addr)
{
  return (addr - LK_MAPS_BASE) >> EROS_PAGE_LGSIZE;
}

// Returns page offset within maps area, or -1 if can't allocate.
long
maps_reserve_locked(unsigned long numPages)
{
  DEBUG(alloc) kprintf(KR_OSTREAM, "maps reserving %d\n", numPages);
  unsigned int k = fls32(numPages - 1);
  // 2**(k-1) < numPages <= 2**(k)
  assert(k <= logMaxBlockSize);	// else block is too big

  // Look for a free block.
  unsigned int j;
  for (j = k; j <= logMaxBlockSize; j++)
    if (! link_isSingleton(&avail_list[j]))
      goto foundj;

  return -1;	// too bad
  

foundj: ;
  struct mpage * jmp
    = container_of(avail_list[j].next, struct mpage, listLink);
  assert(jmp->isFree);
  link_UnlinkUnsafe(&jmp->listLink);	// remove from free list
  jmp->isFree = false;
  unsigned long block = jmp - mpages;
  DEBUG(alloc) kprintf(KR_OSTREAM,
                       "maps reserve found 2**%d at 0x%x %#x\n", j, block, jmp);

  unsigned long x = block;
  unsigned long jj = 1UL << j;
  while (jj != numPages) {
    j--;
    jj = 1UL << j;
    if (jj >= numPages) {
      // Liberate upper half, repeat on lower half
      liberateOneBlock(x + jj, j);
    } else {
      // Repeat on upper half of the block.
      numPages -= jj;
      x += jj;
      mpages[x].isFree = false;
    }
  }

  return block;
}

result_t
maps_init(void)
{
  result_t result;
  int i;

  // Create the top-level GPT for maps. 
  result = capros_SpaceBank_alloc1(KR_BANK, capros_Range_otGPT, KR_MAPS_GPT);
  if (result != RC_OK)
    return result;
  result = capros_GPT_setL2v(KR_MAPS_GPT, 17);
  assert(result == RC_OK);
  // Map it.
  result = capros_Process_getAddrSpace(KR_SELF, KR_TEMP0);
  assert(result == RC_OK);
  result = capros_GPT_setSlot(KR_TEMP0, LK_MAPS_BASE / 0x400000, KR_MAPS_GPT);
  assert(result == RC_OK);

  for (i=0; i <= logMaxBlockSize; i++) {
    link_Init(&avail_list[i]);
  }
  // mpages is initially zero.
  // Free all of the address space.
  // We are in initialization, so no need to lock.
  maps_liberate_locked(0, numpages);

  return RC_OK;
}

void
maps_fini(void)
{
  result_t result;
  result = capros_SpaceBank_free1(KR_BANK, KR_MAPS_GPT);
  assert(result == RC_OK);
}

// Uses KR_TEMP0.
// Returns a SpaceBank exception if can't allocate a needed GPT.
result_t
maps_mapPage_locked(unsigned long pgOffset, cap_t pageCap)
{
  result_t result;
  int gpt17slot = pgOffset / capros_GPT_nSlots;
  int gpt12slot = pgOffset % capros_GPT_nSlots;

  DEBUG(alloc) kprintf(KR_OSTREAM, "maps mapPage at 0x%x\n", pgOffset);

  assert(pageCap != KR_TEMP0);

  result = capros_GPT_getSlot(KR_MAPS_GPT, gpt17slot, KR_TEMP0);
  assert(result == RC_OK);

  // Copy one key.
  result = capros_GPT_setSlot(KR_TEMP0, gpt12slot, pageCap);
  if (result == RC_capros_key_Void) {
    // Need to create the l2v == 12 GPT
    // (We never free this GPT, even if all the space in it is unmapped.)
    result = capros_SpaceBank_alloc1(KR_BANK, capros_Range_otGPT, KR_TEMP0);
    if (result != RC_OK)
      return result;
    result = capros_GPT_setL2v(KR_TEMP0, EROS_PAGE_LGSIZE);
    assert(result == RC_OK);
    result = capros_GPT_setSlot(KR_MAPS_GPT, gpt17slot, KR_TEMP0);
    assert(result == RC_OK);

    // Now that the GPT12 is there, store pageCap again.
    result = capros_GPT_setSlot(KR_TEMP0, gpt12slot, pageCap);
    assert(result == RC_OK);
  }
  else
    assert(result == RC_OK);

  return RC_OK;
}

/* rangeCap is a key register containing a capability to a Range.
 * firstPageOfs is the offset relative to that Range
 * of the first page to map.
 * nPages is the number of consecutive pages to map.
 *
 * This procedure reserves address space for the pages and
 * maps them all into the space.
 *
 * It returns:
 *   -1 if we couldn't allocate virtual addresses;
 *   -2 if our SpaceBank can't allocate a needed GPT;
 *   -3 if the specified offset and nPages is not contained in the Range;
 *   -4 if a frame in the range is not of type capros_Range_otPage;
 *   the nonnegative page offset within the maps area, if successful.
 *
 * This procedure uses KR_TEMP0 and KR_TEMP1.
 */
long
maps_reserveAndMapRange_locked(cap_t rangeCap,
  capros_Range_off_t firstPageOfs,
  unsigned int nPages, bool readOnly)
{
  result_t result;
  unsigned int i;

  // Allocate virtual addresses for the memory.
  long blockStart = maps_reserve_locked(nPages);
  if (blockStart < 0)
    return -1;

  unsigned long pgOffset = blockStart;
  for (i = 0; i < nPages; i++, firstPageOfs+=EROS_OBJECTS_PER_FRAME) {
    // Get page cap to map.
    capros_Range_obType oldType;
    result = capros_Range_getCap(rangeCap,
               capros_Range_otPage, firstPageOfs, &oldType, KR_TEMP1);
    if (result != RC_OK) {	// should be RC_capros_Range_RangeErr
      maps_liberate_locked(blockStart, nPages);
      return -3;
    }
    if (oldType != capros_Range_otNone) {	// frame type is wrong
      // (This should not happen with device or DMA pages.)
      maps_liberate_locked(blockStart, nPages);
      return -4;
    }
    // Map the page.
    if (readOnly) {
      result = capros_Memory_reduce(KR_TEMP1,
                 capros_Memory_readOnly, KR_TEMP1);
      assert(result != RC_OK);
    }
    result = maps_mapPage_locked(pgOffset++, KR_TEMP1);
    if (result != RC_OK) {
      maps_liberate_locked(blockStart, nPages);
      return -2;
    }
  }

  return blockStart;
}

/* blockPageCap is a key register containing a capability to a Page
 * that is the first page of a Device page block or a DMA block
 * of nPages pages.
 *
 * This procedure reserves address space for the pages and
 * maps them all into the space.
 *
 * It returns the page offset within the maps area,
 *   or -1 if we couldn't allocate. */
// Uses KR_TEMP0 and KR_TEMP1.
long
maps_reserveAndMapBlock_locked(cap_t blockPageCap, unsigned int nPages)
{
  result_t result;
  unsigned int i;

  // Allocate virtual addresses for the memory.
  long blockStart = maps_reserve_locked(nPages);
  if (blockStart >= 0) {
    // Map first page.
    unsigned long pgOffset = blockStart;
    result = maps_mapPage_locked(pgOffset++, blockPageCap);
    assert(result == RC_OK);
    // Map other pages.
    for (i = 1; i < nPages; i++) {
      result = capros_Page_getNthPage(blockPageCap, i, KR_TEMP1);
      assert(result == RC_OK);
      result = maps_mapPage_locked(pgOffset++, KR_TEMP1);
      assert(result == RC_OK);
    }
  }
  return blockStart;
}

void
maps_getCap(unsigned long pgOffset, cap_t pageCap)
{
  result_t result;
  int gpt17slot = pgOffset / capros_GPT_nSlots;
  int gpt12slot = pgOffset % capros_GPT_nSlots;

  result = capros_GPT_getSlot(KR_MAPS_GPT, gpt17slot, KR_TEMP1);
  assert(result == RC_OK);
  result = capros_GPT_getSlot(KR_TEMP1, gpt12slot, pageCap);
  assert(result == RC_OK);
}
