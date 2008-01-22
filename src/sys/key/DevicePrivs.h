/*
 * Copyright (C) 2008, Strawberry Development Group.
 *
 * This file is part of the CapROS Operating System.
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
#include <kerninc/Key.h>
#include <kerninc/Activity.h>
#include <kerninc/Invocation.h>
#include <kerninc/PhysMem.h>
#include <kerninc/ObjectSource.h>
#include <kerninc/ObjectCache.h>
#include <kerninc/IRQ.h>
#include <eros/Invoke.h>
#include <eros/StdKeyType.h>

#include <idl/capros/key.h>
#include <idl/capros/DevPrivs.h>

/* DevicePrivs procedures common to more than one architecture. */

/* Inline because there is only one use for each architecure. */
INLINE void
physMem_AllocateDMAPages(Invocation * inv)
{
  unsigned int nPages = inv->entry.w1;
  // Note, CapIDL has a bug, doesn't handle 64-bit quantities
  capros_DevPrivs_addr_t mask = inv->entry.w2 + 
    ((capros_DevPrivs_addr_t)inv->entry.w3 << 32);

  if (nPages > (1U << capros_DevPrivs_logMaxDMABlockSize)) {
      COMMIT_POINT();

      inv->exit.code = RC_capros_key_RequestError;
      return;
  }

  PageHeader * pageH;
  while (1) {
    (void)mask;	// FIXME: mask is currently not used.
    pageH = physMem_AllocateBlock(nPages);
    if (pageH) break;

    // FIXME: If there are a few free frames, try to rearrange them 
    // to be a contiguous block.

    // WaitForAvailablePageFrame
    objC_AgePageFrames();
  }

  COMMIT_POINT();

  objC_AddDMAPages(pageH, nPages);	// Set up the pages in this block.

  Key * key = inv->exit.pKey[0];
  if (key) {
    key_NH_SetToObj(key, pageH_ToObj(pageH), KKT_Page);
  }

  capros_DevPrivs_addr_t pa
    = (kpa_t)pageH_ToPhysPgNum(pageH)<< EROS_PAGE_LGSIZE;
  inv->exit.w1 = pa;
  inv->exit.w2 = pa >> 32;

  inv->exit.code = RC_OK;  /* set the exit code */
}
