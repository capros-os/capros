/*
 * Copyright (C) 2007, 2008, Strawberry Development Group.
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
#include <asm-generic/semaphore.h>

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
__kmalloc(size_t size, gfp_t flags)
{
  void * p = NULL;

  if (flags == GFP_KERNEL
      || flags == GFP_NOIO) {
    down(&mallocLock);
    p = malloc(size);	// allocate in heap
    up(&mallocLock);
  }
  else {
    kprintf(KR_OSTREAM, "__kmalloc flags 0x%x\n", flags);
    assert(((void)"unimplemented malloc pool", false));
  }
  return p;
}

void *
__kzalloc(size_t size, gfp_t flags)
{
  void * p = __kmalloc(size, flags);
  if (p)
    memset(p, 0, size);
  return p;
}
