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
#include <kerninc/ObjH-inline.h>
#include <arch-kerninc/KernTune.h>
#include <arch-kerninc/Page-inline.h>
#include <kerninc/PhysMem.h>
#include <disk/NPODescr.h>
#include <arch-kerninc/PTE.h>

#define dbg_cachealloc	0x1	/* initialization */
#define dbg_ckpt	0x2	/* migration state machine */
#define dbg_map		0x4	/* migration state machine */
#define dbg_ndalloc	0x8	/* node allocation */
#define dbg_pgalloc	0x10	/* page allocation */

/* Following should be an OR of some of the above */
#define dbg_flags   ( 0u )

#define DEBUG(x) if (dbg_##x & dbg_flags)

static void objC_AllocateUserPages(void);

static bool objC_CleanFrame2(ObjectHeader * pObj);

uint32_t objC_nNodes;
uint32_t objC_nFreeNodeFrames;
Node *objC_nodeTable;
Node *objC_firstFreeNode;

uint32_t objC_nPages;
PageHeader * objC_coreTable;

/* Now that CachedDomains is a tunable, domain cache and depend
 * entries are allocated well before we get here.  The new logic only
 * needs to worry about allocating nodes, pages, and core table
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
      keyBits_InitType(node_GetKeyAtSlot(n, j), KKT_Void);

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

#ifdef OPTION_DDB
void
objC_ddb_dump_pinned_objects()
{
  extern void db_printf(const char *fmt, ...);
  uint32_t userPins = 0;
  uint32_t nd, pg;

  for (nd = 0; nd < objC_nNodes; nd++) {
    Node * pNode = objC_GetCoreNodeFrame(nd);
    ObjectHeader * pObj = node_ToObj(pNode);
    if (objH_IsUserPinned(pObj) || node_IsKernelPinned(pNode)) {
      if (objH_IsUserPinned(pObj))
	userPins++;
      printf("node 0x%08x%08x\n",
	     (uint32_t) (pObj->oid >> 32),
	     (uint32_t) pObj->oid);
    }
  }

  for (pg = 0; pg < objC_nPages; pg++) {
    PageHeader * pageH = objC_GetCorePageFrame(pg);
    ObjectHeader * pObj = pageH_ToObj(pageH);
    if (objH_IsUserPinned(pObj) || pageH_IsKernelPinned(pageH)) {
      if (objH_IsUserPinned(pObj))
	userPins++;
      printf("page 0x%08x%08x\n",
	     (uint32_t) (pObj->oid >> 32),
	     (uint32_t) pObj->oid);
    }
  }

#ifdef OLD_PIN
  printf("User pins found: %d official count: %d\n", userPins,
	 ObjectHeader::PinnedObjectCount);
#else
  printf("User pins found: %d \n", userPins);
#endif
}

static void
objC_ddb_dump_obj(ObjectHeader * pObj)
{
  char goodSum;
#ifdef OPTION_OB_MOD_CHECK
  goodSum = (pObj->check == objH_CalcCheck(pObj)) ? 'y' : 'n';
#else
  goodSum = '?';
#endif
  printf("%#x: %s oid %#llx up:%c cr:%c ck:%c drt:%c io:%c sm:%c au:%c cu:%c\n",
	 pObj,
	 ddb_obtype_name(pObj->obType),
	 pObj->oid,
	 objH_IsUserPinned(pObj) ? 'y' : 'n',
	 objH_GetFlags(pObj, OFLG_CURRENT) ? 'y' : 'n',
	 objH_GetFlags(pObj, OFLG_CKPT) ? 'y' : 'n',
	 objH_GetFlags(pObj, OFLG_DIRTY) ? 'y' : 'n',
	 objH_GetFlags(pObj, OFLG_IO) ? 'y' : 'n',
	 goodSum,
	 objH_GetFlags(pObj, OFLG_AllocCntUsed) ? 'y' : 'n',
	 objH_GetFlags(pObj, OFLG_CallCntUsed) ? 'y' : 'n');
}

void
objC_ddb_dump_pages()
{
  uint32_t nFree = 0;
  uint32_t pg;
  
  extern void db_printf(const char *fmt, ...);

  for (pg = 0; pg < objC_nPages; pg++) {
    PageHeader * pageH = objC_GetCorePageFrame(pg);

    switch (pageH_GetObType(pageH)) {
    case ot_PtFreeFrame:
    case ot_PtSecondary:
      nFree++;
      break;

    case ot_PtNewAlloc:
      assert(false);	// should not have at this time

    case ot_PtDataPage:
    case ot_PtDevicePage:
    {
      ObjectHeader * pObj = pageH_ToObj(pageH);
      objC_ddb_dump_obj(pObj);
      break;
    }

    case ot_PtTagPot:
    case ot_PtObjPot:
    {
      ObjectHeader * pObj = pageH_ToObj(pageH);
      printf("%#x: %s oid %#llx\n",
	 pObj,
	 ddb_obtype_name(pObj->obType),
	 pObj->oid);
      break;
    }

    case ot_PtKernelHeap:
    case ot_PtDMABlock:
    case ot_PtDMASecondary:
      printf("%#x: %s\n",
             pageH,
             ddb_obtype_name(pageH_GetObType(pageH)) );
      break;
      
    default:
      printf("%#x: %s ",
             pageH,
             ddb_obtype_name(pageH_GetObType(pageH)) );
      pageH_mdType_dump_pages(pageH);
      break;
    }
  }

  printf("Total of %d pages, of which %d are free\n", objC_nPages, nFree);
}

void
objC_ddb_dump_nodes()
{
  uint32_t nFree = 0;
  uint32_t nd = 0;
  
  extern void db_printf(const char *fmt, ...);

  for (nd = 0; nd < objC_nNodes; nd++) {
    ObjectHeader *pObj = node_ToObj(objC_GetCoreNodeFrame(nd));

    if (pObj->obType == ot_NtFreeFrame) {
      nFree++;
      continue;
    }

    if (pObj->obType > ot_NtLAST_NODE_TYPE)
      fatal("Node @0x%08x: object type %d is broken\n", pObj,
		    pObj->obType); 
    objC_ddb_dump_obj(pObj);
  }

  printf("Total of %d nodes, of which %d are free\n", objC_nNodes, nFree);
}
#endif

/* Queue for activitys that are waiting for available page frames: */
DEFQUEUE(PageAvailableQueue);

/* May Yield. */
static void
objC_AgeNodeFrames()
{
  static uint32_t curNode = 0;
  uint32_t nStuck = 0;
  uint32_t nPinned = 0;
  int pass = 0;
  uint32_t count = 0;
  Node *pObj = 0;

#ifdef DBG_WILD_PTR
  if (dbg_wild_ptr)
    check_Consistency("Before AgeNodeFrames()");
#endif

  for (pass = 0; pass <= age_PageOut; pass++) {
    nPinned = 0;
    nStuck = 0;
    for (count = 0; count < objC_nNodes; count++, curNode++) {
      if (curNode >= objC_nNodes)
	curNode = 0;

      pObj = objC_GetCoreNodeFrame(curNode);
    
      assert(node_ToObj(pObj)->objAge <= age_PageOut);
    
      assert (objH_GetFlags(node_ToObj(pObj), OFLG_IO) == 0);
    
      if (objH_IsUserPinned(node_ToObj(pObj))
          || node_IsKernelPinned(pObj) ) {
	nPinned++;
	nStuck++;
	continue;
      }

      /* Since the object isn't pinned, set its transaction ID to zero
      so it won't inadvertently be considered pinned
      when objH_CurrentTransaction overflows. */
      node_ToObj(pObj)->userPin = 0;
    
      if (pObj->node_ObjHdr.obType == ot_NtFreeFrame)
	continue;

#ifdef OPTION_DISKLESS
      /* In the diskless kernel, dirty objects are only removed as a
       * result of being destroyed. Otherwise, they linger pointlessly
       * in a kind of Camus-esque twilight of nonexistence.
       */
      if (pObj->IsDirty())
	continue;
#endif
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
    
      if (pObj->node_ObjHdr.obType == ot_NtProcessRoot ||
	  pObj->node_ObjHdr.obType == ot_NtRegAnnex ||
	  pObj->node_ObjHdr.obType == ot_NtKeyRegs) {
	  nStuck++;
          continue;
      }
      
      /* THIS ALGORITHM IS NOT THE SAME AS THE PAGE AGEING ALGORITHM!!!
       * 
       * While nodes are promptly cleanable (just write them to a log
       * pot and let the page cleaner work on them), there is still no
       * sense tying up log I/O bandwidth writing the ones that are
       * highly active.  We therefore invalidate them, but we don't try
       * to write them until they hit the ageout age.
       */
    
      if (node_ToObj(pObj)->objAge == age_Invalidate)
	/* Clean the frame, but do not invalidate products yet,
	 * because the object may get resurrected.
	 */
	objC_CleanFrame1(node_ToObj(pObj));

      if (node_ToObj(pObj)->objAge < age_PageOut) {
	node_ToObj(pObj)->objAge++;

	continue;
      }
    
      DEBUG(ckpt)
	dprintf(false, "Ageing out node=0x%08x oty=%d dirty=%c oid=0x%08x%08x\n",
			pObj, pObj->node_ObjHdr.obType,
			(objH_IsDirty(node_ToObj(pObj)) ? 'y' : 'n'),
			(uint32_t) (pObj->node_ObjHdr.oid >> 32),
			(uint32_t) (pObj->node_ObjHdr.oid));

      objC_CleanFrame1(node_ToObj(pObj));
      objC_CleanFrame2(node_ToObj(pObj));

      /* Make sure that the next process that wants a frame is
       * unlikely to choose the same node frame:
       */
      curNode++;

      assert (!objH_IsDirty(node_ToObj(pObj)));
      assert(keyR_IsEmpty(&pObj->node_ObjHdr.keyRing));
    
      /* Remove this node from the cache and return it to the free node
       * list: */
      ReleaseNodeFrame(pObj);

#if defined(TESTING_AGEING) && 0
      dprintf(true, "AgeNodeFrame(): Object evicted\n");
#endif
#ifdef DBG_WILD_PTR
      if (dbg_wild_ptr)
	check_Consistency("After AgeNodeFrames()");
#endif

      return;
    }
  }

  fatal("%d stuck nodes of which %d are pinned\n",
		nStuck, nPinned);
}

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
    objC_CleanFrame1(pageH_ToObj(pageH));
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

/* Clean out the node/page frame, but do not remove it from memory. */
/* pObj->obType must be node or ot_PtDataPage. */
/* May Yield. */
void
objC_CleanFrame1(ObjectHeader *pObj)
{
  /* If this object is due to go out and actively involved in I/O,
   * then we are still waiting for the effects of the last call to
   * complete, and we should put the current activity to sleep on this
   * object:
   */
  if (objH_GetFlags(pObj, OFLG_IO)) {
    if (! act_Current() )
      // act_Current() is generally only zero during initialization.
      fatal("Insufficient memory for initialization\n");
    else {
      act_SleepOn(ObjectStallQueueFromObHdr(pObj));
      act_Yield();
    }
    assert (false);
  }

  /* Clean up the object we are reclaiming so we can free it: */

  keyR_UnprepareAll(&pObj->keyRing);	/* This zaps any PTE's as a side effect. */
  //// Use keyR_UnmapAll instead?
}

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

/* OBJECT AGING POLICY:
 * 
 * Objects are brought in as NewBorn objects, and age until they reach
 * the Invalidate generation.  At that point all outstanding keys are
 * deprepared.  If they make it to the PageOut generation we kick them
 * out of memory (writing if necessary).
 * 
 * When an object is prepared, we conclude that it is an important
 * object, and promote it back to the NewBorn generation.
 */

/* The page frame ager is one of the places where a bunch of sticky
 * issues all collide.  Some design principles that we would LIKE to
 * satisfy:
 * 
 *  1. Pages should be aged without regard to cleanliness.  The issue
 *     is that ageing which favors reclaiming clean pages will tend to
 *     reclaim code pages.
 * 
 *  2. The CPU should not sit idle when frames are reclaimable.  This
 *     conflicts with principle (1), since any policy that satisfies
 *     (1) implies stalling somewhere.
 * 
 *  3. Real-time and non-real-time processes should not have resource
 *     conflicts imposed by the ager.  This means both on the frames
 *     themselves (easy to solve by coloring) and on the I/O Request
 *     structures (which is much harder).
 * 
 * I can see no way to satisfy all of these constraints at once.  For
 * that matter, I can't see any solution that always satisfies (1) and
 * (2) simultaneously.  The problem is that cleaning takes time which
 * is many orders of magnitude longer than a context switch.
 * 
 * The best solution I have come up with is to try to ameliorate
 * matters by impedance matching.  Under normal circumstances, the
 * ager is the only generator of writes in the system.  During
 * stabilization and migration, any write it may do is more likely to
 * help than hurt.  These plus the journaling logic are ALL of the
 * sources of writes in the system, and we know that object reads and
 * writes can never conflict (if the object is already in core to be
 * written, then we won't be reading it from the disk).
 * 
 * This leaves two concerns:
 * 
 *   1. Ensuring that there is no contention on the I/O request pool.
 *   2. Ensuring that there is limited contention for disk bandwidth.
 * 
 * Since reads and writes do not conflict, (1) can be resolved by
 * splitting the I/O request pools by read/write (not currently
 * implemented).  If this split is implemented, (2) can be
 * accomplished by the simple expedient of restricting the relative
 * I/O request pool sizes.
 * 
 * In an attempt to limit the impact of delays due to dirty objects,
 * the ager attempts to write objects long before they are candidates
 * for reclamation (i.e. we run a unified ageing and cleaning policy
 * -- this may want to be revisited in the future, as if the outbound
 * I/O bandwidth is available we might as well use it).
 * 
 * The ageing policy proceeds as follows: when ageing is invoked, run
 * the ager until one of the following happens:
 * 
 *   1. We find a page to reclaim
 *   2. We find a dirty page due to be written.
 * 
 * If (2) occurs, initiate the write and attempt to find a bounded
 * number (currently 5) of additional writes to initiate.
 */

void
objC_AgePageFrames(void)
{
  static uint32_t curPage = 0;

#ifdef DBG_WILD_PTR
  if (dbg_wild_ptr)
    check_Consistency("Before AgePageFrames()");
#endif

  while (1) {
    PageHeader * pageH = objC_GetCorePageFrame(curPage);
    
    /* Some page types do not get aged: */
    switch (pageH_GetObType(pageH)) {
    case ot_PtNewAlloc:
    case ot_PtDevicePage:
    case ot_PtKernelHeap:
    case ot_PtDMABlock:
    case ot_PtDMASecondary:
    case ot_PtFreeFrame:
    case ot_PtSecondary:
      goto nextPage;

    case ot_PtTagPot:
    case ot_PtObjPot:
      assert(!"complete");	// check this case
      break;

    case ot_PtDataPage:
      if (! objH_GetFlags(pageH_ToObj(pageH), OFLG_Cleanable))
        goto nextPage;
      break;

    default:
      if (pageH_mdType_AgingExempt(pageH))
        goto nextPage;
    }
  
    /* Some pages cannot be aged because they are active or pinned: */
    if (objH_IsUserPinned(pageH_ToObj(pageH)))
      goto nextPage;

    /* Since the object isn't pinned, set its transaction ID to zero
    so it won't inadvertently be considered pinned
    when objH_CurrentTransaction overflows. */
    pageH_ToObj(pageH)->userPin = 0;

    if (pageH_IsKernelPinned(pageH)
        || objH_GetFlags(pageH_ToObj(pageH), OFLG_Fetching) )
      goto nextPage;

    switch (pageH_ToObj(pageH)->objAge) {
    case age_Invalidate:
      /* First, we check whether the object is really not recently used.
       * Normally, any reference to the object sets its objAge to age_NewBorn
       * (in objH_SetReferenced). 
       * However, references through mapping table entries happen without
       * benefit of updating objAge.
       * So at this stage we invalidate the mapping table entries. If the
       * object is referenced,
       * the entry will be rebuilt and the age updated then.
       */
      if (pageH_GetObType(pageH) > ot_PtLAST_COMMON_PAGE_TYPE) {
        // It's a machine-dependent frame type.
        if (pageH_mdType_AgingClean(pageH)) {
          curPage++;	// it was freed
          return;
        }
      } else {
        objC_CleanFrame1(pageH_ToObj(pageH));
      }
      goto bumpAge;

    case age_Clean:
      /* At this stage, we initiate cleaning of the object if it is dirty. */
      assert(incomplete);
      goto bumpAge;

    case age_Steal:
      /* Now it's time to evict the object and steal the frame. */
      assert(incomplete);
      break;

    default:
    bumpAge:
      pageH_ToObj(pageH)->objAge++;
      break;
    }
    if (pageH_ToObj(pageH)->objAge == age_PageOut) {
      if (pageH_GetObType(pageH) > ot_PtLAST_COMMON_PAGE_TYPE) {
        // It's a machine-dependent frame type.
        if (! pageH_mdType_AgingSteal(pageH))
          goto nextPage;	// couldn't steal it
      } else {
        objC_CleanFrame1(pageH_ToObj(pageH));
        if (objC_CleanFrame2(pageH_ToObj(pageH)) == false)
          goto nextPage;
  
        assert(!pageH_IsDirty(pageH));

        /* Remove this page from the cache and return it to the free page
         * list:
         */
        assert(keyR_IsEmpty(&pageH_ToObj(pageH)->keyRing));
        ReleaseObjPageFrame(pageH);
      }

      curPage++;
      return;
    }
    
    pageH_ToObj(pageH)->objAge++;

    if (pageH_ToObj(pageH)->objAge == age_Invalidate) {
    }

  nextPage:
    if (++curPage >= objC_nPages)
      curPage = 0;
  }
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

#if 0
void
ObjectCache::RequirePageFrames(uint32_t n)
{
  while (nFreeNodeFrames < n)
    AgePageFrames();
}
#endif

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

Node *
objC_GrabNodeFrame()
{
  Node *pNode = 0;

  if (objC_firstFreeNode == 0)
    objC_AgeNodeFrames();
  
  assert(objC_firstFreeNode);
  assert(objC_nFreeNodeFrames);
  
  pNode = objC_firstFreeNode;
  objC_firstFreeNode = (Node *) objC_firstFreeNode->node_ObjHdr.prep_u.nextFree;
  objC_nFreeNodeFrames--;

  ObjectHeader * const pObj = &pNode->node_ObjHdr;
  assert(pObj->obType == ot_NtFreeFrame);

  /* Rip it off the hash chain, if need be: */
  objH_Unintern(pObj);	/* Should it ever be interned? */
  assert(keyR_IsEmpty(&pObj->keyRing));
  kzero(pObj, sizeof(ObjectHeader));

  pNode->node_ObjHdr.obType = ot_NtUnprepared;

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

/* Release a page that has an ObjectHeader. */
void
ReleaseObjPageFrame(PageHeader * pageH)
{
  DEBUG(ndalloc)
    printf("ReleaseObjPageFrame pageH=0x%08x\n", pageH);

  assert(pageH);
  
  /* Not certain that *anything* handed to ReleaseObjPageFrame() should be
   * dirty, but...
   */
  if (objH_GetFlags(pageH_ToObj(pageH), OFLG_CKPT))
    assert (!pageH_IsDirty(pageH));

#ifndef NDEBUG
  assert(pte_ObIsNotWritable(pageH));
#endif

  objH_Unintern(pageH_ToObj(pageH));
    
  ReleasePageFrame(pageH);
}

/* Put a page on the free list. */
/* Caller must have already removed all previous entanglements. */
void
ReleasePageFrame(PageHeader * pageH)
{
  physMem_FreeBlock(pageH, 1);

  sq_WakeAll(&PageAvailableQueue, false);
}

void
ReleaseNodeFrame(Node * pNode)
{
  DEBUG(ndalloc)
    printf("ReleaseNodeFrame node=0x%08x\n", pNode);

  assert(pNode);

  /* Not certain that *anything* handed to ReleaseNodeFrame() should be
   * dirty, but...
   */
  if (objH_GetFlags(node_ToObj(pNode), OFLG_CKPT))
    assert(!objH_IsDirty(node_ToObj(pNode)));

#ifndef NDEBUG
  uint32_t i = 0;
  for (i = 0; i < EROS_NODE_SIZE; i++)
    assertex (pNode, keyBits_IsUnprepared(&pNode->slot[i]));
#endif

  objH_Unintern(node_ToObj(pNode));
    
  node_ToObj(pNode)->obType = ot_NtFreeFrame;

  node_ToObj(pNode)->prep_u.nextFree = node_ToObj(objC_firstFreeNode);
  objC_firstFreeNode = pNode;
  objC_nFreeNodeFrames++;
}
