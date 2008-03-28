/*
 * Copyright (C) 2001, Jonathan S. Shapiro.
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

/* ObjectRange driver for preloaded ram ranges */

#include <string.h>
#include <kerninc/kernel.h>
#include <kerninc/Activity.h>
#include <kerninc/Machine.h>
#include <kerninc/util.h>
#include <kerninc/ObjectCache.h>
#include <kerninc/ObjectSource.h>
#include <kerninc/Node.h>
#include <eros/Device.h>
#include <disk/PagePot.h>
#include <disk/DiskNodeStruct.h>
#include <disk/NPODescr.h>
#include <arch-kerninc/Page-inline.h>
#include <arch-kerninc/kern-target.h>

#define dbg_fetch	0x1
#define dbg_init	0x2

/* Following should be an OR of some of the above */
#define dbg_flags   ( 0u )

#define DEBUG(x) if (dbg_##x & dbg_flags)

void
AssertPageIsFree(PageHeader * pageH)
{
  switch (pageH_GetObType(pageH)) {
  case ot_PtFreeFrame:
  case ot_PtSecondary:	/* This is part of a free block. */
    break;

  default:
    /* If this happens, we need to figure out a way to prevent
    the preloaded data from getting allocated in initialization. */
    assert(!"Preloaded data was overwritten!");
  }
}

void
preload_Init(void)
{
  int i;

  kpa_t pagePA = VTOP((kva_t)NPObDescr);
  // Make sure the preloaded data hasn't been inadvertently allocated.
  PageHeader * pageH = objC_PhysPageToObHdr(pagePA);
  AssertPageIsFree(pageH);

  struct NPObjectsDescriptor * npod = NPObDescr;	// local copy

  pageH++;	// now the first frame of node data
  pagePA += EROS_PAGE_SIZE;

  for (i = 0; i < npod->numFrames; i++) {
    AssertPageIsFree(pageH + i);
  }
  // Whew, preloaded data is safe.

  OID oid = npod->OIDBase;

  // Preload the initialized nodes.
  oid += FrameToOID(npod->numSubMapFrames);

  DEBUG (init)
    printf("Preloading %d nodes and %d pages at OID %#llx\n",
           npod->numNodes, npod->numNonzeroPages, oid);

  int j = 0;	// number of nodes loaded
  while (j < npod->numNodes) {	// load node frames
    // Load a frame of nodes.
    DiskNodeStruct * dn = KPAtoP(DiskNodeStruct *, pagePA);
    for (i = 0; i < DISK_NODES_PER_PAGE; i++) {
      Node * pNode = objC_GrabNodeFrame();

      node_SetEqualTo(pNode, dn + i);
      pNode->objAge = age_NewBorn;
      objH_InitObj(node_ToObj(pNode), oid + i, 0, ot_NtUnprepared);

      if (++j >= npod->numNodes)
        break;
    }
    oid += FrameToOID(1);
    pagePA += EROS_PAGE_SIZE;
  }
  // Leave the npod frame and the node frames free.

  // Preload the initialized pages.
  // Just leave them where they are and do the bookkeeping.
  for (i = 0; i < npod->numNonzeroPages; i++) {
    PageHeader * pageH = objC_PhysPageToObHdr(pagePA);

    // If it's part of a free block, split it up:
    while (pageH_GetObType(pageH) != ot_PtFreeFrame
           || pageH->kt_u.free.log2Pages != 0) {
      physMem_SplitContainingFreeBlock(pageH);
    }
    // Unlink from free list:
    link_Unlink(&pageH->kt_u.free.freeLink);
    pageH_ToObj(pageH)->obType = ot_PtNewAlloc;

    objC_GrabThisPageFrame(pageH);
    pageH_MDInitDataPage(pageH);
    pageH->objAge = age_NewBorn;
    objH_InitObj(pageH_ToObj(pageH), oid, 0, ot_PtDataPage);

    oid += FrameToOID(1);
    pagePA += EROS_PAGE_SIZE;
  }
}

/* May Yield. */
ObjectHeader *
PreloadObSource_GetObject(ObjectSource * src, OID oid, ObType obType, 
                          ObCount count, bool useCount)
{
  ObjectHeader * pObj;
#if 0
  printf("PreloadObSource_GetObject OID=%#llx type %d\n", oid, obType);
#endif

  assert(src->start <= oid && oid < src->end);

  /* All the initialized preloaded objects were set up in the ObCache
  by preload_Init. Therefore this must be an uninitialized object. */

  if (obType == ot_PtDataPage) {
    PageHeader * pageH = objC_GrabPageFrame();
    pObj = pageH_ToObj(pageH);

    void * dest = (void *) pageH_GetPageVAddr(pageH);
    kzero(dest, EROS_PAGE_SIZE);

    // FIXME: pObj->allocCount not set.
    if (useCount && pObj->allocCount != count) {
      ReleasePageFrame(pageH);
      return 0;
    }

    pageH_MDInitDataPage(pageH);
    pageH->objAge = age_NewBorn;
  }
  else {
    assert(obType == ot_NtUnprepared);

    Node * pNode = objC_GrabNodeFrame();
    pObj = node_ToObj(pNode);

    // FIXME: set count right.
    pObj->allocCount = 0;
    pNode->callCount = 0;
    pNode->nodeData = 0;

    uint32_t ndx;
    for (ndx = 0; ndx < EROS_NODE_SIZE; ndx++) {
      assert (keyBits_IsUnprepared(&pNode->slot[ndx]));
      /* not hazarded because newly loaded node */
      keyBits_InitToVoid(&pNode->slot[ndx]);
    }

    if (useCount && pObj->allocCount != count) {
      ReleaseNodeFrame(pNode);
      return 0;
    }

    pNode->objAge = age_NewBorn;
  }

  objH_InitObj(pObj, oid, 0, obType);

  return pObj;
}

bool
PreloadObSource_Invalidate(ObjectSource *thisPtr, ObjectHeader *obHdr)
{
  fatal("PreloadObSource::Evict() unimplemented\n");

  return false;
}

bool
PreloadObSource_Detach(ObjectSource *thisPtr)
{
  fatal("PreloadObSource::Detach() unimplemented\n");

  return false;
}

bool
PreloadObSource_WriteBack(ObjectSource *thisPtr, ObjectHeader *obHdr, bool b)
{
  fatal("PreloadObSource::Write() unimplemented\n");

  return false;
}
