/*
 * Copyright (C) 2001, Jonathan S. Shapiro.
 * Copyright (C) 2005, 2006, 2007, 2008, Strawberry Development Group
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

/* ObjectRange driver for preloaded ram ranges */

#include <idl/capros/Range.h>
#include <kerninc/kernel.h>
#include <kerninc/Activity.h>
#include <kerninc/Machine.h>
#include <kerninc/ObjectSource.h>
#include <kerninc/PhysMem.h>	/* get MI_DEVICEMEM */
#include <arch-kerninc/Page-inline.h>

ObjectLocator 
PhysPageSource_GetObjectType(ObjectRange * rng, OID oid)
{
  ObjectLocator objLoc = {
    .locType = objLoc_Preload,
    .objType = capros_Range_otPage,	// pages only
    .u = {
      .preload = {
         .range = rng
      }
    }
  };
  return objLoc;
}

static ObCount
PhysPageSource_GetObjectCount(ObjectRange * rng, OID oid,
  ObjectLocator * pObjLoc, bool callCount)
{
  assert(rng->start <= oid && oid < rng->end);

  return 0;	// FIXME
}

/* May Yield. */
ObjectHeader * 
PhysPageSource_GetObject(ObjectRange * rng, OID oid,
  const ObjectLocator * pObjLoc)
{
  assert(rng->start <= oid && oid < rng->end);
  assert(pObjLoc->objType == capros_Range_otPage);

  kpa_t pgFrame = (oid - OID_RESERVED_PHYSRANGE) / EROS_OBJECTS_PER_FRAME;
  pgFrame *= EROS_PAGE_SIZE;

  PageHeader * pageH = objC_PhysPageToObHdr(pgFrame);
  if (pageH == 0) {
    dprintf(true, "PhysPageSource::GetObject(): No header!\n");
    return NULL;
  }

  ObjectHeader * pObj = pageH_ToObj(pageH);

  /* Device pages are entered in the object hash by objC_AddDevicePages(),
     and DMA pages are entered by objC_AddDMAPages(),
     so they should never get here: */
  assert(pObj->obType == ot_PtDataPage);

#ifndef NDEBUG
  kpa_t relFrameNdx = ((kpg_t)(pgFrame / EROS_PAGE_SIZE))
                      - rng->u.pmi->firstObPgAddr;
  assert(pageH == &rng->u.pmi->firstObHdr[relFrameNdx]);
#endif

  return pObj;
}

const struct ObjectSource PhysPageObSource = {
  .name = "physpage",
  .objS_GetObjectType = &PhysPageSource_GetObjectType,
  .objS_GetObjectCount = &PhysPageSource_GetObjectCount,
  .objS_GetObject = &PhysPageSource_GetObject
};

void
PhysPageSource_Init(PmemInfo * pmi)
{
  ObjectRange rng;

  rng.source = &PhysPageObSource;
  rng.start = OID_RESERVED_PHYSRANGE
                  + FrameToOID(pmi->firstObPgAddr);
  rng.end   = rng.start + FrameToOID(pmi->nPages);
  rng.u.pmi = pmi;
  objC_AddRange(&rng);
}

void
PhysPageObSource_Init(void)
{
  unsigned i;

  for (i = 0; i < physMem_nPmemInfo; i++) {
    PmemInfo *pmi = &physMem_pmemInfo[i];
    PhysPageSource_Init(pmi);
  }
}
