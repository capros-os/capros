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

// malloc is protected by:
DECLARE_MUTEX(mallocLock);

void
kfree(const void * p)
{
  down(&mallocLock);
  free((void *)p);
  up(&mallocLock);
}

void *
__kmalloc_node(size_t size, gfp_t flags, int node)
{
  void * p = NULL;

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
    down(&mallocLock);
    p = malloc(size);	// allocate in heap
    up(&mallocLock);
    if ((flags & __GFP_ZERO) && p)
      memset(p, 0, size);
  }
  else {
    kprintf(KR_OSTREAM, "__kmalloc flags 0x%x\n", flags);
    assert(((void)"unimplemented malloc pool", false));
  }
  return p;
}
