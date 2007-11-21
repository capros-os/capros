/*
 * Copyright (C) 2001, Jonathan S. Shapiro.
 * Copyright (C) 2005, 2006, 2007, Strawberry Development Group
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

/* ObjectRange driver for preloaded ram ranges */

#include <kerninc/kernel.h>
#include <kerninc/Activity.h>
#include <kerninc/Machine.h>
#include <kerninc/ObjectSource.h>
#include <eros/Device.h>
#include <kerninc/PhysMem.h>	/* get MI_DEVICEMEM */
#include <arch-kerninc/Page-inline.h>

/* We simulate physical page allocation counters using a single
 * universal allocation counter that is bumped for each new
 * allocation. Since the physical page ObjectHeader is pinned, the
 * only time this will get checked is when an old key is prepared
 * following revocation of the underlying physical page.
 *
 * Note that there is a serious potential security hole here if a
 * physical page key is EVER stored into a persistent node, because
 * the user might well get lucky on the allocation count.
 */
static ObCount PhysPageAllocCount = 0u;

/* The current implementation isn't quite kosher, as the current
 * implementation is built to assume a preloaded ramdisk, and the
 * ramdisk boot sector is inherently machine dependent. At some point
 * in the (near) future, I am going to implement the range preload
 * flag, and the need for this hack will go away.
 */
/*#include <disk/LowVolume.hxx>*/

/* It might appear that this is wrong, because we do not know at the
 * time this constructor runs how many allocatable physical pages
 * there will be. Actually, we can't ever really know how many
 * physically allocatable pages are in a given region, because the
 * kernel may yet do dynamic memory allocations for itself (e.g. to
 * allocate core table entries when it is informed of physical memory
 * on cards.
 *
 * The good news, such as it is, is that we actually can know the
 * right answer when the time comes to (de)allocate the actual pages.
 */

static inline bool
ValidPhysPage(PmemInfo *pmi, kpa_t pgFrame)
{
  kpa_t relFrameNdx;

  if (pgFrame < pmi->basepa)
    return false;

  relFrameNdx = (pgFrame - pmi->basepa) / EROS_PAGE_SIZE;

  if (relFrameNdx >= pmi->nPages)
    return false;

  return true;
}

/* May Yield. */
ObjectHeader *
PhysPageSource_GetObject(ObjectSource *thisPtr, OID oid, ObType obType, 
		       ObCount count, bool useCount)
{
  kpa_t pgFrame = (oid - OID_RESERVED_PHYSRANGE) / EROS_OBJECTS_PER_FRAME;
  
  pgFrame *= EROS_PAGE_SIZE;

  if (!ValidPhysPage(thisPtr->pmi, pgFrame)) {
    dprintf(true, "OID 0x%08x%08x invalid\n",
		    (unsigned long) (oid >> 32),
		    (unsigned long) (oid));
    return 0;
  }

  PageHeader * pObj = objC_PhysPageToObHdr(pgFrame);

  if (pObj == 0) {
    dprintf(true, "PhysPageSource::GetObject(): No header!\n");
    return 0;
  }

  /* Device pages are entered in the object hash by objC_AddDevicePages(),
     so they should never get here: */
  assert(pageH_ToObj(pObj)->obType != ot_PtDevicePage);

#ifndef NDEBUG
  kpa_t relFrameNdx = (pgFrame - thisPtr->pmi->basepa) / EROS_PAGE_SIZE;
  assert(pObj == &thisPtr->pmi->firstObHdr[relFrameNdx]);
#endif

  // FIXME: Where do we check if the page is pinned?
  if (! objC_EvictFrame(pObj))
    return 0;	// could not evict

  pageH_ToObj(pObj)->oid = oid;
  pageH_ToObj(pObj)->allocCount = PhysPageAllocCount;

  pageH_ToObj(pObj)->ioCount = 0;	// should this be an assertion?

  assert(objH_GetFlags(pageH_ToObj(pObj),
                       OFLG_CKPT|OFLG_DIRTY|OFLG_REDIRTY|OFLG_IO) == 0);
  objH_SetFlags(pageH_ToObj(pObj), OFLG_CURRENT);

  pObj->objAge = age_NewBorn;
  pageH_ToObj(pObj)->obType = ot_PtDataPage;
#ifdef OPTION_OB_MOD_CHECK
  pageH_ToObj(pObj)->check = objH_CalcCheck(pageH_ToObj(pObj));
#endif

  pageH_MDInitDataPage(pObj);

  objH_ResetKeyRing(pageH_ToObj(pObj));
  objH_Intern(pageH_ToObj(pObj));

  return pageH_ToObj(pObj);
}

bool
PhysPageSource_Invalidate(ObjectSource *thisPtr, ObjectHeader *pObj)
{
  if (pObj->obType == ot_PtDevicePage) {
    fatal("PhysPageSource::Invalidate(PtDevicePage) is nonsense\n");
  }
  else {
    assert(pObj->obType == ot_PtDataPage);

    kpa_t pgFrame = pageH_GetPageVAddr(objH_ToPage(pObj));

    if (!ValidPhysPage(thisPtr->pmi, pgFrame))
      return false;

  /* Okay, now this is a real nuisance. Free the frame and bump the
   * global physical frame allocation count. */
  
    assert(keyR_IsEmpty(&pObj->keyRing));
    assert(pObj->prep_u.products == 0);
    
    objH_ClearFlags(pObj, OFLG_DIRTY);

  /* FIX: What about transaction lock? */

    ReleaseObjPageFrame(objH_ToPage(pObj));

    PhysPageAllocCount ++;
    // FIXME: Does this inadvertently invalidate other pages,
    // including device pages?

    fatal("PhysPageSource::Invalidate() unimplemented\n");
  }

  return false;
}

bool
PhysPageSource_Detach(ObjectSource *thisPtr)
{
  fatal("PhysPageSource::Detach() unimplemented\n");

  return false;
}

bool
PhysPageSource_WriteBack(ObjectSource *thisPtr, ObjectHeader *obHdr, bool b)
{
  fatal("PhysPageSource::WriteBack() unimplemented\n");

  return false;
}

void
PhysPageSource_Init(ObjectSource * source, PmemInfo * pmi)
{
  source->name = "physpage";
  source->start = OID_RESERVED_PHYSRANGE
                  + ((pmi->base  / EROS_PAGE_SIZE) * EROS_OBJECTS_PER_FRAME);
  source->end   = OID_RESERVED_PHYSRANGE
                  + ((pmi->bound / EROS_PAGE_SIZE) * EROS_OBJECTS_PER_FRAME);
  source->pmi = pmi;
  source->objS_Detach = PhysPageSource_Detach;
  source->objS_GetObject = PhysPageSource_GetObject;
  source->objS_IsRemovable = ObjectSource_IsRemovable;
  source->objS_WriteBack = PhysPageSource_WriteBack;
  source->objS_Invalidate = PhysPageSource_Invalidate;
  source->objS_FindFirstSubrange = ObjectSource_FindFirstSubrange;
  objC_AddSource(source);
}
