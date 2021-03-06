/*
 * Copyright (C) 2001, Jonathan S. Shapiro.
 * Copyright (C) 2005-2010, Strawberry Development Group.
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

#include <string.h>
#include <idl/capros/Range.h>
#include <kerninc/kernel.h>
#include <kerninc/Activity.h>
#include <kerninc/Machine.h>
#include <kerninc/util.h>
#include <kerninc/ObjectCache.h>
#include <kerninc/ObjectSource.h>
#include <kerninc/Node.h>
#include <kerninc/Ckpt.h>
#include <kerninc/ObjH-inline.h>
#include <disk/DiskNode.h>
#include <disk/NPODescr.h>
#include <arch-kerninc/Page-inline.h>
#include <arch-kerninc/kern-target.h>

#define dbg_fetch	0x1
#define dbg_init	0x2

/* Following should be an OR of some of the above */
#define dbg_flags   ( 0u )

#define DEBUG(x) if (dbg_##x & dbg_flags)

static const ObjectSource PreloadObSource;

void
AssertPageIsFree(PageHeader * pageH)
{
  switch (pageH_GetObType(pageH)) {
  case ot_PtFreeFrame:
  case ot_PtFreeSecondary:	/* This is part of a free block. */
    break;

  default:
    /* If this happens, we need to figure out a way to prevent
    the preloaded data from getting allocated in initialization. */
    assert(!"Preloaded data was overwritten!");
  }
}

static PageHeader *
AllocateThisPhysPage(kpa_t pagePA)
{
  PageHeader * pageH = objC_PhysPageToObHdr(pagePA);

  // If it's part of a free block, split it up:
  while (pageH_GetObType(pageH) != ot_PtFreeFrame
         || pageH->kt_u.free.log2Pages != 0) {
    physMem_SplitContainingFreeBlock(pageH);
  }
  // Unlink from free list:
  link_Unlink(&pageH->kt_u.free.freeLink);
  physMem_numFreePageFrames -= 1;
  pageH_ToObj(pageH)->obType = ot_PtNewAlloc;
  return pageH;
}

void
preload_Init(void)
{
  int i, j, k;

  struct NPObjectsDescriptor * npod;
  kpa_t pagePA = VTOP((kva_t)NPObDescr);
  PageHeader * pageH;

  // Make sure the preloaded data hasn't been inadvertently allocated.

  npod = NPObDescr;
  int numImages = npod->numPreloadImages;
  pageH = objC_PhysPageToObHdr(pagePA);
  for (k = 0; k < numImages; k++) {
    if (k == 1) {	// We are preloading persistent objects
      PersistentIPLOID = npod->IPLOID;
      IsPreloadedBigBang = true;
    }

    /* Un-free the page containing the struct NPObjectsDescriptor,
    so it won't be allocated and clobbered below when we allocate
    zero pages and ranges. */
    AssertPageIsFree(pageH++);	// it must be free
    AllocateThisPhysPage(VTOP((kva_t)npod));

    uint32_t thisFrames = 1 + npod->numFrames;  // including the header frame
    npod = (struct NPObjectsDescriptor *)
           ((char *)npod + thisFrames * EROS_PAGE_SIZE);
    for (i = 1; i < thisFrames; i++) {
      AssertPageIsFree(pageH++);
    }
  }
  // Whew, preloaded data is safe.

  // Load all the preload images.
  npod = NPObDescr;	// start at beginning again
  for (k = npod->numPreloadImages; k > 0; k--) {
    pagePA += EROS_PAGE_SIZE;	// skip frame containing NPObjectsDescriptor
    OID oid = npod->OIDBase;

    if (! OIDIsPersistent(oid)) {
      // Preloaded non-persistent objects.
      // PreloadObSource will supply null objects for uninitialized objects.
      ObjectRange rng;
      rng.start = oid;
      rng.end = oid + FrameToOID(npod->numFramesInRange);
      rng.source = &PreloadObSource;

      objC_AddRange(&rng);
    } else {
      // Preloaded persistent objects.
      // This is one way to initialize a big bang.
      // When a disk range is mounted it will add a source.
    }

    // Preload the initialized nodes.
    j = 0;	// number of nodes loaded
    while (j < npod->numNodes) {	// load node frames
      // Load a frame of nodes.
      /* Note: The last frame may have some nodes beyond npod->numNodes.
       * These are as-yet-unallocated by the space bank,
       * and initialized to null by npgen.
       * We preload them here, so the space bank does not have to remember
       * that they need to be cleared. */
      DiskNode * dn = KPAtoP(DiskNode *, pagePA);
      for (i = 0; i < DISK_NODES_PER_PAGE; i++) {
        Node * pNode = objC_GrabNodeFrame();

        node_SetEqualTo(pNode, dn + i);
        pNode->callCount = restartNPCount;
        objH_InitDirtyObj(node_ToObj(pNode), oid + i, capros_Range_otNode,
                         restartNPCount);
      }
      oid += FrameToOID(1);
      pagePA += EROS_PAGE_SIZE;
      j += DISK_NODES_PER_PAGE;
    }
    // Leave the npod frame and the node frames free.

    // Preload the initialized pages.
    // Just leave them where they are and do the bookkeeping.
    for (i = 0; i < npod->numNonzeroPages; i++) {
      pageH = AllocateThisPhysPage(pagePA);

      objC_GrabThisPageFrame(pageH);
      pageH_MDInitDataPage(pageH);
      pageH_SetReferenced(pageH);
      objH_InitDirtyObj(pageH_ToObj(pageH), oid, capros_Range_otPage,
                       restartNPCount);

      oid += FrameToOID(1);
      pagePA += EROS_PAGE_SIZE;
    }

    printf("Preloaded %d nodes and %d pages at OID %#llx, zp=%d submaps=%d\n",
           npod->numNodes, npod->numNonzeroPages, npod->OIDBase,
           npod->numZeroPages, npod->numSubmaps);

    // Go on to the next preload image:
    uint32_t thisFrames = 1 + npod->numFrames;	// including the header frame
    npod = (struct NPObjectsDescriptor *)
           ((char *)npod + thisFrames * EROS_PAGE_SIZE);
  }

  /* Now that the preloaded data is safe,
   * allocate the zero pages and submaps.
   * This is not necessary for non-persistent preloaded pages, because
   * PreloadObSource_GetObject will create them as zero,
   * but for persistent pages it's needed, because there might be
   * stale data on the disk. */
  npod = NPObDescr;
  for (k = npod->numPreloadImages; k > 0; k--) {
    OID oid = npod->OIDBase + FrameToOID(npod->numFrames);

    for (j = npod->numZeroPages + npod->numSubmaps;
         j > 0; j--) {
      CreateNewNullObject(capros_Range_otPage, oid, restartNPCount);
      oid += FrameToOID(1);
    }

    uint32_t thisFrames = 1 + npod->numFrames;	// including the header frame

    /* Now that we're done with the struct NPObjectsDescriptor,
    free its page. */
    pageH = objC_PhysPageToObHdr(VTOP((kva_t)npod));
    ReleasePageFrame(pageH);

    npod = (struct NPObjectsDescriptor *)
           ((char *)npod + thisFrames * EROS_PAGE_SIZE);
  }
}

/* May Yield. */
static ObjectLocator
PreloadObSource_GetObjectType(ObjectRange * rng, OID oid)
{
  ObjectLocator objLoc;

  assert(rng->start <= oid && oid < rng->end);

  /* All the initialized preloaded objects were set up in the ObCache
  by preload_Init. Therefore this must be an uninitialized object. */

  objLoc.locType = objLoc_Preload;
  objLoc.objType = capros_Range_otNone;
  objLoc.u.preload.range = rng;
  return objLoc;
}

static ObCount
PreloadObSource_GetObjectCount(ObjectRange * rng, OID oid,
  ObjectLocator * pObjLoc, bool callCount)
{
  assert(rng->start <= oid && oid < rng->end);

  return restartNPCount;
}

static ObjectHeader *
PreloadObSource_GetObject(ObjectRange * rng, OID oid,
  const ObjectLocator * pObjLoc)
{
  assert(rng->start <= oid && oid < rng->end);

  return CreateNewNullObject(pObjLoc->objType, oid, restartNPCount);
}

static const ObjectSource PreloadObSource = {
  .name = "preload",
  .objS_GetObjectType = &PreloadObSource_GetObjectType,
  .objS_GetObjectCount = &PreloadObSource_GetObjectCount,
  .objS_GetObject = &PreloadObSource_GetObject
};
