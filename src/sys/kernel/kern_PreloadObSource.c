/*
 * Copyright (C) 2001, Jonathan S. Shapiro.
 * Copyright (C) 2005, 2006, 2007, Strawberry Development Group.
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
#include <eros/Device.h>
#include <disk/PagePot.h>
#include <disk/DiskNodeStruct.h>
#include <arch-kerninc/Page-inline.h>

#define dbg_fetch	0x1

/* Following should be an OR of some of the above */
#define dbg_flags   ( 0u )

#define DEBUG(x) if (dbg_##x & dbg_flags)

struct FrameInfo {
  uint32_t obFrameNdx;
  uint64_t obFrameNo;
  uint32_t clusterNo;
  uint32_t tagEntry;
};

/* May Yield. */
ObjectHeader *
PreloadObSource_GetObject(ObjectSource * src, OID oid, ObType obType, 
                          ObCount count, bool useCount)
{
  ObjectHeader * pObj;
#if 0
  printf("PreloadObSource_GetObject OID=0x%08x %08x type %d\n",
         (uint32_t)(oid >> 32), (uint32_t)oid, obType );
#endif

  assert(src->start <= oid && oid < src->end);
  OID relOid = oid - src->start;	/* convert to relative terms */

  struct FrameInfo fi;
  fi.obFrameNdx = relOid % EROS_OBJECTS_PER_FRAME;
  fi.obFrameNo = relOid / EROS_OBJECTS_PER_FRAME;
  fi.clusterNo = fi.obFrameNo / DATA_PAGES_PER_PAGE_CLUSTER;
  fi.tagEntry = fi.obFrameNo % DATA_PAGES_PER_PAGE_CLUSTER;

  fi.obFrameNo += fi.clusterNo;
  fi.obFrameNo++;		/* add one for cluster pot in first cluster */

  assert(fi.obFrameNdx < DISK_NODES_PER_PAGE);

  kva_t ppaddr = src->base;
  ppaddr += (fi.clusterNo * PAGES_PER_PAGE_CLUSTER * EROS_PAGE_SIZE);
  PagePot * pp = (PagePot *) ppaddr;
  /* FIXME: I think the PagePot needs to be read and kept in a page frame
  and updated there. */

  kva_t pageBase = src->base;
  pageBase += (fi.obFrameNo * EROS_PAGE_SIZE);

  if (obType == ot_PtDataPage) {
    PageHeader * pageH = objC_GrabPageFrame();
    pObj = pageH_ToObj(pageH);

    void * dest = (void *) pageH_GetPageVAddr(pageH);

    if (pp->type[fi.tagEntry] == FRM_TYPE_DPAGE) {
      memcpy(dest, (void *) pageBase, EROS_PAGE_SIZE);
    } else {	// FRM_TYPE_ZDPAGE, FRM_TYPE_NODE, or FRM_TYPE_ZNODE
      // If the type was NODE or ZNODE, the frame was converted from node
      // to page, and there is no data to load.
      bzero(dest, EROS_PAGE_SIZE);
    }

    // FIXME: pObj->allocCount not set.
    if (useCount && pObj->allocCount != count) {
      ReleasePageFrame(pageH);
      return 0;
    }

    pageH->objAge = age_NewBorn;
    pageH_MDInitDataPage(pageH);
  }
  else {
    assert(obType == ot_NtUnprepared);

    Node * pNode = objC_GrabNodeFrame();
    pObj = node_ToObj(pNode);

    DEBUG (fetch)
      printf("FetchNode OID=0x%08x%08x base=%08x pp=%08x typ=%d\n",
             (uint32_t) (oid >> 32), (uint32_t) oid,
             src->base, ppaddr,
             pp->type[fi.tagEntry] );

    if (pp->type[fi.tagEntry] == FRM_TYPE_NODE) {
      DiskNodeStruct * dn = (DiskNodeStruct *) pageBase;
      dn += fi.obFrameNdx;

      node_SetEqualTo(pNode, dn);
    } else {	// FRM_TYPE_DPAGE, FRM_TYPE_ZDPAGE, or FRM_TYPE_ZNODE
      // If the type was DPAGE or ZDPAGE, the frame was converted from page
      // to node, and there is no data to load.

      pObj->allocCount = pp->count[fi.tagEntry];
      pNode->callCount = pp->count[fi.tagEntry];
      pNode->nodeData = 0;

      uint32_t ndx;
      for (ndx = 0; ndx < EROS_NODE_SIZE; ndx++) {
	assert (keyBits_IsUnprepared(&pNode->slot[ndx]));
	/* not hazarded because newly loaded node */
	keyBits_InitToVoid(&pNode->slot[ndx]);
      }
    }

    if (useCount && pObj->allocCount != count) {
      ReleaseNodeFrame(pNode);
      return 0;
    }

    pNode->objAge = age_NewBorn;
  }

  pObj->oid = oid;

  objH_SetFlags(pObj, OFLG_CURRENT|OFLG_DISKCAPS);
  assert (objH_GetFlags(pObj, OFLG_CKPT|OFLG_DIRTY|OFLG_REDIRTY|OFLG_IO) == 0);

  pObj->ioCount = 0;
  pObj->obType = obType;
#ifdef OPTION_OB_MOD_CHECK
  pObj->check = objH_CalcCheck(pObj);
#endif

  objH_ResetKeyRing(pObj);
  objH_Intern(pObj);

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
