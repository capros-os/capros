/*
 * Copyright (C) 2008, Strawberry Development Group.
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

/* DMA support.
*/

#include <eros/Invoke.h>	// get RC_OK
#include <linuxk/linux-emul.h>
#include <linuxk/lsync.h>
#include <linux/device.h>
#include <linux/dma-mapping.h>
#include <idl/capros/Node.h>
#include <idl/capros/Page.h>
#include <idl/capros/DevPrivs.h>
#include <domain/domdbg.h>
#include <domain/assert.h>

void *
dma_alloc_coherent(struct device *dev,
  size_t size, dma_addr_t *handle, gfp_t gfp)
{
  result_t result;
  unsigned int i;

#if 1
  printk("dma_alloc_coherent(0x%x)\n", size);
#endif

  unsigned int nPages = (size + (EROS_PAGE_SIZE - 1)) >> EROS_PAGE_LGSIZE;

  // Allocate virtual addresses for the memory.
  long blockStart = maps_reserve(nPages);
  if (blockStart < 0)
    return NULL;

  // Allocate physical pages.
  capros_DevPrivs_addr_t physAddr;
  result = capros_DevPrivs_allocateDMAPages(KR_DEVPRIVS, nPages,
             dev->coherent_dma_mask, &physAddr, KR_TEMP0);
  if (result != RC_OK) {
    maps_liberate(blockStart, nPages);
    return NULL;
  }

  // Map first page.
  unsigned long pgOffset = blockStart;
  result = maps_mapPage(pgOffset++, KR_TEMP0);
  assert(result == RC_OK);
  // Map other pages.
  for (i = 1; i < nPages; i++) {
    result = capros_Page_getNthPage(KR_TEMP0, i, KR_TEMP1);
    assert(result == RC_OK);
    result = maps_mapPage(pgOffset++, KR_TEMP1);
    assert(result == RC_OK);
  }

  *handle = physAddr;
  return maps_pgOffsetToAddr(blockStart);
}

void
dma_free_coherent(struct device *dev, size_t size, void *cpu_addr,
                  dma_addr_t handle)
{
  result_t result;

#if 1
  kprintf(KR_OSTREAM, "dma_free_coherent(0x%x, 0x%x)\n", size, cpu_addr);
#endif

  unsigned int nPages = (size + (EROS_PAGE_SIZE - 1)) >> EROS_PAGE_LGSIZE;
  unsigned long pgOffset = maps_addrToPgOffset((unsigned long)cpu_addr);

  maps_getCap(pgOffset, KR_TEMP0);

  result = capros_DevPrivs_deallocateDMAPages(KR_DEVPRIVS, KR_TEMP0);
  assert(result == RC_OK);

  maps_liberate(pgOffset, nPages);
}
