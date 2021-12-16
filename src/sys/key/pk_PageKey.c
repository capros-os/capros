/*
 * Copyright (C) 1998, 1999, Jonathan S. Shapiro.
 * Copyright (C) 2005-2008, 2010, Strawberry Development Group.
 *
 * This file is part of the CapROS Operating System,
 * and is derived from the EROS Operating System.
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
#include <kerninc/LogDirectory.h>
#include <kerninc/Key-inline.h>
#include <eros/Invoke.h>
#include <eros/StdKeyType.h>
#include <idl/capros/Page.h>
#include <idl/capros/Range.h>

/* May Yield. */
void
PageKey(Invocation* inv /*@ not null @*/)
{
  PageHeader * pageH;

  inv_GetReturnee(inv);

  switch(inv->entry.code) {

  case OC_capros_key_getType:
    if (key_ValidateForInv(inv->key))
      return;

    COMMIT_POINT();

    inv->exit.code = RC_OK;
    inv->exit.w1 = IKT_capros_Page;
    break;

  case OC_capros_Page_zero:		/* zero page */
    /* Since we are zeroing the page, no point in fetching it.
     * Don't prepare the key, just validate it. */
    if (key_ValidateForInv(inv->key))
      return;		// key is rescinded

    if (keyBits_IsReadOnly(inv->key)) {
      COMMIT_POINT();
      inv->exit.code = RC_capros_key_NoAccess;
      break;
    }

    if (keyBits_IsPrepared(inv->key)) {
      pageH = objH_ToPage(key_GetObjectPtr(inv->key));

      if (pageH->ioreq) {		// wait for cleaning to complete
        act_SleepOn(&pageH->ioreq->sq);
        act_Yield();
      }

      pageH_EnsureWritable(pageH);

      COMMIT_POINT();

      kva_t pageAddress = pageH_MapCoherentWrite(pageH);
      kzero((void*)pageAddress, EROS_PAGE_SIZE);
      pageH_UnmapCoherentWrite(pageH);
    } else {
      /* The page isn't in memory.
       * Let's zero it without bringing in the old data. */
      if (! OIDIsPersistent(inv->key->u.unprep.oid)) {
        // Non-persistent pages are created as zero by
        // PreloadObSource_GetObject().

        COMMIT_POINT();
      } else {
        // FIXME ensure enough LogDirectory entries?

        COMMIT_POINT();

        ObjectDescriptor objDescr = {
          .oid = inv->key->u.unprep.oid,
          .allocCount = inv->key->u.unprep.count,
          .logLoc = 0,	// it's zero
          .type = capros_Range_otPage
        };
        ld_recordLocation(&objDescr, workingGenerationNumber);
      }
    }

    inv->exit.code = RC_OK;
    break;
    
  case OC_capros_Page_clone:
    // FIXME: we don't really need to fetch the page from disk for this.
    if (key_PrepareForInv(inv->key))
      return;
    pageH = objH_ToPage(key_GetObjectPtr(inv->key));

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

      pageH_EnsureWritable(pageH);

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
    if (key_PrepareForInv(inv->key))
      return;
    pageH = objH_ToPage(key_GetObjectPtr(inv->key));

  {
    unsigned int ordinal = inv->entry.w1;

    if (pageH_GetObType(pageH) != ot_PtDevBlock
        && pageH_GetObType(pageH) != ot_PtDMABlock) {
request_error:
      COMMIT_POINT();

      inv->exit.code = RC_capros_key_RequestError;
      break;
    }

    PmemInfo * pmi = pageH->physMemRegion;
    kpg_t relativePgNum = pageH - pmi->firstObHdr;
    while (ordinal-- > 0) {
      if (++relativePgNum >= pmi->nPages
          || pageH_GetObType(++pageH) != ot_PtSecondary) {
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
    if (key_PrepareForInv(inv->key))
      return;

    MemoryKey(inv);
    return;
  }

  ReturnMessage(inv);
}
