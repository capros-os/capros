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

    /* FIXME: Don't wait for pages to be cleaned, since it could be
    the page cleaner that needs the space!
    Sorry, using Linux drivers, we have no guarantee that the space needed
    will be available. */

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

INLINE void
ValidateDMAPage(PageHeader * pageH)
{
  ObjectHeader * pObj = pageH_ToObj(pageH);

  assert(objH_GetFlags(pObj, OFLG_IO | OFLG_CKPT) == 0);
  assert(objH_GetFlags(pObj, OFLG_DISKCAPS) == 0);	/* because
		no allocation count */
  assert(pObj->oid >= OID_RESERVED_PHYSRANGE);
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
  // because its ObType is ot_PtDMABlock, not ot_PtDMASecondary
  assert(pageH_GetObType(ph) == ot_PtDMABlock);
  ValidateDMAPage(ph);

  for (; ++ph, ++nPages, ++relativePgNum < pmi->nPages; ) {
    if (pageH_GetObType(ph) != ot_PtDMASecondary)
      break;
    ValidateDMAPage(ph);
  }

  COMMIT_POINT();

  // Rescind keys to all pages.
  unsigned int i;
  for (ph = pageH, i = 0; i < nPages; ph++, i++) {
    ObjectHeader * pObj = pageH_ToObj(ph);
    keyR_RescindAll(&pObj->keyRing, false /* not hasCaps */);
    objH_InvalidateProducts(pObj);
    objH_Unintern(pObj);	// prepare to free the block
  }

  physMem_FreeBlock(pageH, nPages);
  sq_WakeAll(&PageAvailableQueue, false);

  inv->exit.code = RC_OK;
}
