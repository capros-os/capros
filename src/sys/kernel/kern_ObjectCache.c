/*
 * Copyright (C) 1998, 1999, Jonathan S. Shapiro.
 * Copyright (C) 2005-2010, 2012, Strawberry Development Group.
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
#include <kerninc/Ckpt.h>
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
#define dbg_nodelist	0x20	// free node list

/* Following should be an OR of some of the above */
#define dbg_flags   ( 0u )

#define DEBUG(x) if (dbg_##x & dbg_flags)

uint32_t objC_nNodes;
uint32_t objC_nFreeNodeFrames;
Node *objC_nodeTable;
Node *objC_firstFreeNode;

uint32_t objC_nPages;
PageHeader * objC_coreTable;

struct CorePageIterator cpi_Aging;
struct CorePageIterator cpi_KROPageCleanCursor;

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

  memset(objC_coreTable, 0, objC_nPages * sizeof(PageHeader));
  /* Initialize all the core table entries: */

  PageHeader * pObHdr = objC_coreTable;
  for (rgn = 0; rgn < physMem_nPmemInfo; rgn++) {
    PmemInfo * pmi= &physMem_pmemInfo[rgn];
    if (pmi->type != MI_MEMORY)
      continue;

    uint32_t pg;
    for (pg = 0; pg < pmi->nPages; pg++) {
      pObHdr->physMemRegion = pmi;
      pObHdr->kt_u.free.obType = ot_PtFreeSecondary;
      pObHdr++;
    }

    physMem_FreeAll(pmi);	// Free all the pages in this region.
  }
  assert(pObHdr == objC_coreTable + objC_nPages);

  CorePageIterator_Init(&cpi_Aging);	// init aging cursor
  CorePageIterator_Init(&cpi_KROPageCleanCursor);	// init KRO cursor
}

static void
AddCoherentPages(PageHeader * pageH, PmemInfo * pmi, kpg_t nPages,
  OID oid, unsigned int obType)
{
  unsigned int pg;
  PageHeader * ph = pageH;

  kzero(pageH, sizeof(PageHeader) * nPages);

  for (pg = 0;
       pg < nPages;
       pg++, ph++, oid += EROS_OBJECTS_PER_FRAME) {
    ph->physMemRegion = pmi;
    pageH_MDInitDevicePage(ph);	// make it coherent

    ObjectHeader * pObj = pageH_ToObj(ph);
    pObj->obType = ot_PtSecondary;	// all except the first, see below
    pObj->oid = oid;
    pObj->allocCount = restartNPCount;
    objH_SetDirtyFlag(pObj);
    // No objH_CalcCheck for device pages.
    objH_ResetKeyRing(pObj);
    objH_Intern(pObj);
    objH_ClearFlags(pObj, OFLG_Cleanable);
  }

  // First frame has its own type:
  pageH_ToObj(pageH)->obType = obType;
}

PageHeader *
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
  AddCoherentPages(pageH, pmi, nPages, frameOid, ot_PtDevBlock);
  return pageH;
}

void
objC_AddDMAPages(PageHeader * pageH, kpg_t nPages)
{
  assert(pageH->kt_u.free.obType == ot_PtNewAlloc);

  PmemInfo * pmi = pageH->physMemRegion;

  OID oid = pageH_ToPhysPgNum(pageH) * EROS_OBJECTS_PER_FRAME
            + OID_RESERVED_PHYSRANGE;

  AddCoherentPages(pageH, pmi, nPages, oid, ot_PtDMABlock);
}

void
CorePageIterator_Init(struct CorePageIterator * cpi)
{
  cpi->pmi = &physMem_pmemInfo[0];
  cpi->regionsLeft = physMem_nPmemInfo;
  cpi->pagesLeft = 0;
}

PageHeader *
CorePageIterator_Next(struct CorePageIterator * cpi)
{
  if (! cpi->pagesLeft) {
    while (1) {
      if (! cpi->regionsLeft)
        return NULL;
      cpi->regionsLeft--;
      PmemInfo * pmi = cpi->pmi++;
      if (pmi->type == MI_MEMORY) {
        cpi->pagesLeft = pmi->nPages;
        cpi->pageH = &pmi->firstObHdr[0];
        break;
      }
    }
  }
  cpi->pagesLeft--;
  return cpi->pageH++;
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

/* Put a page on the free list. */
/* Caller must have already removed all previous entanglements. */
void
ReleasePageFrame(PageHeader * pageH)
{
  physMem_FreeBlock(pageH, 1);
}

static void
CheckFreeNodeList(void)
{
#if (dbg_flags & dbg_nodelist)
  unsigned long num;
  Node * pNode;
  for (num = 0, pNode = objC_firstFreeNode;
       pNode;
       pNode = objH_ToNode(node_ToObj(pNode)->prep_u.nextFree) ) {
    assertex(pNode, node_ToObj(pNode)->obType == ot_NtFreeFrame);
    num++;
    if (num > objC_nFreeNodeFrames)
      dprintf(true, "Free node chain too long! %#x %#x %d\n",
              objC_firstFreeNode, pNode, objC_nFreeNodeFrames);
  }
  assert(num == objC_nFreeNodeFrames);
#endif
}

/* Remove this node from the cache and return it to the free node list.
 * There must be no hazarded keys in the node. */
void
ReleaseNodeFrame(Node * pNode)
{
  DEBUG(ndalloc)
    printf("ReleaseNodeFrame node=%#x\n", pNode);

  DEBUG(nodelist) CheckFreeNodeList();
  ObjectHeader * pObj = node_ToObj(pNode);

  assert(pObj->obType != ot_NtFreeFrame);
  assert(!objH_IsDirty(pObj));

  keyR_UnprepareAll(&pObj->keyRing);
  objH_InvalidateProducts(pObj);

  unsigned int i;
  for (i = 0; i < EROS_NODE_SIZE; i++) {
    Key * key = node_GetKeyAtSlot(pNode, i);
    assertex(pNode, ! keyBits_IsHazard(key));
    key_NH_Unprepare(key);
  }

  objH_Unintern(pObj);

  pObj->obType = ot_NtFreeFrame;

  pObj->prep_u.nextFree = node_ToObj(objC_firstFreeNode);
  objC_firstFreeNode = pNode;
  objC_nFreeNodeFrames++;

  DEBUG(nodelist) CheckFreeNodeList();
}

/* Release a page that has an ObjectHeader. */
void
ReleaseObjPageFrame(PageHeader * pageH)
{
  DEBUG(pgalloc)
    printf("ReleaseObjPageFrame pageH=0x%08x\n", pageH);

  assert(!pageH_IsDirty(pageH));
  // The following test is slow:
  // assert(pte_ObIsNotWritable(pageH));

  objH_Unintern(pageH_ToObj(pageH));
    
  ReleasePageFrame(pageH);
}

void
objH_InitObj(ObjectHeader * pObj, OID oid)
{
  pObj->oid = oid;

  // Flags default to zero.

  objH_ResetKeyRing(pObj);
  objH_Intern(pObj);
}

void
objH_InitPresentObj(ObjectHeader * pObj, OID oid)
{
#ifdef OPTION_OB_MOD_CHECK
  objH_SetCheck(pObj);
#endif

  objH_InitObj(pObj, oid);
}

void
objH_InitDirtyObj(ObjectHeader * pObj, OID oid, unsigned int baseType,
  ObCount allocCount)
{
  pObj->allocCount = allocCount;
  pObj->obType = BaseTypeToObType(baseType);
  objH_InitPresentObj(pObj, oid);
  // Object is dirty because we just initialized it:
  objH_SetDirtyFlag(pObj);
  if (OIDIsPersistent(oid)) {
    objH_SetFlags(pObj, OFLG_Cleanable);
  } else {
    // not persistent, not cleanable
    objH_ClearFlags(pObj, OFLG_Cleanable);
  }
}

void
objC_GrabThisPageFrame(PageHeader *pageH)
{
  assert(pageH_GetObType(pageH) == ot_PtNewAlloc);

  PmemInfo * pmi = pageH->physMemRegion;	// preserve this field
  kzero(pageH, sizeof(*pageH));
  pageH->physMemRegion = pmi;
  pageH_ToObj(pageH)->obType = ot_PtNewAlloc;	// restore

  // The following test is too slow even for the debug version:
  // assert(pte_ObIsNotWritable(pageH));

  pageH_SetReferenced(pageH);
}

/* The first numNodesToClean entries of nodesToClean are candidates
 * for cleaning. They may have been referenced since being added. */
Node * nodesToClean[DISK_NODES_PER_PAGE];
unsigned int numNodesToClean = 0;

/* Add this node to the list of candidates for cleaning. */
static void
AddNodeToClean(Node * pNode)
{
  unsigned int i;

  assert(objH_IsDirty(node_ToObj(pNode)));
  assert(numNodesToClean < DISK_NODES_PER_PAGE);

  // We mustn't have duplicates:
  for (i = 0; i < numNodesToClean; i++) {
    if (pNode == nodesToClean[i])
      return;
  }
  // OK to add it:
  nodesToClean[numNodesToClean++] = pNode;
}

static void
objH_ClearKRO(ObjectHeader * pObj)
{
  objH_ClearFlags(pObj, OFLG_KRO);
  sq_WakeAll(ObjectStallQueueFromObHdr(pObj));
}

static void
node_ClearDirty(Node * pNode)
{
  ObjectHeader * pObj = node_ToObj(pNode);

  if (objH_IsKRO(pObj)) {
    numKRONodes--;
    assert(numKRONodes >= 0);
    objH_ClearKRO(pObj);
  }
  nodeH_BecomeUnwriteable(pNode);
  objH_ClearFlags(pObj, OFLG_DIRTY);	// cleaned it to the log dir
}

/* Returns true iff freed some nodes. */
// May Yield
static bool
CleanAPotOfNodes(bool force)
{
  unsigned int i;
  ObjectHeader * pObj;

  if (numNodesToClean < DISK_NODES_PER_PAGE
      && ! force)
    return false;	// wait till we get more nodes

  for (i = 0; i < numNodesToClean; i++) {
    Node * pNode = nodesToClean[i];
    pObj = node_ToObj(pNode);
    if ((pObj->objAge < age_Steal	// It was referenced
         && ! objH_IsKRO(pObj) )	// and not KRO
        || pObj->obType == ot_NtFreeFrame) { // It was freed
      // It's no longer a candidate to clean.
      // Remove it.
      nodesToClean[i--] = nodesToClean[--numNodesToClean];
      // numNodesToClean now < DISK_NODES_PER_PAGE.
      if (! force)
        return false;
    }
  }

  PageHeader * pageH = objC_GrabPageFrame();	// may Yield

  // Now we are committed to creating a log pot.

  LID lid = NextLogLoc();
  DEBUG(ckpt) printf("Writing LID %#llx\n", lid);

  DiskNode * dn = (DiskNode *)pageH_GetPageVAddr(pageH);
  for (i = 0; i < numNodesToClean; i++, dn++) {
    Node * pNode = nodesToClean[i];
    pObj = node_ToObj(pNode);
    node_CopyToDiskNode(pNode, dn);
    node_ClearDirty(pNode);
    ObjectDescriptor objDescr = {
      .oid = pObj->oid,
      .allocCount = pObj->allocCount,
      .callCount = pNode->callCount,
      .logLoc = lid + i,
      .type = capros_Range_otNode
    };
    ld_recordLocation(&objDescr, workingGenerationNumber);
  }
  numNodesToClean = 0;

  // Initialize the pot.
  pObj = pageH_ToObj(pageH);
  pObj->obType = ot_PtLogPot;
  objH_InitPresentObj(pObj, lid);

  // Clean it right away if possible.
  IORequest * ioreq = IOReqCleaning_Allocate();
  if (ioreq)
    CleanLogPot(pageH, ioreq);

  return true;
}

/* Returns true if cleaned it,
 * false if queued it to be cleaned. */
static bool
node_Clean(Node * pNode)
{
  ObjectHeader * pObj = node_ToObj(pNode);

  if (node_IsNull(pNode)) {
    ObjectDescriptor objDescr = {
      .oid = pObj->oid,
      .allocCount = pObj->allocCount,
      .callCount = pNode->callCount,
      .logLoc = 0,	// it's null
      .type = capros_Range_otNode
    };
    ld_recordLocation(&objDescr, workingGenerationNumber);
    node_ClearDirty(pNode);
    return true;
  } else {
    AddNodeToClean(pNode);
    return CleanAPotOfNodes(false);
  }
}

/* May Yield. */
static void
objC_AgeNodeFrames(void)
{
  static uint32_t curNode = 0;
  uint32_t nStuck = 0;
  uint32_t nPinned = 0;
  int pass;
  Node * pNode = 0;

  DEBUG(aging) printf("objC_AgeNodeFrames called.\n");

#ifdef DBG_WILD_PTR
  if (dbg_wild_ptr)
    check_Consistency("Before AgeNodeFrames()");
#endif

  /* It may be that we already have a potful of nodes to be cleaned, 
   * but they didn't get cleaned because we were unable to get a page frame
   * or IORequest. Therefore check again here. */ 
  if (CleanAPotOfNodes(false)) {	// cleaned some nodes
    pNode = nodesToClean[0];		// one that we cleaned
    goto stealNode;
  }

  for (pass = 0; pass <= age_Steal; pass++) {
    nPinned = 0;
    nStuck = 0;
    uint32_t count;
    for (count = 0; count < objC_nNodes; count++) {
      pNode = objC_GetCoreNodeFrame(curNode);
      ObjectHeader * pObj = node_ToObj(pNode);
    
      if (pObj->obType == ot_NtFreeFrame)
	return;
    
      assert(pObj->objAge <= age_Steal);
    
      if (objH_IsUserPinned(pObj)) {
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
        keyR_ProcessAllMaps(&pObj->keyRing, &KeyDependEntry_TrackReferenced);
      default:
	pObj->objAge++;
        break;

      case age_Steal:
    
        DEBUG(ckpt)
	  dprintf(false, "Ageing out node=0x%08x oty=%d dirty=%c oid=%#llx\n",
			pNode, pObj->obType,
			(objH_IsDirty(pObj) ? 'y' : 'n'), pObj->oid);

        if (objH_IsDirty(pObj)) {
          /* During a checkpoint, the current version can't be cleaned;
          it must go into the next generation. */
          if (ckptIsActive() && ! objH_IsKRO(pObj)) {
            nStuck++;
            goto nextNode;
          }

          if (node_Clean(pNode))
            goto stealNode;
        } else {	// node is clean
          goto stealNode;
        }
      }

    nextNode:
      if (++curNode >= objC_nNodes)
	curNode = 0;
    }
  }

  fatal("%d stuck nodes of which %d are pinned\n",
		nStuck, nPinned);

stealNode:	// Steal pNode.
  assert(! objH_IsDirty(node_ToObj(pNode)));
  node_ClearAllHazards(pNode);
  ReleaseNodeFrame(pNode);

#if 0
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
objC_GrabNodeFrame(void)
{
  DEBUG(nodelist) CheckFreeNodeList();

  if (objC_firstFreeNode == 0)
    objC_AgeNodeFrames();
  
  assert(objC_firstFreeNode);
  assert(objC_nFreeNodeFrames);
  
  Node * pNode = objC_firstFreeNode;
  ObjectHeader * const pObj = node_ToObj(pNode);
  assertex(pNode, pObj->obType == ot_NtFreeFrame);

  objC_firstFreeNode = objH_ToNode(pObj->prep_u.nextFree);
  objC_nFreeNodeFrames--;

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

  DEBUG(nodelist) CheckFreeNodeList();

  return pNode;
}

// Clean the next KRO node.
// Return true if cleaned one, false if there are no more.
bool
CleanAKRONode(void)
{
  assert(numNodesToClean < DISK_NODES_PER_PAGE);////?

  while (KRONodeCleanCursor < objC_nNodes) {
    Node * pNode = objC_GetCoreNodeFrame(KRONodeCleanCursor++);
    ObjectHeader * pObj = node_ToObj(pNode);
    if (objH_IsKRO(pObj)) {
      node_Clean(pNode);
      return true;
    }
  }
  if (numNodesToClean)
    CleanAPotOfNodes(true);	// force out the last pot
  assert(! numKRONodes);
  return false;
}

/* This procedure is called when we want to mutate a node that is
 * Kernel Read Only.
 *
 * If the node can't be made writeable, this enqueues the current process
 * on the ObjectStallQueue or another queue and Yields. */
void
node_MitigateKRO(Node * pNode)
{
  ObjectHeader * pObj = node_ToObj(pNode);

  assert(pObj->obType != ot_NtFreeFrame);
  assert(objH_GetFlags(pObj, OFLG_Cleanable | OFLG_DIRTY | OFLG_KRO)
         == (OFLG_Cleanable | OFLG_DIRTY | OFLG_KRO));

  CleanAPotOfNodes(false);	// ensure numNodesToClean < DISK_NODES_PER_PAGE
  if (! objH_IsKRO(pObj))	// it got cleaned
    return;
  if (node_Clean(pNode))
    return;
  /* The node was queued to be cleaned.
  Keep cleaning nodes until the pot is cleaned.
  These may not be "hot" nodes, but they do need to be cleaned eventually,
  and most likely can be cleaned (to a pot) without requiring I/O. */
  while (objH_IsKRO(pObj))
    CleanAKRONode();
}

#if 0	// revisit this if it turns out we need it

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

  case ot_PtKernelUse:
    return false;	// not implemented yet: FIXME

  case ot_PtNewAlloc:
    assert(false);	// should not have this now

  case ot_PtDevBlock:	// can't evict this
  case ot_PtDMABlock:	// can't evict this
  case ot_PtSecondary:	// can't evict this
  case ot_PtFreeSecondary:	/* This is part of a free block. We don't need
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

void
CreateLogDirEntryForNonzeroPage(PageHeader * pageH)
{
  IORequest * ioreq = pageH->ioreq;
  ObjectHeader * pObj = pageH_ToObj(pageH);
  ObjectDescriptor objDescr = {
    .oid = pObj->oid,
    .logLoc = FrameToOID(ioreq->rangeLoc) + ioreq->objRange->start,
    .allocCount = pObj->allocCount,
    .callCount = 0,	// not used
    .type = capros_Range_otPage
  };
  ld_recordLocation(&objDescr, workingGenerationNumber);
}

void
DoneCleaningPage(PageHeader * pageH)
{
  ObjectHeader * pObj = pageH_ToObj(pageH);

  // Clear any KRO:
  if (objH_IsKRO(pObj))
    objH_ClearKRO(pObj);

  if (pageH_GetObType(pageH) == ot_PtWorkingCopy) {
    // Done cleaning a WorkingCopy.
    // We have no further use for the page, so free it.
    assert(! pageH_IsDirty(pageH));
    assert(! pObj->prep_u.products);
    assert(keyR_IsEmpty(&pageH_ToObj(pageH)->keyRing));
    ReleasePageFrame(pageH);
  }
}

static void
IOReq_EndPageClean(IORequest * ioreq)
{
  // The IORequest is done.
  PageHeader * pageH = ioreq->pageH;
  ObjectHeader * pObj = pageH_ToObj(pageH);

  if (objH_IsDirty(pObj)) {
    // If it's KRO, it had better not be dirty:
    assert(! objH_IsKRO(pObj));

    // The page was dirtied while it was being written.
    // Tough luck; writing the page was a waste of time.
    // This log location will not be used.
    // No point in making a log directory entry.
  } else {
    if (! objH_IsKRO(pObj))
      CreateLogDirEntryForNonzeroPage(pageH);
    // else KRO, dir ent was created earlier.
  }

  // Mark the page as no longer having I/O.
  pageH->ioreq = NULL;

  DoneCleaningPage(pageH);

  sq_WakeAll(&ioreq->sq);
  IOReq_Deallocate(ioreq);
}

static void
CleanZeroPage(PageHeader * pageH)
{
  ObjectDescriptor objDescr = {
    .oid = pageH_ToObj(pageH)->oid,
    .logLoc = 0,	// it's zero, has no logLoc
    .allocCount = pageH_ToObj(pageH)->allocCount,
    .callCount = 0,	// not used
    .type = capros_Range_otPage
  };
  ld_recordLocation(&objDescr, workingGenerationNumber);
  DoneCleaningPage(pageH);
}

/* On entry pgAddr is the virtual address of a page mapped by
 * pageH_MapCoherentRead.
 * Returns i such that the first i bytes of the page are known to be zero. */
static unsigned int
PageTestZero(kva_t pgAddr)
{
  /* The following loop could be optimized with machine-specific code. */
  uint64_t * pgArray = (uint64_t *)pgAddr;
  unsigned int i;
  for (i = 0; i < EROS_PAGE_SIZE; i += sizeof(uint64_t)) {
    if (pgArray[i/sizeof(uint64_t)])
      break;
  }
  return i;
}

/* If we succeed in initiating cleaning this page, we return false.
   If we can't clean the page now, and the current process is not the PFH,
     we Yield.
   Otherwise we return true.
     (Causing the PFH to Yield could result in deadlock.) */
static bool
pageH_Clean(PageHeader * pageH)
{
  if (pageH->ioreq) {	// It is already being cleaned
    if (proc_IsPFH(proc_Current()))
      return true;
    SleepOnPFHQueue(&pageH->ioreq->sq);	// wait for that to finish first
  }

  switch (pageH_GetObType(pageH)) {
  default: ;
    assertex(pageH, false);

  case ot_PtHomePot:
  case ot_PtTagPot:
    assert(!"complete");	////
    break;

  case ot_PtLogPot:
  {
    /* Log pots are cleaned as soon as they are dirtied, if possible.
    If not, we clean them here. */
    IORequest * ioreq = IOReqCleaning_Allocate();
    if (! ioreq) {
      if (proc_IsPFH(proc_Current()))
        return true;
      SleepOnIOReqCleaning();
    }
    // Beware, must not Yield below without freeing ioreq.
    CleanLogPot(pageH, ioreq);
    break;
  }

  case ot_PtDataPage:
    keyR_ProcessAllMaps(&pageH_ToObj(pageH)->keyRing,
                        &KeyDependEntry_TrackDirty);
  case ot_PtWorkingCopy: ;
    /* IOReqCleaning_AllocateOrWait may Yield. If so we want to do it
    before clearing OFLG_DIRTY, even though we only need the ioreq
    if the page turns out to be nonzero. */
    IORequest * ioreq = IOReqCleaning_Allocate();
    if (! ioreq) {
      if (proc_IsPFH(proc_Current()))
        return true;
      SleepOnIOReqCleaning();
    }
    // Beware, must not Yield below without freeing ioreq.

    pageH_BecomeUnwriteable(pageH);
    pageH_ClearFlags(pageH, OFLG_DIRTY);
    if (objH_IsKRO(pageH_ToObj(pageH))) {
      numKRODirtyPages--;
      assert(numKRODirtyPages >= 0);
    }

    // Is the page zero?
    kva_t pgAddr = pageH_MapCoherentRead(pageH);
    unsigned int i = PageTestZero(pgAddr);
    pageH_UnmapCoherentRead(pageH);
    if (i == EROS_PAGE_SIZE) {	// The page is zero.
      IOReq_Deallocate(ioreq);	// don't need this after all
      CleanZeroPage(pageH);
    } else {		// the page is not zero
      // While it is being cleaned, it is marked not dirty, and I/O in progress.

      ioreq->pageH = pageH;	// link page and ioreq
      pageH->ioreq = ioreq;

      LID lid = NextLogLoc();
      ObjectRange * rng = LidToRange(lid);
      assert(rng);	// it had better be mounted

      ioreq->requestCode = capros_IOReqQ_RequestType_writeRangeLoc;
      ioreq->objRange = rng;
      ioreq->rangeLoc = OIDToFrame(lid - rng->start);
      ioreq->doneFn = &IOReq_EndPageClean;

      if (objH_IsKRO(pageH_ToObj(pageH)))
        /* We know it will stay clean, so we might as well create the
        dir ent now, rather than in IOReq_EndPageClean. That also ensures
        that all dir ents have been created by checkpoint phase 3. */
        CreateLogDirEntryForNonzeroPage(pageH);

      sq_Init(&ioreq->sq);
      ioreq_Enqueue(ioreq);
    }
  }
  return false;
}

// Clean a dirty KRO page.
void
CleanAKROPage(void)
{
  assert(numKRODirtyPages);

  int count;
  for (count = objC_nPages; count-- > 0; ) {
    PageHeader * pageH;
    // The following iterates at most twice:
    while (! (pageH = CorePageIterator_Next(&cpi_KROPageCleanCursor)))
      CorePageIterator_Init(&cpi_KROPageCleanCursor);	// wrap around

    ObjectHeader * pObj = pageH_ToObj(pageH);
    switch (pObj->obType) {
    case ot_PtWorkingCopy:
    case ot_PtDataPage:
    case ot_PtLogPot:
      if (objH_GetFlags(pObj, OFLG_KRO | OFLG_DIRTY)
          == (OFLG_KRO | OFLG_DIRTY)) {
        bool b = pageH_Clean(pageH);	// may Yield
        (void)b;
        assert(!b);	// PFH should never call this
        return;
      }
      break;
    default: ;
    }
  }
}

/* This procedure is called when we want to mutate a page that is
 * Kernel Read Only.
 *
 * If the page is made writeable, this returns a reference to the same frame.
 *
 * If the current version of the page is moved to a new frame and made
 * writeable, this returns a reference to the new frame.
 * Any locks on the original page are ignored, so the caller is advised to
 * Yield.
 *
 * If the page can't be made writeable, this enqueues the current process
 * on the ObjectStallQueue and Yields. */
PageHeader *
pageH_MitigateKRO(PageHeader * old)
{
  assert(objH_GetFlags(pageH_ToObj(old),
                       OFLG_KRO | OFLG_Cleanable | OFLG_Fetching )
         == (OFLG_KRO | OFLG_Cleanable) );
  assert(pageH_GetObType(old) == ot_PtDataPage);

#ifdef OPTION_OB_MOD_CHECK // too slow?
  uint32_t cc = pageH_CalcCheck(old);
  if (pageH_ToObj(old)->check != cc)
    fatal("pageH_MitigateKRO(%#x): chk=%#x CalcCheck=%#x\n",
	  old, pageH_ToObj(old)->check, cc);
#endif

  // If the page is zero, we can clean it immediately:
  kva_t oldAddr = pageH_MapCoherentRead(old);
  unsigned int i = PageTestZero(oldAddr);
  if (i == EROS_PAGE_SIZE) {	// the page is zero
    assert(objH_IsDirty(pageH_ToObj(old)));	// can't be KRO, clean, and zero
    if (! old->ioreq) {
      // Clean this zero page now.
      pageH_UnmapCoherentRead(old);
      keyR_ProcessAllMaps(&pageH_ToObj(old)->keyRing,
                          &KeyDependEntry_TrackDirty);
      pageH_BecomeUnwriteable(old);
      pageH_ClearFlags(old, OFLG_DIRTY);
      numKRODirtyPages--;
      assert(numKRODirtyPages >= 0);
      CleanZeroPage(old);
      return old;
    }
    /* The page is being cleaned.
    This can happen if, before the start of a checkpoint,
    the page is nonzero, we start to clean it, then we dirty it
    making it zero, then a demarcation event occurs.
    In this case, even though the page is zero,
    we must not clean it now (making a log directory entry indicating
    the page is zero); if we did then when the clean finishes,
    seeing the page is clean,
    it will make a log directory entry, overwriting the zero entry. */
  }
  /* FIXME: When we try to grab a page frame, we should not clean too
  aggressively. Rather than steal potentially useful pages,
  we should put this page at the head of the list of pages to clean. */
  PageHeader * new = objC_GrabPageFrame();
  pageH_MDInitDataPage(new);
  kva_t newAddr = pageH_MapCoherentWrite(new);

  // Copy the part we know is zero (no point in re-reading it):
  memset((void *) newAddr, 0, i);
  // Copy the rest:
  memcpy((uint8_t *)newAddr + i, (uint8_t *) oldAddr + i, EROS_PAGE_SIZE - i);

  pageH_UnmapCoherentWrite(new);
  pageH_UnmapCoherentRead(old);
  pageH_SetReferenced(new);

#ifdef OPTION_OB_MOD_CHECK
  pageH_ToObj(new)->check = pageH_ToObj(old)->check;
#endif
  pageH_ToObj(new)->allocCount = pageH_ToObj(old)->allocCount;

  if (old->ioreq
      || objH_IsUserPinned(pageH_ToObj(old)) ) {
    // The old page has I/O, so it can't be moved.
    DEBUG(ckpt) printf("Moving KRO page with I/O, %#x\n", old);

    // The original page will be the working version, and
    // the new page will be the current version.
    // This is a bit like stealing the original page, and
    // fetching it into the new page.

    // Producer is moving, so invalidate any products:
    objH_InvalidateProducts(pageH_ToObj(old));

    // Set up new PageHeader:
    // Copy OFLG_*CntUsed:
    pageH_ToObj(new)->flags = pageH_ToObj(old)->flags;
    objH_ClearFlags(pageH_ToObj(new), OFLG_KRO);
    /* NOTE: We have to set the dirty bit on the current version here.
    If we don't, the page could be stolen.
    (It's unlikely to be stolen soon, but possible.)
    If it is reaccessed before the Working version is written to disk,
    the Working version won't be found, because it is not in the object hash.
    It will fetch an older version from disk. 

    Therefore the caller must ensure, before calling, that it is OK to set
    the dirty bit; that the log and directory reservations will succeed. */
    objH_SetDirtyFlag(pageH_ToObj(new));

    pageH_ToObj(old)->obType = ot_PtWorkingCopy;
    objH_Unintern(pageH_ToObj(old));

    pageH_ToObj(new)->obType = ot_PtDataPage;
    objH_InitObj(pageH_ToObj(new), pageH_ToObj(old)->oid);
    pageH_MDInitDataPage(new);

    // If would be easy to just unprepare all the keys to the old page,
    // but that would set OFLG_allocCntUsed unnecessarily.
    keyR_ObjectMoved(&pageH_ToObj(old)->keyRing, pageH_ToObj(new));
   
    return new;
  } else {
    assert(pageH_IsDirty(old));
    printf("Copying KRO page, %#x\n", old);////

    // The newly allocated page will be the working version.
    // Set up its PageHeader.
    pageH_ToObj(new)->obType = ot_PtWorkingCopy;
    objH_SetFlags(pageH_ToObj(new), OFLG_DIRTY | OFLG_KRO | OFLG_Cleanable);
    // OFLG_*CntUsed aren't used in new.
    pageH_ToObj(new)->oid = pageH_ToObj(old)->oid;

    // The original page is no longer KRO:
    objH_ClearFlags(pageH_ToObj(old), OFLG_KRO);
    // Leave OFLG_DIRTY set; see comment above for why.

    return old;
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
  uint32_t nStuck = 0;
  uint32_t nPinned = 0;
  int pass;

  DEBUG(aging) printf("objC_AgePageFrames called.\n");

#ifdef DBG_WILD_PTR
  if (dbg_wild_ptr)
    check_Consistency("Before AgePageFrames()");
#endif

  for (pass = 0; pass <= age_Steal; pass++) {
    nPinned = 0;
    nStuck = 0;
    uint32_t count;
    for (count = 0; count < objC_nPages; count++) {
      PageHeader * pageH;
      struct CorePageIterator saved_cpi = cpi_Aging;
      // The following iterates at most twice:
      while (! (pageH = CorePageIterator_Next(&cpi_Aging)))
        CorePageIterator_Init(&cpi_Aging);	// wrap around
      
      switch (pageH_GetObType(pageH)) {
      /* Some page types do not get aged: */
      case ot_PtNewAlloc:
      case ot_PtWorkingCopy:
      case ot_PtKernelUse:
      case ot_PtDevBlock:
      case ot_PtDMABlock:
      case ot_PtSecondary:
      case ot_PtFreeFrame:
      case ot_PtFreeSecondary:
        nStuck++;
        continue;

      case ot_PtTagPot:
      case ot_PtHomePot:
      case ot_PtLogPot:
        break;

      case ot_PtDataPage:
        if (! objH_GetFlags(pageH_ToObj(pageH), OFLG_Cleanable))
          continue;
  
        /* Pinned pages cannot be stolen. No point in aging them either,
        because they are recently used. */
        if (objH_IsUserPinned(pageH_ToObj(pageH))) {
          nPinned++;
          nStuck++;
          continue;
        }

        /* Since the object isn't pinned, set its transaction ID to zero
        so it won't inadvertently be considered pinned
        when objH_CurrentTransaction overflows. */
        pageH_ToObj(pageH)->userPin = 0;
        break;

      default:
        // It's a machine-dependent frame type.
        if (pageH_mdType_Aging(pageH))
          return;	// it was freed
        continue;
      }

      if (objH_GetFlags(pageH_ToObj(pageH), OFLG_Fetching) ) {
        nStuck++;
        continue;
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
        keyR_ProcessAllMaps(&pageH_ToObj(pageH)->keyRing,
                            &KeyDependEntry_TrackReferenced);
      default:
      bumpAge:
        pageH_ToObj(pageH)->objAge++;
        break;

      case age_Clean:
        /* At this stage, we initiate cleaning of the object if it is dirty. */
        if (pageH_IsDirty(pageH)) {
          /* During a checkpoint, the current version can't be cleaned;
          it must go into the next generation. */
          if (ckptIsActive()
              && ! objH_IsKRO(pageH_ToObj(pageH)) ) {
            nStuck++;
            continue;
          }
  
          if (pageH_Clean(pageH))
            continue;	// couldn't clean it and we are the PFH
        }
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
          if (proc_IsPFH(proc_Current()))
            continue;	// we are the PFH, must find another page
          // Revisit this page next time:
          cpi_Aging = saved_cpi;
          SleepOnPFHQueue(&pageH->ioreq->sq);
        }

        // Steal this frame.
        objH_InvalidateProducts(pageH_ToObj(pageH));
        keyR_UnprepareAll(&pageH_ToObj(pageH)->keyRing);
        ReleaseObjPageFrame(pageH);

        return;
      }
    }
  }

  fatal("%d stuck pages of which %d are pinned\n",
		nStuck, nPinned);
}

bool
OKToGrabPages(unsigned int numPages, bool forMT)
{
  /* We reserve KTUNE_MapTabReserve frames for free frames and mapping tables.
  This prevents a deadlock when the PFH needs a frame but a page has to be
  cleaned first (cleaning requires the PFH). 
  Mapping tables can be freed without requiring
  the user-mode Page Fault Handler (PFH). */
  if ((int)(physMem_numFreePageFrames + physMem_numMapTabPageFrames)
      - numPages >= KTUNE_MapTabReserve)
    return true;

  /* If the request is for a mapping table, that is OK: */
  if (forMT)
    return true;

  /* If it is the user-mode Page Fault Handler asking for the page(s),
  always allow it. Otherwise we risk deadlock, because stealing a page
  may require cleaning it, which requires the PFH. */
  if (proc_IsPFH(proc_Current()))
    return true;

  return false;
}

/* May Yield. */
PageHeader *
objC_GrabPageFrame2(bool forMT)
{
  PageHeader * pageH;

  while (1) {
    // Try to allocate one page.
    if (OKToGrabPages(1, forMT)) {
      pageH = physMem_AllocateBlock(1);
      if (pageH) break;
    }

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

    pageH_MDInitDataPage(pageH);

    kva_t pageAddr = pageH_MapCoherentWrite(pageH);
    kzero((void *)pageAddr, EROS_PAGE_SIZE);
    pageH_UnmapCoherentWrite(pageH);
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
    /* Copy data page only, not ot_PtDevBlock. */
    /* Object is now free of encumberance, but it wasn't an evictable
     * object, and it may be dirty. We need to find another location
     * for it.
     */
    // The following test is slow:
    // assert(pte_ObIsNotWritable(objH_ToPage(pObj)));

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
  objH_InitPresentObj(newObj, pObj->oid);
  // FIXME: Init obtype to capros_Range_ot*.

  objH_SetFlags(newObj, objH_GetFlags(pObj, OFLG_DISKCAPS)); // correct?
  /* The copy is now current. The old object is still the checkpoint
     version. */
  objH_ClearFlags(pObj, OFLG_CURRENT);

  return newObj;
}
#endif

/* Process and Depend entries are allocated before we get here.
 * Here we allocate only logDirNodes, nodes, pages, and PageHeader's.
 * Nodes and pages are allocated in equal proportions, with
 * one PageHeader per page.
 * There are twice as many logDirNodes as nodes+pages
 * (to allow for dirtying all objects in memory for each of 2 checkpoints).
 */
void
objC_Init()
{
  uint32_t availBytes;
  uint32_t allocQuanta;
  uint32_t i;
  uint32_t sizeofLogDirEntry;
  void * logDir;

  availBytes = physMem_AvailBytes(&physMem_any);
  sizeofLogDirEntry = ld_getDirEntrySize();

  DEBUG(cachealloc)
    printf("%d bytes of available storage, sizeof(Node) = %d,"
           " sizeof(PageHeader) = %d.\n"
           "sizeof(TreeNode) = %d\n",
           availBytes, sizeof(Node), sizeof(PageHeader), sizeofLogDirEntry);

  allocQuanta =
    sizeof(Node) + EROS_PAGE_SIZE + sizeof(PageHeader)
    + 2 * sizeofLogDirEntry;

  objC_nNodes = availBytes / allocQuanta;
  numLogDirEntries = objC_nNodes * 4;
 
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

  DEBUG(nodelist) CheckFreeNodeList();

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
      PageHeader * pageH = objC_AddDevicePages(pmi);
      assert(pageH);
    }
  }
  
  // Allocate logDirNodes:
  logDir = KPAtoP(void *,
          physMem_Alloc(numLogDirEntries*sizeofLogDirEntry, &physMem_any));
  kzero(logDir, numLogDirEntries*sizeofLogDirEntry);
  ld_defineDirectory(logDir);

  // Allocate pages:
  objC_AllocateUserPages();

  printf("Object cache initialized:\n"
         "  %d cached domains, %d nodes, %d pages, %d logDirNodes\n",
	 KTUNE_NCONTEXT,
	 objC_nNodes, objC_nPages, numLogDirEntries);
}
