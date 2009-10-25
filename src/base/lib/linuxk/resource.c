/*
 * Copyright (C) 2007, Strawberry Development Group.
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

#include <linuxk/linux-emul.h>
#include <linux/ioport.h>

/* I/O resources (ports and memory) are reserved by allocating keys
at big bang time. 
Therefore __request_region() doesn't need to keep track of reservations.
These Linux procedures therefore always say that the resource is available. */

struct resource iomem_resource;	// contents are not used
struct resource ioport_resource;	// contents are not used

struct resource *
__request_region(struct resource * parent,
                 resource_size_t start,
                 resource_size_t n, const char *name, int flags)
{
  /* We want to return a non-NULL value to indicate success.
  We return a pointer to code, which at least is read-only. */
  return (struct resource *)&__request_region;
}

void
__release_region(struct resource * res, resource_size_t start,
                 resource_size_t n)
{
}
