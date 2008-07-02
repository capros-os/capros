/*
 * Copyright (C) 1998, 1999, Jonathan S. Shapiro.
 * Copyright (C) 2005, 2006, 2007, 2008, Strawberry Development Group.
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

#include <string.h>
#include <kerninc/kernel.h>
#include <kerninc/util.h>
#include <kerninc/Key.h>
#include <kerninc/Activity.h>
#include <kerninc/Invocation.h>
#include <kerninc/ObjectCache.h>
#include <kerninc/PhysMem.h>
#include <kerninc/IORQ.h>
#include <kerninc/Key-inline.h>
#include <eros/Invoke.h>
#include <eros/StdKeyType.h>
#include <idl/capros/Page.h>

/* May Yield. */
void
PageKey(Invocation* inv /*@ not null @*/)
{
  inv_GetReturnee(inv);

  PageHeader * pageH = objH_ToPage(key_GetObjectPtr(inv->key));

  switch(inv->entry.code) {

  case OC_capros_key_getType:
    COMMIT_POINT();

    inv->exit.code = RC_OK;
    inv->exit.w1 = AKT_Page;
    break;

  case OC_capros_Page_zero:		/* zero page */
    if (keyBits_IsReadOnly(inv->key)) {
      COMMIT_POINT();
      inv->exit.code = RC_capros_key_NoAccess;
      break;
    }

    if (pageH->ioreq) {		// wait for cleaning to complete
      act_SleepOn(&pageH->ioreq->sq);
      act_Yield();
    }

    /* Mark the object dirty. */
    pageH_MakeDirty(pageH);

    COMMIT_POINT();

    if (pageH_ToObj(pageH)->oid < FIRST_PERSISTENT_OID) {
      kva_t pageAddress = pageH_MapCoherentWrite(pageH);
      kzero((void*)pageAddress, EROS_PAGE_SIZE);
      pageH_UnmapCoherentWrite(pageH);
    } else {
#if 0
      /* Here we could just zero the page, but that requires minding
       * cache coherence, which may require cache flushes.
       * The following is probably more efficient in some use cases. */
      objH_InvalidateProducts(pageH_ToObj(pageH));
      keyR_UnprepareAll(& pageH_ToObj(pageH)->keyRing);
      ReleaseObjPageFrame(pageH);
      ... save to the log as zero
#else
      kva_t pageAddress = pageH_MapCoherentWrite(pageH);
      kzero((void*)pageAddress, EROS_PAGE_SIZE);
      pageH_UnmapCoherentWrite(pageH);
#endif
    }

    inv->exit.code = RC_OK;
    break;
    
  case OC_capros_Page_clone:
    {
      /* copy content of page key in arg0 to current page */

      if (keyBits_IsReadOnly(inv->key)) {
	COMMIT_POINT();
	inv->exit.code = RC_capros_key_NoAccess;
	break;
      }

      key_Prepare(inv->entry.key[0]);

      /* FIX: This is now wrong: phys pages, time page */
      if (keyBits_IsType(inv->entry.key[0], KKT_Page) == false) {
	COMMIT_POINT();
	inv->exit.code = RC_capros_key_RequestError;
	break;
      }

      /* Mark the object dirty. */
      pageH_MakeDirty(pageH);

      assert(keyBits_IsPrepared(inv->key));

      COMMIT_POINT();

      PageHeader * sourcePage = objH_ToPage(key_GetObjectPtr(inv->entry.key[0]));

      kva_t pageAddress = pageH_MapCoherentWrite(pageH);
      kva_t sourceAddress = pageH_MapCoherentRead(sourcePage);
      memcpy((void *) pageAddress, (void *) sourceAddress, EROS_PAGE_SIZE);
      pageH_UnmapCoherentRead(sourcePage);
      pageH_UnmapCoherentWrite(pageH);
						    
      inv->exit.code = RC_OK;
      break;
    }

  case OC_capros_Page_getNthPage:
  {
    unsigned int ordinal = inv->entry.w1;

    if (pageH_GetObType(pageH) != ot_PtDMABlock) {
request_error:
      COMMIT_POINT();

      inv->exit.code = RC_capros_key_RequestError;
      break;
    }

    PmemInfo * pmi = pageH->physMemRegion;
    kpg_t relativePgNum = pageH - pmi->firstObHdr;
    while (ordinal > 0) {
      if (++relativePgNum >= pmi->nPages
          || pageH_GetObType(++pageH) != ot_PtDMASecondary) {
        goto request_error;
      }
    }

    COMMIT_POINT();

    Key * key = inv->exit.pKey[0];
    if (key) {
      key_NH_Unchain(key);
      key_SetToObj(key, pageH_ToObj(pageH), KKT_Page, inv->key->keyPerms, 0);
    }

    inv->exit.code = RC_OK;
    break;
  }

  default:
    // Handle methods inherited from Memory object.
    MemoryKey(inv);
    return;
  }

  ReturnMessage(inv);
}
