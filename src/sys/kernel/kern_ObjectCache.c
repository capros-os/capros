/*
 * Copyright (C) 1998, 1999, Jonathan S. Shapiro.
 * Copyright (C) 2005, 2006, 2007, Strawberry Development Group.
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
#include <arch-kerninc/KernTune.h>
#include <kerninc/PhysMem.h>
#include <disk/PagePot.h>

#define MAX_SOURCE 16

#define dbg_cachealloc	0x1	/* initialization */
#define dbg_ckpt	0x2	/* migration state machine */
#define dbg_map		0x4	/* migration state machine */
#define dbg_ndalloc	0x8	/* node allocation */
#define dbg_pgalloc	0x10	/* page allocation */
#define dbg_obsrc	0x20	/* addition of object sources */
#define dbg_findfirst	0x40	/* finding first subrange */

/* Following should be an OR of some of the above */
#define dbg_flags   ( 0u )

#define DEBUG(x) if (dbg_##x & dbg_flags)

static void objC_GrabThisPageFrame(PageHeader *);

static void objC_CleanFrame1(ObjectHeader * pObj);
static bool objC_CleanFrame2(ObjectHeader * pObj);

struct PageInfo {
  uint32_t nPages;
  uint32_t basepa;
  PageHeader * firstObHdr;
};

uint32_t objC_nNodes;
uint32_t objC_nFreeNodeFrames;
Node *objC_nodeTable;
Node *objC_firstFreeNode;
PageHeader * objC_firstFreePage;

uint32_t objC_nPages;
uint32_t objC_nFreePageFrames;
uint32_t objC_nReservedIoPageFrames = 0;
uint32_t objC_nCommittedIoPageFrames = 0;
PageHeader * objC_coreTable;

/* initializes nodeTable. replaces call to Node constructor */
static Node *
objC_InitNodeTable(int num)
{
  int i = 0;
  uint32_t j = 0;

  Node *temp = (Node *)KPAtoP(void *, physMem_Alloc(num*sizeof(Node), &physMem_any));
  for (i = 0; i < num; i++) {
    Node *n = &temp[i];
    /* Possibly: Init keys the first time node is used. */
    for (j = 0; j < EROS_NODE_SIZE; j++)
      keyBits_InitToVoid(&n->slot[j]);
    keyR_ResetRing(&n->node_ObjHdr.keyRing);
    n->node_ObjHdr.flags = 0;
    n->node_ObjHdr.userPin = 0;
    n->node_ObjHdr.obType = ot_NtFreeFrame;
  }

  return temp;
}

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
  
  objC_nodeTable = objC_InitNodeTable(objC_nNodes);

  DEBUG(cachealloc)
    printf("Allocated Nodes: 0x%x at 0x%08x\n",
		   sizeof(Node[objC_nNodes]), objC_nodeTable);

  /* Drop all the nodes on the free node list: */
  for (i = 0; i < objC_nNodes; i++) {
    /* object type is set in constructor... */
    assert (objC_nodeTable[i].node_ObjHdr.obType == ot_NtFreeFrame);
    objC_nodeTable[i].node_ObjHdr.next = DOWNCAST(&objC_nodeTable[i+1], ObjectHeader);
  }
  
  objC_nodeTable[objC_nNodes - 1].node_ObjHdr.next = 0;
  objC_firstFreeNode = &objC_nodeTable[0];

  objC_nFreeNodeFrames = objC_nNodes;

  Depend_InitKeyDependTable(objC_nNodes);

  DEBUG(cachealloc)
    printf("%d bytes of available storage after key dep tbl alloc.\n", availBytes);

  objC_AllocateUserPages();

  DEBUG(cachealloc)
    printf("%d cached domains, %d nodes, %d pages\n",
		   KTUNE_NCONTEXT,
		   objC_nNodes, objC_nPages);

  objC_nFreePageFrames = objC_nPages;
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

void
objC_AllocateUserPages()
{
  /* When we get here, we are allocating the last of the core
   * memory. take it all.
   *
   * A tacit assumption is made in this code that any space allocated
   * within a given memory region has been allocated from top or
   * bottom. Any remaining space is assumed to be a single, contiguous
   * hole.  There is an assertion check that should catch cases where
   * this assumption is false, whereupon some poor soul will need to
   * fix it.
   */
  uint32_t j = 0;
  uint32_t i = 0;
  unsigned rgn = 0;
  kpsize_t np;
    
  objC_nPages = physMem_AvailPages(&physMem_pages);

  objC_coreTable = KPAtoP(PageHeader *,
             physMem_Alloc(objC_nPages*sizeof(PageHeader), &physMem_any));
  assert(objC_coreTable);

  for (j = 0; j < objC_nPages; j++) {
    PageHeader * temp = &objC_coreTable[j];
    temp->kt_u.free.obType = ot_PtFreeFrame;
  }

  DEBUG(pgalloc)
    printf("Allocated Page Headers: 0x%x at 0x%08x\n",
		   sizeof(PageHeader[objC_nPages]), objC_coreTable);

  /* Block the pages by class, allocate them, and recompute nPages.
   * Link all pages onto the appropriate free list:
   */
    
  /* On the way through this loop, nPages holds the total number
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

    np = physMem_ContiguousPages(&xmc);
    /* See the comment at the top of this function if this assertion
     * fails! */
    assert(np == physMem_AvailPages(&xmc));

#ifdef TESTING_AGEING
    np = min (np, 50);
#endif

    pmi->nPages = np;
    pmi->basepa =
      (uint32_t) physMem_Alloc(EROS_PAGE_SIZE * np, &xmc);
    pmi->firstObHdr = &objC_coreTable[objC_nPages];

    /* See the comment at the top of this function if this assertion
     * fails! */
    assert(physMem_AvailPages(&xmc) == 0);

    objC_nPages += np;
  }

#if 0
  printf("nPages = %d (0x%x)\n", nPages, nPages);

  halt();
#endif

  /* Populate all of the page address pointers in the core table
   * entries:
   */

  {
    PageHeader * pObHdr = 0;
    kpa_t framePa;
    uint32_t pg = 0;
    
    for (rgn = 0; rgn < physMem_nPmemInfo; rgn++) {
      PmemInfo *pmi= &physMem_pmemInfo[rgn];
      if (pmi->type != MI_MEMORY)
	continue;

      framePa = pmi->basepa;
      pObHdr = pmi->firstObHdr;
      
      for (pg = 0; pg < pmi->nPages; pg++) {
	pObHdr->pageAddr = PTOV(framePa);
	framePa += EROS_PAGE_SIZE;
	pObHdr++;
      }
    }
  }

  /* Link all of the resulting core table entries onto the free page list: */
  
  for (i = 0; i < objC_nPages; i++) {
    objC_coreTable[i].kt_u.free.obType = ot_PtFreeFrame;	/* redundant? */
    objC_coreTable[i].kt_u.free.next = &objC_coreTable[i+1];
  }

  objC_coreTable[objC_nPages - 1].kt_u.free.next = 0;
  objC_firstFreePage = &objC_coreTable[0];
}

void
objC_AddDevicePages(PmemInfo *pmi)
{
  uint32_t j = 0;
  uint32_t pg = 0;
  kpa_t framePa;
  PageHeader * pObHdr = 0;

  /* Not all BIOS's report a page-aligned start address for everything. */
  pmi->basepa = (pmi->base & ~EROS_PAGE_MASK);
  pmi->nPages = (pmi->bound - pmi->basepa) / EROS_PAGE_SIZE;

  pmi->firstObHdr = MALLOC(PageHeader, pmi->nPages);
  for (j = 0; j < pmi->nPages; j++) {
    PageHeader * temp = &pmi->firstObHdr[j];
    temp->kt_u.free.obType = ot_PtFreeFrame;
  }

  framePa = pmi->basepa;
  pObHdr = pmi->firstObHdr;
      
  for (pg = 0; pg < pmi->nPages; pg++) {
    pObHdr->pageAddr = PTOV(framePa);
    framePa += EROS_PAGE_SIZE;
    pObHdr++;
  }

  /* Note that these pages do NOT go on the free list! */
}

ObjectHeader*
objC_OIDtoObHdr(uint32_t cdaL/*cdaLo*/, uint16_t cdaH/*cdaHi*/)
{
  /* FIX: implement me */
  fatal("OIDtoObHdr unimplemented!\n");
  return 0;
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

PageHeader *
objC_PhysPageToObHdr(kpa_t pagepa)
{
  unsigned rgn = 0;
  kva_t startpa;
  kva_t endpa;
  uint32_t pageNo;
    
  for (rgn = 0; rgn < physMem_nPmemInfo; rgn++) {
    PmemInfo *pmi= &physMem_pmemInfo[rgn];
    if (pmi->type != MI_MEMORY && pmi->type != MI_DEVICEMEM && pmi->type != MI_BOOTROM)
      continue;

    startpa = pmi->basepa;
    endpa = startpa + pmi->nPages * EROS_PAGE_SIZE;

    if (pagepa < startpa || pagepa >= endpa)
      continue;

    assert (pagepa >= pmi->basepa);

    pageNo = (pagepa - pmi->basepa) / EROS_PAGE_SIZE;

    return &pmi->firstObHdr[pageNo];
  }

  return 0;
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

void
objC_ddb_dump_pages()
{
  uint32_t nFree = 0;
  uint32_t pg = 0;
  char goodSum;
  
  extern void db_printf(const char *fmt, ...);

  for (pg = 0; pg < objC_nPages; pg++) {
    PageHeader * pageH = objC_GetCorePageFrame(pg);

    switch (pageH_GetObType(pageH)) {
    case ot_PtFreeFrame:
      nFree++;
      break;

    case ot_PtNewAlloc:
    case ot_PtKernelHeap:
      break;

    case ot_PtDataPage:
    case ot_PtDevicePage:
    {
      ObjectHeader * pObj = pageH_ToObj(pageH);
#ifdef OPTION_OB_MOD_CHECK
      goodSum = (pObj->check == objH_CalcCheck(pObj)) ? 'y' : 'n';
#else
      goodSum = '?';
#endif
      printf("%02d: %s oid 0x%08x%08x up:%c cr:%c ck:%c drt:%c%c io:%c sm:%c dc:%c\n",
	   pg,
	   ddb_obtype_name(pObj->obType),
	   (uint32_t) (pObj->oid >> 32),
	   (uint32_t) (pObj->oid),
	   objH_IsUserPinned(pObj) ? 'y' : 'n',
	   objH_GetFlags(pObj, OFLG_CURRENT) ? 'y' : 'n',
	   objH_GetFlags(pObj, OFLG_CKPT) ? 'y' : 'n',
	   objH_GetFlags(pObj, OFLG_DIRTY) ? 'y' : 'n',
	   objH_GetFlags(pObj, OFLG_REDIRTY) ? 'y' : 'n',
	   objH_GetFlags(pObj, OFLG_IO) ? 'y' : 'n',
	   goodSum,
	   objH_GetFlags(pObj, OFLG_DISKCAPS) ? 'y' : 'n');
      break;
    }
      
    default:
      printf("%02d: %s ",
             pg,
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
  char goodSum;
  
  extern void db_printf(const char *fmt, ...);

  for (nd = 0; nd < objC_nNodes; nd++) {
    ObjectHeader *pObj = DOWNCAST(objC_GetCoreNodeFrame(nd), ObjectHeader);

    if (pObj->obType == ot_NtFreeFrame) {
      nFree++;
      continue;
    }

    if (pObj->obType > ot_NtLAST_NODE_TYPE)
      fatal("Node @0x%08x: object type %d is broken\n", pObj,
		    pObj->obType); 
    
#ifdef OPTION_OB_MOD_CHECK
    goodSum = (pObj->check == objH_CalcCheck(pObj)) ? 'y' : 'n';
#else
    goodSum = '?';
#endif
    printf("%02d: %s oid 0x%08x%08x up:%c cr:%c ck:%c drt:%c%c io:%c sm:%c dc:%c\n",
	   nd,
	   ddb_obtype_name(pObj->obType),
	   (uint32_t) (pObj->oid >> 32),
	   (uint32_t) (pObj->oid),
	   objH_IsUserPinned(pObj) ? 'y' : 'n',
	   objH_GetFlags(pObj, OFLG_CURRENT) ? 'y' : 'n',
	   objH_GetFlags(pObj, OFLG_CKPT) ? 'y' : 'n',
	   objH_GetFlags(pObj, OFLG_DIRTY) ? 'y' : 'n',
	   objH_GetFlags(pObj, OFLG_REDIRTY) ? 'y' : 'n',
	   objH_GetFlags(pObj, OFLG_IO) ? 'y' : 'n',
	   goodSum,
	   objH_GetFlags(pObj, OFLG_DISKCAPS) ? 'y' : 'n');
  }

  printf("Total of %d nodes, of which %d are free\n", objC_nNodes, nFree);
}
#endif

/* Queue for activitys that are waiting for available page frames: */
static DEFQUEUE(PageAvailableQueue);

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
    
      assert(pObj->objAge <= age_PageOut);
    
      assert (objH_GetFlags(DOWNCAST(pObj, ObjectHeader), OFLG_IO) == 0);
    
      if (objH_IsUserPinned(DOWNCAST(pObj, ObjectHeader))
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
       * If a node has an active context structure, it is not a
       * candidate for ageing.  This pins a (relatively) small number
       * of nodes in memory, but guarantees that the invoker and the
       * invokee in a given invocation will not get aged out.
       * Simultaneously, it guarantees that on invocation of a process
       * capability the process context will not go out due to ageing.
       * 
       * The reason this is an issue is because we short-circuit the
       * pin of the constituents in Process::Prepare() by testing for
       * a valid saveArea.
       * 
       * In SMP implementations we will need to pin contexts in the
       * same way that we pin nodes, in order to ensure that one
       * processor does not remove a context that is active on
       * another.
       * 
       * One unfortunate aspect of this design rule is that it becomes
       * substantially harder to test the ageing mechanism.  This rule
       * has the effect of increasing the minimum number of nodes
       * required to successfully operate the system.  This issue can
       * be eliminated once we start pinning process structures.
       *
       * FIX: This is actually not good, as stale processes can
       * potentially liver in the context cache for quite a long
       * time. At some point, we need to introduce a context cache
       * cleaning mechanism so that the non-persistent kernel will
       * have one. Hmm. I suppose that context cache flush can
       * continue to be triggered by the Checkpoint timer even in the
       * non-persistent kernel.
       */
    
      if (pObj->node_ObjHdr.obType == ot_NtProcessRoot ||
	  pObj->node_ObjHdr.obType == ot_NtRegAnnex ||
	  pObj->node_ObjHdr.obType == ot_NtKeyRegs) {


        /* Activity::Current() changed to act_Current() */
	if (pObj->node_ObjHdr.prep_u.context &&
	    ((pObj->node_ObjHdr.prep_u.context == act_Current()->context) ||
	     (inv_IsActive(&inv) && (inv.invokee == pObj->node_ObjHdr.prep_u.context)))) {
	  nStuck++;
	  pObj->objAge = age_LiveProc;
	}

      }
      
      /* THIS ALGORITHM IS NOT THE SAME AS THE PAGE AGEING ALGORITHM!!!
       * 
       * While nodes are promptly cleanable (just write them to a log
       * pot and let the page cleaner work on them), there is still no
       * sense tying up log I/O bandwidth writing the ones that are
       * highly active.  We therefore invalidate them, but we don't try
       * to write them until they hit the ageout age.
       */
    
      if (pObj->objAge == age_Invalidate)
	/* Clean the frame, but do not invalidate products yet,
	 * because the object may get resurrected.
	 */
	objC_CleanFrame1(node_ToObj(pObj));

      if (pObj->objAge < age_PageOut) {
	pObj->objAge++;

	continue;
      }
    
      DEBUG(ckpt)
	dprintf(false, "Ageing out node=0x%08x oty=%d dirty=%c oid=0x%08x%08x\n",
			pObj, pObj->node_ObjHdr.obType,
			(objH_IsDirty(DOWNCAST(pObj, ObjectHeader)) ? 'y' : 'n'),
			(uint32_t) (pObj->node_ObjHdr.oid >> 32),
			(uint32_t) (pObj->node_ObjHdr.oid));

      objC_CleanFrame1(node_ToObj(pObj));
      objC_CleanFrame2(node_ToObj(pObj));

      /* Make sure that the next process that wants a frame is
       * unlikely to choose the same node frame:
       */
      curNode++;

      assert (!objH_IsDirty(DOWNCAST(pObj, ObjectHeader)));
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

  objH_TransLock(pObj);

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
    newPage->objAge = age_NewBorn;	/* FIX: is this right? */
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
    newNode->objAge = age_NewBorn;	/* FIX: is this right? */
  }

  newObj->oid = pObj->oid;
  newObj->allocCount = pObj->allocCount;

  /* The copy is now current. The old object is still the checkpoint
     version. */
  objH_SetFlags(newObj, OFLG_CURRENT);
  objH_ClearFlags(pObj, OFLG_CURRENT);

  assert(objH_GetFlags(pObj, OFLG_IO) == 0);
  objH_SetFlags(newObj, objH_GetFlags(pObj, OFLG_DISKCAPS));
  newObj->ioCount = 0;

  newObj->obType = pObj->obType;
#ifdef OPTION_OB_MOD_CHECK
  newObj->check = objH_CalcCheck(newObj);
#endif
  assert(keyR_IsEmpty(&newObj->keyRing));

  objH_Intern(newObj);

  return newObj;
}

/* Evict the current resident of a page frame. This is called
 * when we need to claim a particular physical page frame.
 * It is satisfactory to accomplish this by grabbing some
 * other frame and moving the object to it. 
 */
/* This procedure may Yield. */
bool
objC_EvictFrame(PageHeader * pObj)
{
  DEBUG(ndalloc)
    printf("objC_EvictFrame obj=0x%08x type=%d\n", pObj, pageH_GetObType(pObj));

  switch (pageH_GetObType(pObj)) {
  case ot_PtFreeFrame:
    break;

  case ot_PtKernelHeap:
    return false;	// not implemented yet: FIXME

  case ot_PtNewAlloc:
    assert(false);	// should not have this now

  case ot_PtDevicePage: // can't evict this
    return false;

  case ot_PtDataPage:
    objC_CleanFrame1(pageH_ToObj(pObj));
    if (!objC_CleanFrame2(pageH_ToObj(pObj))) {
      (void) objC_CopyObject(pageH_ToObj(pObj));
  
      /* Since we could not write the old frame out, we assume that it
       * is not backed by anything. In this case, the right thing to do
       * is to simply mark the old one clean, turn off it's checkpoint
       * bit if any (it's not writable anyway), and allow ReleaseFrame()
       * to release it.
       */

      pageH_ClearFlags(pObj, OFLG_CKPT | OFLG_DIRTY);
    }
    assert(keyR_IsEmpty(&pageH_ToObj(pObj)->keyRing));
    ReleaseObjPageFrame(pObj);
    break;

  default:
    pageH_mdType_EvictFrame(pObj);
    break;
  }

  // Unlink from free list.
  PageHeader * * pp = &objC_firstFreePage;
  while (*pp != pObj) {
    assert(*pp);	// else not found in list
    pp = &(*pp)->kt_u.free.next;
  }
  (*pp) = pObj->kt_u.free.next;

  objC_GrabThisPageFrame(pObj);
  return true;
}

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
    if (objC_IsRemovable(pObj) == false)
      return false;

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

static void
objC_AgePageFrames()
{
  static uint32_t curPage = 0;
  uint32_t count = 0;

  uint32_t nStuck = 0;
  uint32_t nPasses = 200;	/* arbitrary - catches kernel bugs and */
				/* dickhead kernel hackers (like me) */

#ifdef DBG_WILD_PTR
  if (dbg_wild_ptr)
    check_Consistency("Before AgePageFrames()");
#endif

  do {
    for (count = 0; count < objC_nPages; count++, curPage++) {
      if (curPage >= objC_nPages)
	curPage = 0;

      PageHeader * pObj = objC_GetCorePageFrame(curPage);
      
      /* Some page types do not get aged: */
      switch (pageH_GetObType(pObj)) {
      case ot_PtNewAlloc:
      case ot_PtDevicePage:
      case ot_PtKernelHeap:
	nStuck++;
      case ot_PtFreeFrame:
	continue;

      case ot_PtDataPage:
        break;

      default:
        if (pageH_mdType_AgingExempt(pObj))
          continue;
      }
	  
      /* Some pages cannot be aged because they are active or pinned: */
      if (objH_IsUserPinned(pageH_ToObj(pObj)))
	continue;

      /* Since the object isn't pinned, set its transaction ID to zero
      so it won't inadvertently be considered pinned
      when objH_CurrentTransaction overflows. */
      pageH_ToObj(pObj)->userPin = 0;

      if (pageH_IsKernelPinned(pObj))
	continue;
    
      if (pObj->objAge == age_PageOut) {
        if (pageH_GetObType(pObj) > ot_PtLAST_COMMON_PAGE_TYPE) {
          // It's a machine-dependent frame type.
          if (! pageH_mdType_AgingSteal(pObj))
            continue;	// couldn't steal it
        } else {
          objC_CleanFrame1(pageH_ToObj(pObj));
	  if (objC_CleanFrame2(pageH_ToObj(pObj)) == false)
	    continue;
    
	  assert(!pageH_IsDirty(pObj));

	  /* Remove this page from the cache and return it to the free page
	   * list:
	   */
          assert(keyR_IsEmpty(&pageH_ToObj(pObj)->keyRing));
	  ReleaseObjPageFrame(pObj);
        }

	curPage++;
	return;
      }
      
      pObj->objAge++;

      if (pObj->objAge == age_Invalidate) {
        if (pageH_GetObType(pObj) > ot_PtLAST_COMMON_PAGE_TYPE) {
          // It's a machine-dependent frame type.
          if (pageH_mdType_AgingClean(pObj)) {
	    curPage++;	// it was freed
	    return;
          }
        } else {
	  objC_CleanFrame1(pageH_ToObj(pObj));
        }
      }
    }
  } while (--nPasses);
}

/* May Yield. */
PageHeader *
objC_GrabPageFrame()
{
  // WaitForAvailablePageFrame
  assert (objC_nFreePageFrames >= objC_nReservedIoPageFrames);
  
  if (objC_nFreePageFrames == objC_nReservedIoPageFrames)
    objC_AgePageFrames();

  assert (objC_nFreePageFrames > objC_nReservedIoPageFrames);

  assert (objC_nFreePageFrames > 0);

  assert(objC_firstFreePage);

  PageHeader * pObj = objC_firstFreePage;
  objC_firstFreePage = objC_firstFreePage->kt_u.free.next;

  DEBUG(ndalloc)
    printf("objC_GrabPageFrame obj=0x%08x\n", pObj);

  objC_GrabThisPageFrame(pObj);

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

static void
objC_GrabThisPageFrame(PageHeader *pObj)
{
  assert(pageH_GetObType(pObj) == ot_PtFreeFrame);
  objC_nFreePageFrames--;

  kva_t kva = pObj->pageAddr;	// preserve this field
  bzero(pObj, sizeof(*pObj));
  pObj->pageAddr = kva;

  pObj->kt_u.ob.obType = ot_PtNewAlloc; /* until further notice */

  assert(pte_ObIsNotWritable(pObj));

  pObj->objAge = age_NewBorn;
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
  objC_firstFreeNode = (Node *) objC_firstFreeNode->node_ObjHdr.next;
  objC_nFreeNodeFrames--;

  ObjectHeader * const pObj = &pNode->node_ObjHdr;
  assert(pObj->obType == ot_NtFreeFrame);

  /* Rip it off the hash chain, if need be: */
  objH_Unintern(pObj);	/* Should it ever be interned? */
  assert(keyR_IsEmpty(&pObj->keyRing));
  bzero(pObj, sizeof(ObjectHeader));

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

  pNode->objAge = age_NewBorn;
  objH_ResetKeyRing(pObj);
    
  return pNode;
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
  pageH->kt_u.free.obType = ot_PtFreeFrame;
  pageH->kt_u.free.next = objC_firstFreePage;
  objC_firstFreePage = pageH;

  objC_nFreePageFrames++;
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

  node_ToObj(pNode)->next = node_ToObj(objC_firstFreeNode);
  objC_firstFreeNode = pNode;
  objC_nFreeNodeFrames++;
}

/****************************************************************
 *
 * Interaction with object sources;
 *
 ****************************************************************/

static DEFQUEUE(SourceWait);

static ObjectSource *sources[MAX_SOURCE];
static uint32_t nSource = 0;

bool
objC_AddSource(ObjectSource *source)
{
  unsigned i = 0;

  if (nSource == MAX_SOURCE)
    fatal("Limit on total object sources exceeded\n");
  
  DEBUG(obsrc)
    printf("New source: \"%s\" [0x%08x%08x,0x%08x%08x)\n",
		   source->name,
		   (unsigned) (source->start >> 32),
		   (unsigned) (source->start),
		   (unsigned) (source->end >> 32),
		   (unsigned) (source->end));

  /* First, verify that the new source does not overlap any existing
   * source (except for the object cache, of course).
   */

  for (i = 1; i < nSource; i++) {
    if (source->end <= sources[i]->start)
      continue;
    if (source->start >= sources[i]->end)
      continue;
    dprintf(true, "New source 0x%x overlaps existing source 0x%x\n",
		    source, sources[i]);
    return false;
  }

  sources[nSource++] = source;

  sq_WakeAll(&SourceWait, false);

  return true;
}

bool
objC_HaveSource(OID oid)
{
  unsigned i = 0;

  /* NOTE that this skips the ObCache */
  for (i = 1; i < nSource; i++)
    if (sources[i]->start <= oid && oid < sources[i]->end)
      return true;

  /* Until proven otherwise: */
  return false;
}

/* May Yield. */
ObjectHeader *
objC_GetObject(OID oid, ObType obType,
               ObCount count, bool useCount)
{
  unsigned i = 0;

  ObjectHeader *pObj = 0;

  if (!objC_HaveSource(oid)) {
    dprintf(true, "No source for OID 0x%08x%08x...\n",
		    (unsigned long) (oid >> 32),
		    (unsigned long) (oid));
    act_SleepOn(&SourceWait);
    act_Yield();
  }

  for (i = 0; !pObj && i < nSource; i++) {
    if (sources[i]->start <= oid && oid < sources[i]->end)
      pObj = sources[i]->objS_GetObject(sources[i], oid, obType, count, useCount);
  }

  if (pObj) {
#ifdef DBG_WILD_PTR
    if (dbg_wild_ptr)
      check_Consistency("End GetObject()");
#endif

    if (useCount && pObj->allocCount != count)
      return 0;
      
    return pObj;
  }


  inv_MaybeDecommit(&inv);


  fatal("ObjecCache::GetObject(): oid 0x%08x%08x not found\n",
		(unsigned) (oid >> 32),
		(unsigned) oid);

  return 0;
}

bool
objC_WriteBack(ObjectHeader *pObj, bool inBackground)
{
  bool done = false;
  unsigned i = 0;

  for (i = 0; !done && i < nSource; i++)
    if (sources[i]->start <= pObj->oid && pObj->oid < sources[i]->end)
      done = sources[i]->objS_WriteBack(sources[i], pObj, inBackground);

  return done;
}

bool
objC_Invalidate(ObjectHeader *pObj)
{
  bool done = false;
  unsigned i = 0;

  for (i = 0; !done && i < nSource; i++)
    if (sources[i]->start <= pObj->oid && pObj->oid < sources[i]->end)
      done = sources[i]->objS_Invalidate(sources[i], pObj);

  return done;
}

bool
objC_IsRemovable(ObjectHeader *pObj)
{
  bool result = false;
  unsigned i = 0;

  for (i = 0; !result && i < nSource; i++)
    if (sources[i]->start <= pObj->oid && pObj->oid < sources[i]->end)
      result = sources[i]->objS_IsRemovable(sources[i], pObj);

  /* Until proven otherwise: */
  return result;
}

void
objC_FindFirstSubrange(OID limStart, OID limEnd, 
                       OID* subStart /*@ not null @*/, OID* subEnd /*@ not null @*/)
{
  unsigned i = 0;

  /* Bypass entry 0, because the object cache itself can of course
   * represent anything representable. */

  *subStart = ~0llu;		/* until proven otherwise */
  *subEnd = ~0llu;

  DEBUG(findfirst)
    printf("ObCache::FindFirstSubrange(): limStart 0x%08x%08x, "
		   "limEnd 0x%08x%08x  nSource %d\n",
		   (unsigned long) (limStart >> 32),
		   (unsigned long) (limStart),
		   (unsigned long) (limEnd >> 32),
		   (unsigned long) (limEnd),
		   nSource);

  /* ObjectSources (ignoring the object cache) implement disjoint
   * ranges, but they do not necessarily implement fully populated
   * ranges. 
   */
  for (i = 1; i < nSource; i++) {
    /* Check if the requested range and the source overlap: */
    if (sources[i]->end <= limStart) {
      DEBUG(findfirst)
	printf("Reject %d: end 0x%08x%08x <= limStart\n",
		       i,
		       (unsigned long) (sources[i]->end >> 32),
		       (unsigned long) (sources[i]->end));
      continue;
    }

    if (sources[i]->start >= limEnd) {
      DEBUG(findfirst)
	printf("Reject %d: start 0x%08x%08x >= limStart\n",
		       i,
		       (unsigned long) (sources[i]->start >> 32),
		       (unsigned long) (sources[i]->start));
      continue;
    }

    /* If so, and if the answer could possibly be better than what we
     * already have, ask the source: */
    if (sources[i]->start < *subStart) {
      DEBUG(findfirst)
	printf("Consulting source %d for [0x%08x%08x, 0x%08x%08x)\n",
		       i,
		       (unsigned long) (sources[i]->start >> 32),
		       (unsigned long) (sources[i]->start),
		       (unsigned long) (sources[i]->end >> 32),
		       (unsigned long) (sources[i]->end));
      sources[i]->objS_FindFirstSubrange(sources[i], limStart, limEnd, subStart, subEnd);
    }
  }
}

#ifdef OPTION_DDB
void
objC_ddb_DumpSources()
{
  extern void db_printf(const char *fmt, ...);
  unsigned i = 0;

  for (i = 0; i < nSource; i++) {
    ObjectSource *src = sources[i];
    printf("[0x%08x%08x,0x%08x%08x): %s\n",
	   (unsigned) (src->start >> 32),
	   (unsigned) src->start,
	   (unsigned) (src->end >> 32),
	   (unsigned) src->end,
	   src->name);
  }

  if (nSource == 0)
    printf("No object sources.\n");
}
#endif

/****************************************************************
 *
 * Interaction with object sources;
 *
 ****************************************************************/

void
objC_InitObjectSources()
{
  unsigned i;
  struct grub_mod_list * modp;
  ObjectSource *source = (ObjectSource *)KPAtoP(void *, physMem_Alloc(sizeof(ObjectSource), &physMem_any));

  /* code for initializing ObCacheSource */
  source->name = "obcache";
  source->start = 011u;
  source->end = ~011u;
  source->objS_Detach = ObCacheSource_Detach;
  source->objS_GetObject = ObCacheSource_GetObject;
  source->objS_IsRemovable = ObjectSource_IsRemovable;
  source->objS_WriteBack = ObCacheSource_WriteBack;
  source->objS_Invalidate = ObCacheSource_Invalidate;
  source->objS_FindFirstSubrange = ObjectSource_FindFirstSubrange;
  objC_AddSource(source);

  DEBUG (obsrc) printf("objC_InitObjectSources: Added obcache.\n");
  
  for (i = MultibootInfoPtr->mods_count,
         modp = KPAtoP(struct grub_mod_list *, MultibootInfoPtr->mods_addr);
       i > 0;
       --i, modp++) {
    uint32_t nObFrames;
    const char * p = KPAtoP(char *, modp->cmdline);
    OID startOid;

    /* Skip module file name. */
    while (*p != ' ' && *p != 0) p++;
    assert(*p == ' ');
    p++;

    /* Get starting OID from "command line" string. */
    startOid = strToUint64(&p);

    /* Calculate number of OIDs in this division. */
    nObFrames = (modp->mod_end - modp->mod_start)/EROS_PAGE_SIZE; /* size in pages */
    /* Preloaded module does not have a checkpoint seqno page. */
    /* Take out a pot for each whole or partial cluster. */
    nObFrames -= (nObFrames + (PAGES_PER_PAGE_CLUSTER-1))
                 / PAGES_PER_PAGE_CLUSTER;

    /* code for initializing PreloadObSource */
    source = (ObjectSource *)KPAtoP(void *, physMem_Alloc(sizeof(ObjectSource), &physMem_any));
    source->name = "preload";
    source->start = startOid;
    source->end = startOid + (nObFrames * EROS_OBJECTS_PER_FRAME);
    source->base = PTOV(modp->mod_start);
    source->objS_Detach = PreloadObSource_Detach;
    source->objS_GetObject = PreloadObSource_GetObject;
    source->objS_IsRemovable = ObjectSource_IsRemovable;
    source->objS_WriteBack = PreloadObSource_WriteBack;
    source->objS_Invalidate = PreloadObSource_Invalidate;
    source->objS_FindFirstSubrange = ObjectSource_FindFirstSubrange;
  
    objC_AddSource(source);

    DEBUG (obsrc) printf("objC_InitObjectSources: Added preloaded module, startOid=0x%08lx%08lx.\n",
                         (uint32_t) (startOid >> 32),
                         (uint32_t) startOid );
  }                                                                              
  for (i = 0; i < physMem_nPmemInfo; i++) {
    PmemInfo *pmi = &physMem_pmemInfo[i];

    if (pmi->type == MI_MEMORY) {
      source = (ObjectSource *)KPAtoP(void *, physMem_Alloc(sizeof(ObjectSource), &physMem_any));
      /* code for initializing PhysPageSource */
      source->name = "physpage";
      source->start = OID_RESERVED_PHYSRANGE + ((pmi->base / EROS_PAGE_SIZE) * EROS_OBJECTS_PER_FRAME);
      source->end = OID_RESERVED_PHYSRANGE + ((pmi->bound / EROS_PAGE_SIZE) * EROS_OBJECTS_PER_FRAME);
      source->pmi = pmi;
      source->objS_Detach = PhysPageSource_Detach;
      source->objS_GetObject = PhysPageSource_GetObject;
      source->objS_IsRemovable = ObjectSource_IsRemovable;
      source->objS_WriteBack = PhysPageSource_WriteBack;
      source->objS_Invalidate = PhysPageSource_Invalidate;
      source->objS_FindFirstSubrange = ObjectSource_FindFirstSubrange;
      objC_AddSource(source);

      DEBUG (obsrc) printf("objC_InitObjectSources: Added physmem.\n");
    }
  }

  // Need to special case the publication of the BIOS ROM, as we need
  // object cache entries for these. This must be done *after* the
  // MI_MEMORY cases, because malloc() needs to work.

  for (i = 0; i < physMem_nPmemInfo; i++) {
    PmemInfo *pmi = &physMem_pmemInfo[i];

    if (pmi->type == MI_BOOTROM) {
      objC_AddDevicePages(pmi);
      source = (ObjectSource *)KPAtoP(void *, physMem_Alloc(sizeof(ObjectSource), &physMem_any));
      /* code for initializing PhysPageSource */
      source->name = "physpage";
      source->start = OID_RESERVED_PHYSRANGE + ((pmi->base / EROS_PAGE_SIZE) * EROS_OBJECTS_PER_FRAME);
      source->end = OID_RESERVED_PHYSRANGE + ((pmi->bound / EROS_PAGE_SIZE) * EROS_OBJECTS_PER_FRAME);
      source->pmi = pmi;
      source->objS_Detach = PhysPageSource_Detach;
      source->objS_GetObject = PhysPageSource_GetObject;
      source->objS_IsRemovable = ObjectSource_IsRemovable;
      source->objS_WriteBack = PhysPageSource_WriteBack;
      source->objS_Invalidate = PhysPageSource_Invalidate;
      source->objS_FindFirstSubrange = ObjectSource_FindFirstSubrange;  
      objC_AddSource(source);

      DEBUG (obsrc) printf("objC_InitObjectSources: Added BOOTROM.\n");
    }
  }
}
