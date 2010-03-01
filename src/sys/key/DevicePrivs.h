/*
 * Copyright (C) 2008, 2010, Strawberry Development Group.
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
#include <kerninc/IORQ.h>
#include <kerninc/GPT.h>
#include <eros/Invoke.h>
#include <eros/StdKeyType.h>
#include <kerninc/Key-inline.h>

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

    if (OKToGrabPages(nPages, false)) {
      pageH = physMem_AllocateBlock(nPages);
      if (pageH) break;
    }

    // FIXME: If there are a few free frames, try to rearrange them 
    // to be a contiguous block.

    objC_AgePageFrames();
  }

  COMMIT_POINT();

  objC_AddDMAPages(pageH, nPages);	// Set up the pages in this block.

  Key * key = inv->exit.pKey[0];
  if (key) {
    key_NH_Unchain(key);
    key_SetToObj(key, pageH_ToObj(pageH), KKT_Page, 0, 0);
  }

  capros_DevPrivs_addr_t pa = pageH_ToPhysAddr(pageH);
  inv->exit.w1 = pa;
  inv->exit.w2 = pa >> 32;

  inv->exit.code = RC_OK;  /* set the exit code */
}

INLINE void
ValidateDMAPage(PageHeader * pageH)
{
#ifndef NDEBUG
  ObjectHeader * pObj = pageH_ToObj(pageH);

  assert(pObj->oid >= OID_RESERVED_PHYSRANGE);
#endif
}

/* Inline because there is only one use for each architecure. */
INLINE void
physMem_DeallocateDMAPages(Invocation * inv)
{
  Key * key = inv->entry.key[0];
  key_Prepare(key);
  if (! keyBits_IsType(key, KKT_Page)) {
      COMMIT_POINT();

      inv->exit.code = RC_capros_key_RequestError;
      return;
  }

  PageHeader * pageH = objH_ToPage(key_GetObjectPtr(key));
  if (pageH_GetObType(pageH) != ot_PtDMABlock) {
    COMMIT_POINT();

    inv->exit.code = RC_capros_key_RequestError;
    return;
  }

  /* Count the number of pages in the block. */
  PmemInfo * pmi = pageH->physMemRegion;
  kpg_t relativePgNum = pageH - pmi->firstObHdr;
  unsigned int nPages = 0;
  PageHeader * ph = pageH;

  // Validate first page separately,
  // because its ObType is ot_PtDMABlock, not ot_PtSecondary
  assert(pageH_GetObType(ph) == ot_PtDMABlock);
  ValidateDMAPage(ph);

  for (; ++ph, ++nPages, ++relativePgNum < pmi->nPages; ) {
    if (pageH_GetObType(ph) != ot_PtSecondary)
      break;
    ValidateDMAPage(ph);
  }

  COMMIT_POINT();

  // Rescind keys to all pages.
  unsigned int i;
  for (ph = pageH, i = 0; i < nPages; ph++, i++) {
    ObjectHeader * pObj = pageH_ToObj(ph);
    objH_Rescind(pObj);
    objH_InvalidateProducts(pObj);
    objH_Unintern(pObj);	// prepare to free the block
  }

  physMem_FreeBlock(pageH, nPages);

  inv->exit.code = RC_OK;
}

INLINE void
devPrivs_allocateIORQ(Invocation * inv)
{
  COMMIT_POINT();
  IORQ * iorq = IORQ_Allocate();
  if (iorq) {
    Key * key = inv->exit.pKey[0];
    if (key) {
      key_NH_Unchain(key);
      keyBits_InitType(key, KKT_IORQ);
      key->u.nk.value[0] = iorq - IORQs;
    }
    inv->exit.code = RC_OK;
  } else {
    inv->exit.code = RC_capros_DevPrivs_AllocFail;
  }
}

INLINE void
devPrivs_DeclarePFHProcess(Invocation * inv)
{
  Key * key = inv->entry.key[0];
  key_Prepare(key);
  if (! keyBits_IsType(key, KKT_Process)) {
    COMMIT_POINT();

    inv->exit.code = RC_capros_key_RequestError;
    return;
  }

  Process * proc = key->u.gk.pContext;

  Node * pNode = proc->procRoot;
  if (OIDIsPersistent(node_ToObj(pNode)->oid)) {
    // Only non-persistent can be a PFH.
    COMMIT_POINT();

    inv->exit.code = RC_capros_key_RequestError;
    return;
  }

  // Go through the process's address space, 
  // allocating and pinning all addressable mapping tables.
  proc_LockAllMapTabs(proc);

  COMMIT_POINT();

  proc->kernelFlags |= KF_PFH;

  inv->exit.code = RC_OK;
}

/* Inline because there is only one use for each architecure. */
INLINE void
devPrivs_publishMem(Invocation * inv, kpa_t base, kpa_t bound)
{
printf("*****publishMem base=%#llx bound=%#llx\n", base, bound);////
  if ((base % EROS_PAGE_SIZE)
      || (bound % EROS_PAGE_SIZE)
      || (base >= bound) ) {
    inv->exit.code = RC_capros_key_RequestError;
    return;
  }

  OID oidCount = ((bound - base) >> EROS_PAGE_LGSIZE)
                      * EROS_OBJECTS_PER_FRAME;
  if (oidCount > UINT32_MAX) {
    // the range is too large to be represented in the count of a range key
    inv->exit.code = RC_capros_key_RequestError;
    return;
  }

  PmemInfo * pmi;
  // Ensure pages are set up, one fragment at a time.
  kpa_t checked;
  for (checked = base; checked < bound; ) {
    kpa_t fragBound;
    if (physMem_CheckOverlap(base, bound, &pmi)) {
      // base is not already set up; we need to do it.
      if (pmi)
        fragBound = pmi->base;
      else
        fragBound = bound;

      int ret = physMem_AddRegion(base, fragBound, MI_DEVICEMEM, &pmi);
      if (ret) {
        inv->exit.code = RC_capros_DevPrivs_AllocFail;
        return;
      }

      PageHeader * pageH = objC_AddDevicePages(pmi);
      if (!pageH) {
        // FIXME remove the region added above
        inv->exit.code = RC_capros_DevPrivs_AllocFail;
        return;
      }
      PhysPageSource_Init(pmi);
    } else {
      // base is already set up by pmi.
    }
    checked = pmi->bound;
  }

  Key * key = inv->exit.pKey[0];
  if (key) {
    key_NH_Unchain(key);
    assert(! keyBits_IsHazard(key));
    // Return a Range cap for the pages.
    keyBits_InitType(key, KKT_Range);
    key->keyPerms = 0;
    key->keyData = 0;
    key->u.rk.oid = OID_RESERVED_PHYSRANGE
                    + (base >> EROS_PAGE_LGSIZE) * EROS_OBJECTS_PER_FRAME;
    key->u.rk.count = oidCount;
  }

  inv->exit.code = RC_OK;
}
