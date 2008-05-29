/*
 * Copyright (C) 2001, Jonathan S. Shapiro.
 * Copyright (C) 2005, 2006, 2007, 2008, Strawberry Development Group
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

/* May Yield. */
ObjectHeader *
PhysPageSource_GetObject(ObjectRange * rng, OID oid, ObType obType, 
		       ObCount count, bool useCount)
{
  kpa_t pgFrame = (oid - OID_RESERVED_PHYSRANGE) / EROS_OBJECTS_PER_FRAME;
  
  pgFrame *= EROS_PAGE_SIZE;

  PageHeader * pObj = objC_PhysPageToObHdr(pgFrame);

  if (pObj == 0) {
    dprintf(true, "PhysPageSource::GetObject(): No header!\n");
    return 0;
  }

  /* Device pages are entered in the object hash by objC_AddDevicePages(),
     and DMA pages are entered by objC_AddDMAPages(),
     so they should never get here: */
  assert(pageH_ToObj(pObj)->obType == ot_PtDataPage);

#ifndef NDEBUG
  kpa_t relFrameNdx = ((kpg_t)(pgFrame / EROS_PAGE_SIZE))
                      - rng->pmi->firstObPgAddr;
  assert(pObj == &rng->pmi->firstObHdr[relFrameNdx]);
#endif

#if 0	// revisit this if it turns out we need it
  // FIXME: Where do we check if the page is pinned?
  if (! objC_EvictFrame(pObj))
    return 0;	// could not evict

  pObj->objAge = age_NewBorn;
  pageH_MDInitDataPage(pObj);

  objH_InitObj(pageH_ToObj(pObj), oid, PhysPageAllocCount, ?, ot_PtDataPage);
#endif

  return pageH_ToObj(pObj);
}

bool
PhysPageSource_WriteBack(ObjectRange * rng, ObjectHeader *obHdr, bool b)
{
  fatal("PhysPageSource::WriteBack() unimplemented\n");

  return false;
}

const struct ObjectSource PhysPageObSource = {
  .name = "physpage",
  .objS_GetObject = PhysPageSource_GetObject,
  .objS_WriteBack = PhysPageSource_WriteBack,
};

void
PhysPageSource_Init(PmemInfo * pmi)
{
  ObjectRange rng;

  rng.source = &PhysPageObSource;
  rng.start = OID_RESERVED_PHYSRANGE
                  + FrameToOID(pmi->firstObPgAddr);
  rng.end   = rng.start + FrameToOID(pmi->nPages);
  rng.pmi = pmi;
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
