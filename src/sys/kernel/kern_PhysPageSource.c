/*
 * Copyright (C) 2001, Jonathan S. Shapiro.
 * Copyright (C) 2005, 2006, Strawberry Development Group
 *
 * This file is part of the EROS Operating System.
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

/* ObjectRange driver for preloaded ram ranges */

#include <kerninc/kernel.h>
#include <kerninc/Activity.h>
#include <kerninc/Machine.h>
#include <kerninc/ObjectSource.h>
#include <eros/Device.h>
#include <kerninc/PhysMem.h>	/* get MI_DEVICEMEM */

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

ObjectHeader *
PhysPageSource_GetObject(ObjectSource *thisPtr, OID oid, ObType obType, 
		       ObCount count, bool useCount)
{
  ObjectHeader *pObj = 0;
#ifndef NDEBUG
  kpa_t relFrameNdx;
#endif
  kpa_t pgFrame = (oid - OID_RESERVED_PHYSRANGE) / EROS_OBJECTS_PER_FRAME;
  
  pgFrame *= EROS_PAGE_SIZE;

  if (!ValidPhysPage(thisPtr->pmi, pgFrame)) {
    dprintf(true, "OID 0x%08x%08x invalid\n",
		    (unsigned long) (oid >> 32),
		    (unsigned long) (oid));
    return 0;
  }


  pObj = objC_PhysPageToObHdr(pgFrame);

  if (pObj == 0) {
    dprintf(true, "PhysPageSource::GetObject(): No header!\n");
    return pObj;
  }

#ifndef NDEBUG
  relFrameNdx = (pgFrame - thisPtr->pmi->basepa) / EROS_PAGE_SIZE;
  assert(pObj == &thisPtr->pmi->firstObHdr[relFrameNdx]);
#endif

  if (! objC_EvictFrame(pObj))
    return 0;	// could not evict

  pObj->kt_u.ob.oid = oid;
  pObj->kt_u.ob.allocCount = PhysPageAllocCount;
  pObj->age = age_NewBorn;

  objH_SetFlags(pObj, OFLG_CURRENT|OFLG_DISKCAPS);
  assert (objH_GetFlags(pObj, OFLG_CKPT|OFLG_DIRTY|OFLG_REDIRTY|OFLG_IO) == 0);
 
  pObj->kt_u.ob.ioCount = 0;
  if (thisPtr->pmi->type == MI_DEVICEMEM) {
    pObj->obType = ot_PtDevicePage;

    /* Do not bother with calculating the checksum value, as device
     * memory is always considered dirty. */
    objH_SetFlags(pObj, OFLG_DIRTY);
  }
  else {
    pObj->obType = ot_PtDataPage;
#ifdef OPTION_OB_MOD_CHECK
    pObj->kt_u.ob.check = objH_CalcCheck(pObj);
#endif
  }

  objH_ResetKeyRing(pObj);
  objH_Intern(pObj);

  return pObj;
}

bool
PhysPageSource_Invalidate(ObjectSource *thisPtr, ObjectHeader *pObj)
{
  if (pObj->obType == ot_PtDevicePage) {
    fatal("PhysPageSource::Invalidate(PtDevicePage) is nonsense\n");
  }
  else {
    kpa_t pgFrame;

    assert(pObj->obType == ot_PtDataPage);

    pgFrame = pObj->pageAddr;

    if (!ValidPhysPage(thisPtr->pmi, pgFrame))
      return false;

  /* Okay, now this is a real nuisance. Free the frame and bump the
   * global physical frame allocation count. */
  
    assert(keyR_IsEmpty(&pObj->keyRing));
    assert(pObj->prep_u.products == 0);
    

    objH_Unintern(pObj);
    objH_ClearFlags(pObj, OFLG_DIRTY);


  /* FIX: What about transaction lock? */

    objC_ReleaseFrame(pObj);

    PhysPageAllocCount ++;

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
