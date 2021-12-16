/*
 * Copyright (C) 2007, 2008, 2009, Strawberry Development Group.
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

#include <stdlib.h>
#include <string.h>

#include <domain/assert.h>

#include <linuxk/linux-emul.h>
#include <linux/slab.h>
#include <linux/semaphore.h>

//#define KMALLOC_DEBUG
#define ALIGN_BYTES 4

// malloc is protected by:
DECLARE_MUTEX(mallocLock);

void
kfree(const void * p)
{
  if (p) {
    char * pToFree = (char *)p;
#ifdef KMALLOC_DEBUG
    pToFree -= ALIGN_BYTES;
    uint32_t size = *((uint32_t *)pToFree);
    assert(size != 0xbadbadac);
    assert(size < 0x00100000);	// is it reasonable?
    *((uint32_t *)pToFree) = 0xbadbadac;	// mark as unallocated
    memset(pToFree, 0, size);	// better not need the data now
#endif
    down(&mallocLock);
    free((void *)pToFree);
    up(&mallocLock);
  }
}

void *
__kmalloc_node(size_t size, gfp_t flags, int node)
{
#ifdef CONFIG_SPINLOCK_USES_IRQ
#error
/* We must handle GFP_ATOMIC without the possibility of getting
preempted. That means we must not use the semaphore mallocLock
and must not use the VCSK. */
#endif

  // Sorry, but since we are using malloc, we have to ignore SLAB_HWCACHE_ALIGN:
  flags &= ~SLAB_HWCACHE_ALIGN;

  gfp_t flagsNotZero = flags & ~__GFP_ZERO;
  if (flagsNotZero == GFP_KERNEL
      || flagsNotZero == GFP_ATOMIC
      || flagsNotZero == GFP_NOIO
      || flagsNotZero == 0) {
    void * p = NULL;
    size_t sizeToAlloc = size;
#ifdef KMALLOC_DEBUG
    sizeToAlloc += ALIGN_BYTES;
#endif

    down(&mallocLock);
    p = malloc(sizeToAlloc);	// allocate in heap
    up(&mallocLock);
    if (p) {
      char * pToReturn = p;
#ifdef KMALLOC_DEBUG
      pToReturn += ALIGN_BYTES;
      *((uint32_t *)p) = size;	// save size and mark as allocated
      memset(pToReturn, 0, size);	// no one should be using the data
#else
      if (flags & __GFP_ZERO)
        memset(pToReturn, 0, size);
#endif
      return pToReturn;
    }
  }
  else {
    kprintf(KR_OSTREAM, "__kmalloc flags 0x%x\n", flags);
    assert(((void)"unimplemented malloc pool", false));
  }
  return NULL;
}
