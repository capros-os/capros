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
#include <kerninc/Check.h>
#include <kerninc/ObjectCache.h>
#include <kerninc/Node.h>
#include <kerninc/Depend.h>
#include <kerninc/Activity.h>
#include <kerninc/Invocation.h>
#include <kerninc/ObjectSource.h>
#include <kerninc/multiboot.h>
#include <kerninc/util.h>
#include <kerninc/IORQ.h>
#include <kerninc/LogDirectory.h>
#include <kerninc/ObjH-inline.h>
#include <arch-kerninc/KernTune.h>
#include <arch-kerninc/Page-inline.h>
#include <kerninc/PhysMem.h>
#include <disk/NPODescr.h>
#include <disk/DiskNode.h>
#include <arch-kerninc/PTE.h>
#include <eros/machine/IORQ.h>

#define dbg_cachealloc	0x1	/* initialization */
#define dbg_ckpt	0x2	/* migration state machine */
#define dbg_aging	0x4
#define dbg_ndalloc	0x8	/* node allocation */
#define dbg_pgalloc	0x10	/* page allocation */

/* Following should be an OR of some of the above */
#define dbg_flags   ( 0u | dbg_aging)////

#define DEBUG(x) if (dbg_##x & dbg_flags)

uint32_t objC_nNodes;
uint32_t objC_nFreeNodeFrames;
Node *objC_nodeTable;
Node *objC_firstFreeNode;

uint32_t objC_nPages;
PageHeader * objC_coreTable;

Node *
objC_ContainingNode(void * vp)
{
  char * bp = (char *) vp;
  char * nt = (char *) objC_nodeTable;
  int nchars = bp - nt;
  Node * pNode = &objC_nodeTable[nchars/sizeof(Node)];
  assert(objC_ValidNodePtr(pNode));
  return pNode;
}

static void
objC_AllocateUserPages(void)
{
  /* When we get here, we are allocating the last of the core
   * memory. take it all.
   */
  unsigned rgn;
  kpsize_t np;
    
  uint32_t trialNPages = physMem_AvailPages(&physMem_pages);

  /* The coreTable itself will use up some of the available space,
     so factor that in: */
  trialNPages = (trialNPages * EROS_PAGE_SIZE)
                / (EROS_PAGE_SIZE + sizeof(PageHeader));
  trialNPages += 1;	// just in case

  objC_coreTable = KPAtoP(PageHeader *,
             physMem_Alloc(trialNPages*sizeof(PageHeader), &physMem_any));
  assert(objC_coreTable);

  DEBUG(cachealloc)
    printf("Allocated %#x PageHeaders at %#x\n", trialNPages, objC_coreTable);

  /* On the way through this loop, objC_nPages holds the total number
   * of pages in all previous allocations until the very end.
   */
  objC_nPages = 0;

  for (rgn = 0; rgn < physMem_nPmemInfo; rgn++) {
    PmemInfo *pmi= &physMem_pmemInfo[rgn];
    PmemConstraint xmc;

    if (pmi->type != MI_MEMORY)
      continue;

    xmc.base = pmi->base;
    xmc.bound = pmi->bound;
    xmc.align = EROS_PAGE_SIZE;

    np = PmemInfo_ContiguousPages(pmi);

    assert(np == physMem_AvailPages(&xmc));

#ifdef TESTING_AGEING
    np = min (np, 50);
#endif

    pmi->nPages = np;
    pmi->firstObPgAddr
      = physMem_Alloc(EROS_PAGE_SIZE * np, &xmc) >> EROS_PAGE_LGSIZE;
    pmi->firstObHdr = &objC_coreTable[objC_nPages];

    objC_nPages += np;
  }

  DEBUG(pgalloc)
    printf("Page Headers: allocated %d at 0x%08x, used %d\n",
		   trialNPages, objC_coreTable, objC_nPages);

  // Check that the space we allocated was big enough. 
  assert(objC_nPages <= trialNPages);

#if 0
  printf("nPages = %d (0x%x)\n", nPages, nPages);
#endif

  /* Initialize all the core table entries: */

  PageHeader * pObHdr = objC_coreTable;
  for (rgn = 0; rgn < physMem_nPmemInfo; rgn++) {
    PmemInfo * pmi= &physMem_pmemInfo[rgn];
    if (pmi->type != MI_MEMORY)
      continue;

    uint32_t pg;
    for (pg = 0; pg < pmi->nPages; pg++) {
      pObHdr->physMemRegion = pmi;
      pObHdr->kt_u.free.obType = ot_PtSecondary;
      pObHdr++;
    }

    physMem_FreeAll(pmi);	// Free all the pages in this region.
  }
  assert(pObHdr == objC_coreTable + objC_nPages);
}

static void
AddCoherentPages(PageHeader * pageH, PmemInfo * pmi, kpg_t nPages,
  OID oid, unsigned int obType)
{
  unsigned int pg;

  kzero(pageH, sizeof(PageHeader) * nPages);

  for (pg = 0;
       pg < nPages;
       pg++, pageH++, oid += EROS_OBJECTS_PER_FRAME) {
    pageH->physMemRegion = pmi;
    pageH_MDInitDevicePage(pageH);	// make it coherent

    ObjectHeader * pObj = pageH_ToObj(pageH);
    pObj->obType = obType;
    pObj->oid = oid;
    pObj->allocCount = 0;	// FIXME or PhysPageAllocCount??
    objH_SetFlags(pObj, OFLG_CURRENT | OFLG_DIRTY);
    // No objH_CalcCheck for device pages.
    objH_ResetKeyRing(pObj);
    objH_Intern(pObj);
    objH_SetFlags(pObj, OFLG_DIRTY);
    objH_ClearFlags(pObj, OFLG_Cleanable);
  }
}

void
objC_AddDevicePages(PmemInfo * pmi)
{
  /* Not all BIOS's report a page-aligned start address for everything,
  so we might actually be shifting out nonzero bits here. */
  pmi->firstObPgAddr = pmi->base >> EROS_PAGE_LGSIZE;

  kpg_t nPages = (kpg_t)(pmi->bound / EROS_PAGE_SIZE) - pmi->firstObPgAddr;

  PageHeader * pageH = MALLOC(PageHeader, nPages);

  pmi->nPages = nPages;
  pmi->firstObHdr = pageH;

  OID frameOid = pmi->firstObPgAddr * EROS_OBJECTS_PER_FRAME
                        + OID_RESERVED_PHYSRANGE;
  AddCoherentPages(pageH, pmi, nPages, frameOid, ot_PtDevicePage);
}

void
objC_AddDMAPages(PageHeader * pageH, kpg_t nPages)
{
  assert(pageH->kt_u.free.obType == ot_PtNewAlloc);

  PmemInfo * pmi = pageH->physMemRegion;

  OID oid = pageH_ToPhysPgNum(pageH) * EROS_OBJECTS_PER_FRAME
            + OID_RESERVED_PHYSRANGE;

  AddCoherentPages(pageH, pmi, nPages, oid, ot_PtDMASecondary);

  // First frame has its own type:
  pageH_ToObj(pageH)->obType = ot_PtDMABlock;
}

PageHeader *
objC_GetCorePageFrame(uint32_t ndx)
{
  unsigned rgn = 0;

  for (rgn = 0; rgn < physMem_nPmemInfo; rgn++) {
    PmemInfo *pmi= &physMem_pmemInfo[rgn];
    if (pmi->type != MI_MEMORY)
      continue;

    if (ndx < pmi->nPages)
      return &pmi->firstObHdr[ndx];
    ndx -= pmi->nPages;
  }

  return 0;
}

Node *
objC_GetCoreNodeFrame(uint32_t ndx)
{
  return &objC_nodeTable[ndx];
}

#ifndef NDEBUG
bool
objC_ValidPagePtr(const ObjectHeader * pObj)
{
  uint32_t wobj = (uint32_t) pObj;
  unsigned rgn = 0;
  uint32_t wbase;
  uint32_t top;
  uint32_t delta;
  
  for (rgn = 0; rgn < physMem_nPmemInfo; rgn++) {
    PmemInfo *pmi= &physMem_pmemInfo[rgn];
    if (pmi->type != MI_MEMORY && pmi->type != MI_DEVICEMEM && pmi->type != MI_BOOTROM)
      continue;

    wbase = (uint32_t) pmi->firstObHdr;
    top = (uint32_t) (pmi->firstObHdr + pmi->nPages);

    if (wobj < wbase)
      continue;
    if (wobj >= top)
      continue;

    delta = wobj - wbase;
    if (delta % sizeof(PageHeader))
      return false;
    return true;
  }

  return false;
}
#endif

#ifndef NDEBUG
bool
objC_ValidNodePtr(const Node *pObj)
{
  uint32_t wobj = (uint32_t) pObj;
  uint32_t wbase = (uint32_t) objC_nodeTable;
  uint32_t wtop = (uint32_t) (objC_nodeTable + objC_nNodes);
  uint32_t delta;

  if (wobj < wbase)
    return false;
  if (wobj >= wtop)
    return false;

  delta = wobj - wbase;
  if (delta % sizeof(Node))
    return false;

  return true;
}
#endif

#ifndef NDEBUG
bool
objC_ValidKeyPtr(const Key *pKey)
{
  uint32_t wobj = (uint32_t) pKey;
  uint32_t wbase = (uint32_t) objC_nodeTable;
  uint32_t wtop = (uint32_t) (objC_nodeTable + objC_nNodes);
  uint32_t delta;
  uint32_t i = 0;
  Node *pNode = 0;

  if (inv_IsInvocationKey(&inv, pKey))
    return true;

  if ( proc_ValidKeyReg(pKey) )
    return true;

  if ( act_ValidActivityKey(0, pKey) ) /* first parameter is unused in act_ValidActivityKey() */
    return true;

  if (wobj < wbase)
    return false;
  if (wobj >= wtop)
    return false;

  /* It's in the right range to be a pointer into a node.  See if it's
   * at a valid slot:
   */
  
  delta = wobj - wbase;

  delta /= sizeof(Node);

  pNode = objC_nodeTable + delta;

  for (i = 0; i < EROS_NODE_SIZE; i++) {
    if ( node_GetKeyAtSlot(pNode, i) == pKey )
      return true;
  }

  return false;
}
#endif

/* Put a page on the free list. */
/* Caller must have already removed all previous entanglements. */
void
ReleasePageFrame(PageHeader * pageH)
{
  physMem_FreeBlock(pageH, 1);
}

/* Remove this node from the cache and return it to the free node list: */
void
ReleaseNodeFrame(Node * pNode)
{
  DEBUG(ndalloc)
    printf("ReleaseNodeFrame node=%#x\n", pNode);

  ObjectHeader * pObj = node_ToObj(pNode);

  assert(!objH_IsDirty(pObj));

  objH_InvalidateProducts(pObj);
  keyR_UnprepareAll(&pObj->keyRing);

#ifndef NDEBUG
  uint32_t i = 0;
  for (i = 0; i < EROS_NODE_SIZE; i++)
    assertex (pNode, keyBits_IsUnprepared(&pNode->slot[i]));
#endif

  objH_Unintern(pObj);

  pObj->obType = ot_NtFreeFrame;

  pObj->prep_u.nextFree = node_ToObj(objC_firstFreeNode);
  objC_firstFreeNode = pNode;
  objC_nFreeNodeFrames++;
}

/* Release a page that has an ObjectHeader. */
void
ReleaseObjPageFrame(PageHeader * pageH)
{
  DEBUG(pgalloc)
    printf("ReleaseObjPageFrame pageH=0x%08x\n", pageH);

  assert(!pageH_IsDirty(pageH));
  assert(pte_ObIsNotWritable(pageH));

  objH_Unintern(pageH_ToObj(pageH));
    
  ReleasePageFrame(pageH);
}

void
objH_InitObj(ObjectHeader * pObj, OID oid)
{
  pObj->oid = oid;

  objH_SetFlags(pObj, OFLG_CURRENT);
  assert(objH_GetFlags(pObj, OFLG_CKPT|OFLG_DIRTY|OFLG_IO) == 0);

#ifdef OPTION_OB_MOD_CHECK
  pObj->check = objH_CalcCheck(pObj);
#endif

  objH_ResetKeyRing(pObj);
  objH_Intern(pObj);
}

void
objH_InitDirtyObj(ObjectHeader * pObj, OID oid, unsigned int baseType,
  ObCount allocCount)
{
  pObj->allocCount = allocCount;
  pObj->obType = BaseTypeToObType(baseType);
  objH_InitObj(pObj, oid);
  // Object is dirty because we just initialized it:
  objH_SetFlags(pObj, OFLG_DIRTY);
  if (oid < FIRST_PERSISTENT_OID) {
    // not persistent, not cleanable
    objH_ClearFlags(pObj, OFLG_Cleanable);
  } else {
    objH_SetFlags(pObj, OFLG_Cleanable);
  }
}

void
objC_GrabThisPageFrame(PageHeader *pObj)
{
  assert(pageH_GetObType(pObj) == ot_PtNewAlloc);

  PmemInfo * pmi = pObj->physMemRegion;	// preserve this field
  kzero(pObj, sizeof(*pObj));
  pObj->physMemRegion = pmi;

  assert(pte_ObIsNotWritable(pObj));

  pageH_SetReferenced(pObj);
}

/* The first numNodesToClean entries of nodesToClean are candidates
 * for cleaning. They may have been referenced since being added. */
Node * nodesToClean[DISK_NODES_PER_PAGE];
unsigned int numNodesToClean = 0;

/* Returns true iff freed some nodes. */
// May Yield
bool
CleanAPotOfNodes(bool force)
{
  unsigned int i;

  if (numNodesToClean < DISK_NODES_PER_PAGE
      && ! force)
    return false;	// wait till we get more nodes

  for (i = 0; i < numNodesToClean; i++) {
    Node * pNode = nodesToClean[i];
    if (node_ToObj(pNode)->objAge < age_Steal) {
      // It was referenced after it was added.
      // It's no longer a candidate to clean.
      // Remove it.
      nodesToClean[i--] = nodesToClean[--numNodesToClean];
      // numNodesToClean now < DISK_NODES_PER_PAGE.
      if (! force)
        return false;
    }
  }

  IORequest * ioreq = AllocateIOReqCleaningAndPage();	// may Yield
  PageHeader * pageH = ioreq->pageH;
  ObjectHeader * pObj;

  // Now we are committed to creating a log pot.

  LID lid = NextLogLoc();
  ObjectRange * rng = LidToRange(lid);
  assert(rng);	// it had better be mounted

  DiskNode * dn = (DiskNode *)pageH_GetPageVAddr(pageH);
  for (i = 0; i < numNodesToClean; i++, dn++) {
    Node * pNode = nodesToClean[i];
    pObj = node_ToObj(pNode);
    node_CopyToDiskNode(pNode, dn);
    ObjectDescriptor objDescr = {
      .oid = pObj->oid,
      .allocCount = pObj->allocCount,
      .callCount = pNode->callCount,
      .logLoc = lid + i,
      .type = capros_Range_otNode
    };
    ld_recordLocation(&objDescr, workingGenerationNumber);
    //// Don't release if (force)?
    ReleaseNodeFrame(pNode);
  }

  // Write the pot.
  pObj = pageH_ToObj(pageH);
  pObj->obType = ot_PtLogPot;
  objH_InitObj(pObj, lid);

  ioreq->requestCode = capros_IOReqQ_RequestType_writeRangeLoc;
  ioreq->objRange = rng;
  ioreq->rangeLoc = OIDToFrame(lid - rng->start);
  ioreq->doneFn = &IOReq_EndWrite;
  sq_Init(&ioreq->sq);
  ioreq_Enqueue(ioreq);
  return true;
}

/* May Yield. */
static void
objC_AgeNodeFrames(void)
{
  static uint32_t curNode = 0;
  uint32_t nStuck = 0;
  uint32_t nPinned = 0;
  int pass;

  DEBUG(aging) printf("objC_AgeNodeFrames called.\n");

#ifdef DBG_WILD_PTR
  if (dbg_wild_ptr)
    check_Consistency("Before AgeNodeFrames()");
#endif

  /* It may be that we already have a potful of nodes to be cleaned, 
   * but they didn't get cleaned because we were unable to get a page frame
   * or IORequest. Therefore check again here. */ 
  if (CleanAPotOfNodes(false))
    goto bumpAndReturn;	// freed some nodes

  for (pass = 0; pass <= age_Steal; pass++) {
    nPinned = 0;
    nStuck = 0;
    uint32_t count;
    for (count = 0; count < objC_nNodes; count++) {
      Node * pNode = objC_GetCoreNodeFrame(curNode);
      ObjectHeader * pObj = node_ToObj(pNode);
    
      if (pObj->obType == ot_NtFreeFrame)
	return;
    
      assert(pObj->objAge <= age_Steal);
    
      if (objH_IsUserPinned(pObj)
          || node_IsKernelPinned(pNode) ) {
	nPinned++;
	nStuck++;
	goto nextNode;
      }

      /* Since the object isn't pinned, set its transaction ID to zero
      so it won't inadvertently be considered pinned
      when objH_CurrentTransaction overflows. */
      pObj->userPin = 0;

      if (! objH_GetFlags(pObj, OFLG_Cleanable)) {
	nStuck++;
	goto nextNode;
      }

      /* DO NOT AGE OUT CONSTITUENTS OF AN ACTIVE CONTEXT
       * 
       * This is an issue because when we access a process
       * we mark the process pinned and referenced, but not the
       * constituent nodes.
       *
       * This logic moves the design in the direction of making
       * processes first-class objects. Processes have their own
       * pins and their own aging mechanism.
       * The constituent nodes just happen to be in node space
       * instead of in process space.
       */
    
      if (pObj->obType == ot_NtProcessRoot ||
	  pObj->obType == ot_NtRegAnnex ||
	  pObj->obType == ot_NtKeyRegs) {
	  nStuck++;
          goto nextNode;
      }
      
      /* THIS ALGORITHM IS NOT THE SAME AS THE PAGE AGEING ALGORITHM!!!
       * 
       * Nodes are promptly cleanable (just write them to a log
       * pot), so we don't clean them until the age_Steal stage.
       */

      switch (pObj->objAge) {
      case age_Invalidate:
        keyR_TrackReferenced(&pObj->keyRing);
      default:
	pObj->objAge++;
        break;

      case age_Steal:
    
        DEBUG(ckpt)
	  dprintf(false, "Ageing out node=0x%08x oty=%d dirty=%c oid=%#llx\n",
			pNode, pObj->obType,
			(objH_IsDirty(pObj) ? 'y' : 'n'), pObj->oid);

        if (objH_IsDirty(pObj)) {
          if (node_IsNull(pNode)) {
            ObjectDescriptor objDescr = {
              .oid = pObj->oid,
              .allocCount = pObj->allocCount,
              .callCount = pNode->callCount,
              .logLoc = 0,	// it's null
              .type = capros_Range_otNode
            };
            ld_recordLocation(&objDescr, workingGenerationNumber);
            objH_ClearFlags(pObj, OFLG_DIRTY);	// cleaned it to the log dir
            goto stealNode;
          } else {
            /* Add this dirty node to the list of candidates for cleaning. */
            assert(numNodesToClean < DISK_NODES_PER_PAGE);
            nodesToClean[numNodesToClean++] = pNode;
            if (CleanAPotOfNodes(false))
              goto bumpAndReturn;	// freed some nodes
          }
        } else {	// node is clean
        stealNode:	// Steal this node.
          ReleaseNodeFrame(pNode);
          goto bumpAndReturn;
        }
      }

    nextNode:
      if (++curNode >= objC_nNodes)
	curNode = 0;
    }
  }

  fatal("%d stuck nodes of which %d are pinned\n",
		nStuck, nPinned);

bumpAndReturn:
  // We freed some nodes.

#if defined(TESTING_AGEING) && 0
  dprintf(true, "AgeNodeFrame(): Object evicted\n");
#endif
#ifdef DBG_WILD_PTR
  if (dbg_wild_ptr)
    check_Consistency("After AgeNodeFrames()");
#endif

  // No point in reconsidering the node we just freed:
  if (++curNode >= objC_nNodes)
    curNode = 0;
  return;
}

Node *
objC_GrabNodeFrame()
{
  if (objC_firstFreeNode == 0)
    objC_AgeNodeFrames();
  
  assert(objC_firstFreeNode);
  assert(objC_nFreeNodeFrames);
  
  Node * pNode = objC_firstFreeNode;
  objC_firstFreeNode = objH_ToNode(node_ToObj(objC_firstFreeNode)->prep_u.nextFree);
  objC_nFreeNodeFrames--;

  ObjectHeader * const pObj = node_ToObj(pNode);
  assert(pObj->obType == ot_NtFreeFrame);

  assert(keyR_IsEmpty(&pObj->keyRing));
  kzero(pObj, sizeof(ObjectHeader));

  pObj->obType = ot_NtUnprepared;

#ifndef NDEBUG
  uint32_t i;
  for (i = 0; i < EROS_NODE_SIZE; i++) {
    if (keyBits_IsUnprepared(&pNode->slot[i]) == false)
      dprintf(true, "Virgin node 0x%08x had prepared slot %d\n",
		pNode, i);
  }
#endif
  assert(objC_ValidNodePtr(pNode));

  DEBUG(ndalloc)
    printf("Allocated node=0x%08x nfree=%d\n", pNode, objC_nFreeNodeFrames);

  node_SetReferenced(pNode);
  objH_ResetKeyRing(pObj);

  return pNode;
}

#if 0	// revisit this if it turns out we need it

/* pObj->obType must be node or ot_PtDataPage. */
bool
objC_CleanFrame2(ObjectHeader *pObj)
{
  if (pObj->obType <= ot_NtLAST_NODE_TYPE)
    /* FIXME: shouldn't we write back the node *before* clearing it? */
    node_DoClearThisNode((Node *)pObj);

  else
    objH_InvalidateProducts(pObj);

  /* Object must be paged out if dirty: */
  if (objH_IsDirty(pObj)) {
    /* If the object got rescued, it won't have hit ageout age, so
     * the only way it should still be dirty is if the write has not
     * completed:
     */

    DEBUG(ckpt)
      dprintf(true, "ty %d oid 0x%08x%08x slq=0x%08x\n",
		      pObj->obType,
		      (uint32_t) (pObj->oid >> 32),
		      (uint32_t) pObj->oid,
		      ObjectStallQueueFromObHdr(pObj));
    
    objC_WriteBack(pObj, false);	/*** return value not used? */
  }

  return true;
}

/* Evict the current resident of a page frame. This is called
 * when we need to claim a particular physical page frame.
 * It is satisfactory to accomplish this by grabbing some
 * other frame and moving the object to it. 
 */
/* This procedure may Yield. */
bool
objC_EvictFrame(PageHeader * pageH)
{
  DEBUG(ndalloc)
    printf("objC_EvictFrame obj=0x%08x type=%d\n",
           pageH, pageH_GetObType(pageH));

  switch (pageH_GetObType(pageH)) {
  case ot_PtFreeFrame:
    while (pageH->kt_u.free.log2Pages != 0)	// just want one page
      physMem_SplitContainingFreeBlock(pageH);
    break;

  case ot_PtKernelHeap:
    return false;	// not implemented yet: FIXME

  case ot_PtNewAlloc:
    assert(false);	// should not have this now

  case ot_PtDevicePage:	// can't evict this
  case ot_PtDMABlock:	// can't evict this
  case ot_PtDMASecondary:	// can't evict this
  case ot_PtSecondary:	/* This is part of a free block. We don't need
		to evict it, but we won't bother to split up the block. */
    return false;

  case ot_PtDataPage:
    keyR_UnprepareAll(&pageH_ToObj(pageH)->keyRing);	/* This zaps any PTE's as a side effect. */
    if (!objC_CleanFrame2(pageH_ToObj(pageH))) {
      (void) objC_CopyObject(pageH_ToObj(pageH));
  
      /* Since we could not write the old frame out, we assume that it
       * is not backed by anything. In this case, the right thing to do
       * is to simply mark the old one clean, turn off it's checkpoint
       * bit if any (it's not writable anyway), and allow ReleaseFrame()
       * to release it.
       */

      pageH_ClearFlags(pageH, OFLG_CKPT | OFLG_DIRTY);
    }
    assert(keyR_IsEmpty(&pageH_ToObj(pageH)->keyRing));
    ReleaseObjPageFrame(pageH);
    break;

  default:
    pageH_mdType_EvictFrame(pageH);
    break;
  }

#if 0	// until I figure out what we need to do here
  // Unlink from free list.
  PageHeader * * pp = &objC_firstFreePage;
  while (*pp != pageH) {
    assert(*pp);	// else not found in list
    pp = &(*pp)->kt_u.free.next;
  }
  (*pp) = pageH->kt_u.free.next;
  objC_nFreePageFrames--;

  pageH_ToObj(pageH)->obType = ot_PtNewAlloc; /* until further notice */

  objC_GrabThisPageFrame(pageH);
#else
  assert(false);
#endif
  return true;
}
#endif

static void
IOReq_EndPageClean(IORequest * ioreq)
{
  // The IORequest is done.
  PageHeader * pageH = ioreq->pageH;
  ObjectHeader * pObj = pageH_ToObj(pageH);

  // Mark the page as no longer having I/O.
  pageH->ioreq = NULL;

  if (pageH_IsDirty(pageH)) {
    // The page was dirtied while it was being written.
    // Tough luck; writing the page was a waste of time.
    // This log location will not be used.
    // No point in making a log directory entry.
  } else {
    ObjectDescriptor objDescr = {
      .oid = pObj->oid,
      .logLoc = FrameToOID(ioreq->rangeLoc) + ioreq->objRange->start,
      .allocCount = pObj->allocCount,
      .callCount = 0,	// not used
      .type = capros_Range_otPage
    };
    ld_recordLocation(&objDescr, workingGenerationNumber);
  }
  sq_WakeAll(&ioreq->sq, false);
  // Caller has unlinked the ioreq and will deallocate it.
}

static void
pageH_Clean(PageHeader * pageH)
{
  if (! pageH_IsDirty(pageH))
    return;

  switch (pageH_GetObType(pageH)) {
  case ot_PtLogPot:
    /* Log pots are cleaned as soon as they are dirtied,
    so they should not get here. */
  default: ;
    assert(false);

  case ot_PtHomePot:
  case ot_PtTagPot:
    assert(!"complete");	////
    break;

  case ot_PtDataPage:
    keyR_TrackDirty(&pageH_ToObj(pageH)->keyRing);
    pageH_ClearFlags(pageH, OFLG_DIRTY);
    // While it is being cleaned, it is marked not dirty, and I/O in progress.

    IORequest * ioreq = IOReqCleaning_AllocateOrWait();	// may Yield
    ioreq->pageH = pageH;	// link page and ioreq
    pageH->ioreq = ioreq;

    LID lid = NextLogLoc();
    ObjectRange * rng = LidToRange(lid);
    assert(rng);	// it had better be mounted

    ioreq->requestCode = capros_IOReqQ_RequestType_writeRangeLoc;
    ioreq->objRange = rng;
    ioreq->rangeLoc = OIDToFrame(lid - rng->start);
    ioreq->doneFn = &IOReq_EndPageClean;
    sq_Init(&ioreq->sq);
    ioreq_Enqueue(ioreq);
  }
}

/* OBJECT AGING POLICY:
 * 
 * Objects are brought in as NewBorn objects.
 * If they are explicitly referenced, the age is reset to NewBorn.
 * Objects age until they reach the Invalidate age.
 * At that point all mapping table entries dependent on the object are
 * invalidated, to detect implicit references via them.
 *
 * At age_Clean, we initiate cleaning the object (if it is dirty).
 * At age_Steal, we steal the object frame.
 */

/* To ensure that we can make progress, IORequests for cleaning
 * are in a different pool from IORequests for reading.
 * The relative pool sizes can be adjusted, and this may help
 * manage the disk bandwidth.
 *
 * Note: It might be beneficial to clean objects (if the I/O bandwidth
 * is available) without aging objects.
 * This will help the next checkpoint to stabilize more quickly.
 * 
 * Note: Once we have initiated cleaning of a dirty page, perhaps
 * we should initiate a few more at that time,
 * while the disk head is at the log.
 */

void
objC_AgePageFrames(void)
{
  static uint32_t curPage = 0;
  uint32_t nStuck = 0;
  uint32_t nPinned = 0;
  int pass;

  DEBUG(aging) printf("objC_AgePageFrames called.");

#ifdef DBG_WILD_PTR
  if (dbg_wild_ptr)
    check_Consistency("Before AgePageFrames()");
#endif

  for (pass = 0; pass <= age_Steal; pass++) {
    nPinned = 0;
    nStuck = 0;
    uint32_t count;
    for (count = 0; count < objC_nNodes; count++) {
      PageHeader * pageH = objC_GetCorePageFrame(curPage);
      
      switch (pageH_GetObType(pageH)) {
      /* Some page types do not get aged: */
      case ot_PtNewAlloc:
      case ot_PtDevicePage:
      case ot_PtKernelHeap:
      case ot_PtDMABlock:
      case ot_PtDMASecondary:
      case ot_PtFreeFrame:
      case ot_PtSecondary:
        nStuck++;
        goto nextPage;

      case ot_PtTagPot:
      case ot_PtHomePot:
      case ot_PtLogPot:
        break;

      case ot_PtDataPage:
        if (! objH_GetFlags(pageH_ToObj(pageH), OFLG_Cleanable))
          goto nextPage;
        break;

      default:
        // It's a machine-dependent frame type.
        if (pageH_mdType_Aging(pageH)) {
          curPage++;	// it was freed
          return;
        }
        goto nextPage;
      }
  
      /* Some pages cannot be aged because they are active or pinned: */
      if (objH_IsUserPinned(pageH_ToObj(pageH))
          || pageH_IsKernelPinned(pageH) ) {
        nPinned++;
        nStuck++;
        goto nextPage;
      }

      /* Since the object isn't pinned, set its transaction ID to zero
      so it won't inadvertently be considered pinned
      when objH_CurrentTransaction overflows. */
      pageH_ToObj(pageH)->userPin = 0;

      if (objH_GetFlags(pageH_ToObj(pageH), OFLG_Fetching) ) {
        nStuck++;
        goto nextPage;
      }

      switch (pageH_ToObj(pageH)->objAge) {
      case age_Invalidate:
        /* First, we check whether the object is really not recently used.
         * Normally, any reference to the object sets its objAge to age_NewBorn
         * (in objH_SetReferenced). 
         * However, references through mapping table entries happen without
         * benefit of updating objAge.
         * So at this stage we invalidate the mapping table entries.
         * If the object is referenced,
         * the entry will be rebuilt and the age updated then.
         */
        keyR_TrackReferenced(&pageH_ToObj(pageH)->keyRing);
      default:
      bumpAge:
        pageH_ToObj(pageH)->objAge++;
        break;

      case age_Clean:
        /* At this stage, we initiate cleaning of the object if it is dirty. */
        pageH_Clean(pageH);
        goto bumpAge;

      case age_Steal:
        /* Now it's time to evict the object and steal the frame. */

        /* Dirtying a page also references it, which resets the age.
        If we are at age_Steal, it wasn't referenced since age_Clean,
        therefore it shouldn't be dirty either. */
        assert(! pageH_IsDirty(pageH));

        if (pageH->ioreq) {
          /* We are still waiting for a previously initiated clean
          to complete. Wait now for it to complete.
          Otherwise, we would go on to preferentially steal clean pages,
          usually code pages. */
          act_SleepOn(&pageH->ioreq->sq);
          act_Yield();
        }

        // Steal this frame.
        objH_InvalidateProducts(pageH_ToObj(pageH));
        keyR_UnprepareAll(&pageH_ToObj(pageH)->keyRing);
        ReleaseObjPageFrame(pageH);

        if (++curPage >= objC_nPages)
          curPage = 0;
        return;
      }

    nextPage:
      if (++curPage >= objC_nPages)
        curPage = 0;
    }
  }

  fatal("%d stuck pages of which %d are pinned\n",
		nStuck, nPinned);
}

/* May Yield. */
PageHeader *
objC_GrabPageFrame(void)
{
  PageHeader * pageH;

  while (1) {
    // Try to allocate one page.
    pageH = physMem_AllocateBlock(1);
    if (pageH) break;

    // WaitForAvailablePageFrame
    objC_AgePageFrames();
  };

  DEBUG(ndalloc)
    printf("objC_GrabPageFrame obj=0x%08x\n", pageH);

  objC_GrabThisPageFrame(pageH);

  return pageH;
}

// Ensure that there are at least numFrames free objects.
void
EnsureObjFrames(unsigned int baseType, unsigned int numFrames)
{
  switch (baseType) {
  default: ;
    assert(false);

  case capros_Range_otPage:
    // FIXME: this loop may never terminate!
    while (physMem_numFreePageFrames < numFrames)
      objC_AgePageFrames();
    break;

  case capros_Range_otNode:
    // FIXME: this loop may never terminate!
    while (objC_nFreeNodeFrames < numFrames)
      objC_AgeNodeFrames();
    break;
  }
}

ObjectHeader *
CreateNewNullObject(unsigned int baseType, OID oid, ObCount allocCount)
{
  ObjectHeader * pObj;

  switch (baseType) {
  default: ;
    assert(false);

  case capros_Range_otPage:
  {
    PageHeader * pageH = objC_GrabPageFrame();
    pObj = pageH_ToObj(pageH);

    void * dest = (void *) pageH_GetPageVAddr(pageH);
    kzero(dest, EROS_PAGE_SIZE);

    pageH_MDInitDataPage(pageH);
    pageH_SetReferenced(pageH);
    break;
  }

  case capros_Range_otNode:
  {
    Node * pNode = objC_GrabNodeFrame();
    pObj = node_ToObj(pNode);

    pNode->callCount = allocCount;	// use for call count too
    pNode->nodeData = 0;

    uint32_t ndx;
    for (ndx = 0; ndx < EROS_NODE_SIZE; ndx++) {
      assert (keyBits_IsUnprepared(&pNode->slot[ndx]));
      /* not hazarded because newly loaded node */
      keyBits_InitToVoid(&pNode->slot[ndx]);
    }

    node_SetReferenced(pNode);
  }
  }

  objH_InitDirtyObj(pObj, oid, baseType, allocCount);

  return pObj;
}

#if 0
void
ObjectCache::RequirePageFrames(uint32_t n)
{
  while (nFreeNodeFrames < n)
    AgePageFrames();
}
#endif

#if 0	// revisit this when we need it
/* This is used for copy on write processing, and also for frame
 * eviction. The copy becomes current, and is initially clean. The
 * original is no longer current, but may still be the checkpoint
 * version. */
/* May Yield. */
static ObjectHeader *
objC_CopyObject(ObjectHeader *pObj)
{
  ObjectHeader *newObj;
  kva_t fromAddr;
  kva_t toAddr;
  unsigned i = 0;

  DEBUG(ndalloc)
    printf("objC_CopyObject obj=0x%08x\n", pObj);

  assert(pObj->prep_u.products == 0);

  objH_TransLock(pObj);	// needed?

  if (pObj->obType == ot_PtDataPage) {
    assert(keyR_IsEmpty(&pObj->keyRing));
    /* Copy data page only, not ot_PtDevicePage. */
    /* Object is now free of encumberance, but it wasn't an evictable
     * object, and it may be dirty. We need to find another location
     * for it.
     */
    assert(pte_ObIsNotWritable(objH_ToPage(pObj)));

    PageHeader * newPage = objC_GrabPageFrame();
    newObj = pageH_ToObj(newPage);

    assert(newObj != pObj);

    fromAddr = pageH_GetPageVAddr(objH_ToPage(pObj));
    toAddr = pageH_GetPageVAddr(newPage);

    memcpy((void *) toAddr, (void *) fromAddr, EROS_PAGE_SIZE);
    pageH_SetReferenced(newPage);	/* FIX: is this right? */
    pageH_MDInitDataPage(newPage);
  }
  else { /* It's a node */
    assert (pObj->obType <= ot_NtLAST_NODE_TYPE
            && pObj->obType != ot_NtFreeFrame);
    assert(keyR_IsEmpty(&pObj->keyRing));

    Node * oldNode = (Node *) pObj;
    Node * newNode = objC_GrabNodeFrame();
    newObj = node_ToObj(newNode);

    assert(newObj != pObj);

    newNode->callCount = oldNode->callCount;
    for (i = 0; i < EROS_NODE_SIZE; i++) {

      key_NH_Set(node_GetKeyAtSlot(newNode, i), node_GetKeyAtSlot(oldNode, i));

    }
    node_SetReferenced(newNode);	/* FIX: is this right? */
  }

  newObj->allocCount = pObj->allocCount;
  objH_InitObj(newObj, pObj->oid);
  // FIXME: Init obtype to capros_Range_ot*.

  objH_SetFlags(newObj, objH_GetFlags(pObj, OFLG_DISKCAPS)); // correct?
  /* The copy is now current. The old object is still the checkpoint
     version. */
  objH_ClearFlags(pObj, OFLG_CURRENT);

  return newObj;
}
#endif

/* Process and Depend entries are allocated before we get here.
 * Here we allocate only nodes, pages, and core table
 * entries.  Nodes and pages are allocated in equal proportions, with
 * one core table entry per page.
 */

void
objC_Init()
{
  uint32_t availBytes;
  uint32_t allocQuanta;
  uint32_t i;

  availBytes = physMem_AvailBytes(&physMem_any);
    
  DEBUG(cachealloc)
    printf("%d bytes of available storage, sizeof(Node) = %d,"
           " sizeof(ObjectHeader) = %d.\n",
           availBytes, sizeof(Node), sizeof(ObjectHeader));

  allocQuanta =
    sizeof(Node) + EROS_PAGE_SIZE + sizeof(ObjectHeader);

  objC_nNodes = availBytes / allocQuanta;
    
#ifdef TESTING_AGEING
  objC_nNodes = 90;			/* This is one less than logtst requires. */
#endif
  
  objC_nodeTable = KPAtoP(Node *,
                     physMem_Alloc(objC_nNodes*sizeof(Node), &physMem_any));
  kzero(objC_nodeTable, objC_nNodes*sizeof(Node));

  for (i = 0; i < objC_nNodes; i++) {
    unsigned int j;
    Node * n = &objC_nodeTable[i];

    for (j = 0; j < EROS_NODE_SIZE; j++)
      keyBits_InitToVoid(node_GetKeyAtSlot(n, j));

    keyR_ResetRing(&n->node_ObjHdr.keyRing);
    // n->node_ObjHdr.flags = 0;
    // n->node_ObjHdr.userPin = 0;
    n->node_ObjHdr.obType = ot_NtFreeFrame;
  }

  DEBUG(cachealloc)
    printf("Allocated Nodes: 0x%x at 0x%08x\n",
		   sizeof(Node[objC_nNodes]), objC_nodeTable);

  /* Drop all the nodes on the free node list: */
  for (i = 0; i < objC_nNodes; i++) {
    /* object type is set in constructor... */
    assert (objC_nodeTable[i].node_ObjHdr.obType == ot_NtFreeFrame);
    objC_nodeTable[i].node_ObjHdr.prep_u.nextFree
      = node_ToObj(&objC_nodeTable[i+1]);
  }
  
  objC_nodeTable[objC_nNodes - 1].node_ObjHdr.prep_u.nextFree = 0;
  objC_firstFreeNode = &objC_nodeTable[0];

  objC_nFreeNodeFrames = objC_nNodes;

  Depend_InitKeyDependTable(objC_nNodes);

  DEBUG(cachealloc)
    printf("%d bytes of available storage after key dep tbl alloc.\n", availBytes);

  /* Add all the device pages.
     By adding them now, before calling objC_AllocateUserPages(),
     we can get storage for the PageHeader entries from contiguous
     physical memory, rather than the heap. */
  for (i = 0; i < physMem_nPmemInfo; i++) {
    PmemInfo *pmi = &physMem_pmemInfo[i];

    if (pmi->type == MI_DEVICEMEM || pmi->type == MI_BOOTROM) {
      // Need to special case the publication of device mem, as we need
      // object cache entries for these, because there will be keys to them.
      objC_AddDevicePages(pmi);
    }
  }

  objC_AllocateUserPages();

  DEBUG(cachealloc)
    printf("%d cached domains, %d nodes, %d pages\n",
		   KTUNE_NCONTEXT,
		   objC_nNodes, objC_nPages);
}
