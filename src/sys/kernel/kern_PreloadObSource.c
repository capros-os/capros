/*
 * Copyright (C) 2001, Jonathan S. Shapiro.
 * Copyright (C) 2005, Strawberry Development Group.
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

/* The current implementation isn't quite kosher, as the current
 * implementation is built to assume a preloaded ramdisk, and the
 * ramdisk boot sector is inherently machine dependent. At some point
 * in the (near) future, I am going to implement the range preload
 * flag, and the need for this hack will go away.
 */
/*#include <disk/LowVolume.hxx>*/

struct FrameInfo {
  uint32_t obFrameNdx;
  uint64_t obFrameNo;
  uint32_t clusterNo;
  uint32_t tagEntry;

  /*FrameInfo(OID oid);*/
};

typedef struct FrameInfo FrameInfo;

static inline void InitFrameInfo(FrameInfo *thisPtr, OID oid)
{
  thisPtr->obFrameNdx = oid % EROS_OBJECTS_PER_FRAME;
  thisPtr->obFrameNo = oid / EROS_OBJECTS_PER_FRAME;
  thisPtr->clusterNo = thisPtr->obFrameNo / DATA_PAGES_PER_PAGE_CLUSTER;
  thisPtr->tagEntry = thisPtr->obFrameNo % DATA_PAGES_PER_PAGE_CLUSTER;

  thisPtr->obFrameNo += thisPtr->clusterNo;
  thisPtr->obFrameNo++;		/* add one for cluster pot in first cluster */

}

static bool
FetchPage(ObjectSource *src, ObjectHeader *pObj)
{
  OID oid = pObj->kt_u.ob.oid;
  FrameInfo fi;
  void *dest = 0;
  PagePot *pp = 0;
  OID relOid;
  kva_t ppaddr;

  assert(src->start <= oid && oid < src->end);

  dest = (uint8_t *) objC_ObHdrToPage(pObj);

  relOid = oid - src->start;		/* convert to relative terms */

  InitFrameInfo(&fi, relOid);

  assert(fi.obFrameNdx < DISK_NODES_PER_PAGE);

  ppaddr = src->base;
  ppaddr += (fi.clusterNo * PAGES_PER_PAGE_CLUSTER * EROS_PAGE_SIZE);

  pp = (PagePot *) ppaddr;

  if (pp->type[fi.tagEntry] == FRM_TYPE_NODE ||
      pp->type[fi.tagEntry] == FRM_TYPE_ZNODE) {

    /* Retag the existing frame to be a page frame. Do not bump the
     * allocation count when converting TO pages. 
     *
     * Note an assumption here: when WriteObject(obj) is called it will
     * preserve the invariant that the count field in the page pot
     * will be max(pp->count[x], obj->ob.allocCount, obj->callCount).
     */
    pp->type[fi.tagEntry] = FRM_TYPE_ZDPAGE;
  }

  if (pp->type[fi.tagEntry] == FRM_TYPE_DPAGE) {
    kva_t pageBase = src->base;
    pageBase += (fi.obFrameNo * EROS_PAGE_SIZE);

    memcpy(dest, (void *) pageBase, EROS_PAGE_SIZE);
    return true;
  }
  else if (pp->type[fi.tagEntry] == FRM_TYPE_ZDPAGE) {
    bzero(dest, EROS_PAGE_SIZE);
    return true;
  }

  pObj->kt_u.ob.allocCount = pp->count[fi.tagEntry];

  return false;
}

static bool
FetchNode(ObjectSource *src, ObjectHeader *pObj)
{
  OID oid = pObj->kt_u.ob.oid;
  OID relOid;
  FrameInfo fi;
  PagePot * pp = 0;
  kva_t ppaddr;
  unsigned i = 0;
  DiskNodeStruct *dn = 0;
  OID frameOid;
  uint32_t ndx = 0;
  kva_t pageBase;

  assert(src->start <= oid && oid < src->end);

  relOid = oid - src->start;			/* convert to relative terms */

  InitFrameInfo(&fi, relOid);

  assert(fi.obFrameNdx < DISK_NODES_PER_PAGE);

  ppaddr = src->base;
  ppaddr += (fi.clusterNo * PAGES_PER_PAGE_CLUSTER * EROS_PAGE_SIZE);

  pp = (PagePot *) ppaddr;

  if (pp->type[fi.tagEntry] == FRM_TYPE_DPAGE ||
      pp->type[fi.tagEntry] == FRM_TYPE_ZDPAGE) {

    /* Retag the existing frame to be a node frame. When converting
     * from pages to something else, we MUST bump the allocation
     * count.
     *
     * Note an assumption here: when WriteObject(obj) is called it will
     * preserve the invariant that the count field in the page pot
     * will be max(pp->count[x], obj->ob.allocCount, obj->callCount).
     */
    pp->type[fi.tagEntry] = FRM_TYPE_ZNODE;
  }

  /* The first time we actually touch a ZNODE frame, convert it to a
   * proper node frame.
   */
  if (pp->type[fi.tagEntry] == FRM_TYPE_ZNODE) {
    pageBase = src->base;
    pageBase += (fi.obFrameNo * EROS_PAGE_SIZE);

    dn = (DiskNodeStruct *) pageBase;
    frameOid = EROS_FRAME_FROM_OID(oid);

    for (i = 0; i < DISK_NODES_PER_PAGE; i++) {
      DiskNodeStruct *pNode = dn + i;

      pNode->allocCount = pp->count[fi.tagEntry];
      pNode->callCount = pp->count[fi.tagEntry];
      pNode->oid = frameOid + i;

      for (ndx = 0; ndx < EROS_NODE_SIZE; ndx++) {
	assert (keyBits_IsUnprepared(&pNode->slot[ndx]));
	/* not hazarded because newly loaded node */
	keyBits_InitToVoid(&pNode->slot[ndx]);
      }
    }

    pp->type[fi.tagEntry] = FRM_TYPE_NODE;
  }

  assert(pp->type[fi.tagEntry] == FRM_TYPE_NODE);

  pageBase = src->base;
  pageBase += (fi.obFrameNo * EROS_PAGE_SIZE);

  dn = (DiskNodeStruct *) pageBase;
  dn += fi.obFrameNdx;

  node_SetEqualTo((Node *)pObj, dn);

  return true;
}

ObjectHeader *
PreloadObSource_GetObject(ObjectSource *thisPtr, OID oid, ObType obType, 
                          ObCount count, bool useCount)
{
  ObjectHeader * pObj;
  bool result;

  if (obType == ot_PtDataPage) {
    pObj = objC_GrabPageFrame();
    pObj->kt_u.ob.oid = oid;

    result = FetchPage(thisPtr, pObj);
  }
  else {
    assert(obType == ot_NtUnprepared);

    pObj = DOWNCAST(objC_GrabNodeFrame(), ObjectHeader);
    pObj->kt_u.ob.oid = oid;

    result = FetchNode(thisPtr, pObj);
  }

  if (!result || (useCount && pObj->kt_u.ob.allocCount != count)) {
    objC_ReleaseFrame(pObj);
    return 0;
  }

  assert (pObj->kt_u.ob.oid == oid);

  pObj->age = age_NewBorn;

  objH_SetFlags(pObj, OFLG_CURRENT|OFLG_DISKCAPS);
  assert (objH_GetFlags(pObj, OFLG_CKPT|OFLG_DIRTY|OFLG_REDIRTY|OFLG_IO) == 0);

  pObj->kt_u.ob.ioCount = 0;
  pObj->obType = obType;
#ifdef OPTION_OB_MOD_CHECK
  pObj->kt_u.ob.check = objH_CalcCheck(pObj);
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
