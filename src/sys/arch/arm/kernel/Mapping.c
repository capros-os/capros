/*
 * Copyright (C) 2006, Strawberry Development Group.
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
   Research Projects Agency under Contract No. W31P4Q-06-C-0040. */

#include <string.h>
#include <kerninc/kernel.h>
#include <kerninc/KernStream.h>
#include <kerninc/Machine.h>
#include <kerninc/Process.h>
#include <kerninc/ObjectCache.h>
#include <kerninc/Activity.h>
#include <arch-kerninc/PTE.h>
#include "arm.h"
#include "PTEarm.h"

#define dbg_map		0x4	/* migration state machine */

#define DEBUG(x) if (dbg_##x & dbg_flags)

/* Following should be an OR of some of the above */
#define dbg_flags   ( 0u )

struct ObjectHeader;

extern kva_t heap_start;
extern kva_t heap_end;
extern kva_t heap_defined;
kva_t heap_havePTs;	/* second level page tables exist to here */
extern kva_t heap_bound;

kva_t kernelStackBot;	/* the highest address of the kernel stack +1 */

/* FLPT_FCSEPA is the physical address of the First Level Page Table that is
   used for processes using the Fast Context Switch Extension. */
kpa_t FLPT_FCSEPA;
uint32_t * FLPT_FCSEVA;	/* Virtual address of the above */

/* MMU Domains

*/
struct MMUDomain {
  unsigned char pid;	/* the process ID that owns this domain, 0 if none */
} MMUDomains[16];

/* Small Spaces

One address space (FLPT_FCSE) uses the Fast Context Switch Extension to divide 
the space into multiple small spaces. 
(This lets us avoid flushing the cache and TLB.)
A process using a small address space runs with the Process ID for that space.

*/
struct SmallSpace {
  struct ObjectHeader * producer;	/* or NULL if space is not assigned */
  unsigned char domain;	/* the domain owned by this PID, 0 if none */
} SmallSpaces[NumSmallSpaces];

void
map_HeapInit(void)
{
  int i;

  FLPT_FCSEVA = KPAtoP(uint32_t *, FLPT_FCSEPA);

  /* Initialize the heap space. */
  heap_start = HeapVA;
  heap_end = HeapVA;
  heap_defined = HeapVA;
  heap_havePTs = HeapVA;
  heap_bound = HeapEndVA;

  /* Initialize domains */
  for (i = 0; i < 16; i++) {
    MMUDomains[i].pid = 0;
  }

  /* Initialize SmallSpaces */
  for (i = 0; i < NumSmallSpaces; i++) {
    SmallSpaces[i].producer = 0;
    SmallSpaces[i].domain = 0;
  }
}

/* Some day this might have to return failure? */
unsigned int
AllocateSmallSpace(ObjectHeader * producer)
{
  int i;

  for (i = 1; i < NumSmallSpaces; i++) {
    if (SmallSpaces[i].producer == 0) {
      SmallSpaces[i].producer = producer; /* grab it */
      assert(SmallSpaces[i].domain == 0);
      return i;
    }
  }
  fatal("AllocateSmallSpace: stealing space unimplemented\n");
  return 0;
}

unsigned int
EnsureSSDomain(unsigned int ssid)
{
  int i;

  if (SmallSpaces[ssid].domain == 0) {
    for (i = 1; i < 16; i++) {
       if (MMUDomains[i].pid == 0) {
         /* Found an unused domain. */
         MMUDomains[i].pid = ssid;
         SmallSpaces[ssid].domain = i;
         return i;
       }
    }
    fatal("Unimplemented: need to steal a domain\n");
  }
  return SmallSpaces[ssid].domain;
}

void
proc_ResetMappingTable(Process * p)
{
  p->md.flmtProducer = NULL;
  p->md.firstLevelMappingTable = FLPT_FCSEPA;
  p->md.dacr = 0x1;	/* client access for domain 0 only,
	which means all user-mode accesses will fault. */
  p->md.pid = 0;
}

/* mach_EnsureHeap() must find (or clear) an available physical page and
 * cause it to become mapped at the end of the physical memory map.
 */
void
mach_EnsureHeap(kva_t target,
  kpa_t (*acquire_heap_page)(void) )
{
  kpa_t paddr;
  unsigned FLPTIndex;	/* index into FLPT */
  unsigned CPTIndex;	/* index into CPT */

  assert(target <= heap_bound);
  assert((heap_defined & EROS_PAGE_MASK) == 0);

  /* Ensure there are enough pages. */
  while (heap_defined < target) {
    /* Ensure there are enough mapping tables. */
    while (heap_havePTs < target) {
      uint32_t s;
      paddr = (*acquire_heap_page)();	/* get a page for mapping tables */
      for (s = EROS_PAGE_SIZE; s > 0;
           paddr += CPT_SIZE, s -= CPT_SIZE, heap_havePTs += CPT_SPAN) {
        memset(KPAtoP(void *, paddr), CPT_SIZE, 0);	/* clear it */
        FLPTIndex = heap_havePTs >> 20;
        FLPT_FCSEVA[FLPTIndex] = paddr + L1D_COARSE_PT;	/* domain 0 */
      }
    }

    paddr = (*acquire_heap_page)();	/* get a page for the heap */
    FLPTIndex = heap_defined >> 20;
    CPTIndex = (heap_defined >> 12) & (CPT_ENTRIES -1);
    PTE * cpt = KPAtoP(PTE *, FLPT_FCSEVA[FLPTIndex] & L1D_COARSE_PT_ADDR);
    PTE_Set(&cpt[CPTIndex],
            paddr + 0x550	/* AP=0b01 */
              + PTE_CACHEABLE + PTE_BUFFERABLE + PTE_SMALLPAGE);

    heap_defined += EROS_PAGE_SIZE;
  }

}

/* Procedure used by Check: */

bool
pageH_mdType_CheckPage(PageHeader * pPage)
{
  PTE* pte;
  uint32_t ent;
  PTE* thePTE;

  assert(pageH_GetObType(pPage) == ot_PtMappingPage);

  if (pPage->kt_u.mp.tableSize == 1)
    return true;

  // Check a second-level mapping table.
  pte = (PTE*) pageH_GetPageVAddr(pPage);

#define MAPPING_ENTRIES_PER_PAGE 1024	// this is wrong
  for (ent = 0; ent < MAPPING_ENTRIES_PER_PAGE; ent++) {
    thePTE = &pte[ent];
    if (pte_isValid(thePTE)) {
      uint32_t pteWord = pte_AsWord(thePTE);

      if ((pteWord & 0x30) == 0x30) {	// writeable by some user
        kpa_t pageFrame = pte_PageFrame(thePTE);

#if 0
        kva_t thePage = PTOV(pageFrame);
        if (thePage >= KVTOL(KVA_FROMSPACE))
  	continue;
#endif

        PageHeader * thePageHdr = objC_PhysPageToObHdr(pageFrame);
        assert(thePageHdr && pageH_IsObjectType(thePageHdr));

        if (objH_GetFlags(pageH_ToObj(thePageHdr), OFLG_CKPT)) {
  	  printf("Writable PTE=0x%08x (map page 0x%08x), ckpt pg"
  		       " 0x%08x%08x\n",
		       pteWord, pte,
		       (uint32_t) (thePageHdr->kt_u.ob.oid >> 32),
		       (uint32_t) thePageHdr->kt_u.ob.oid);

	  return false;
        }
        if (!pageH_IsDirty(thePageHdr)) {
  	  printf("Writable PTE=0x%08x (map page 0x%08x), clean pg"
		         " 0x%08x%08x\n",
		         pteWord, pte,
		         (uint32_t) (thePageHdr->kt_u.ob.oid >> 32),
		         (uint32_t) thePageHdr->kt_u.ob.oid);

	  return false;
        }
      }
    }
  }

  return true;
}

/* This procedure may Yield. */
static void
ReleaseMappingFrame(PageHeader * pObj)
{
  assert(pageH_GetObType(pObj) == ot_PtMappingPage);
  ObjectHeader * pProducer = pObj->kt_u.mp.producer;

  assert(pProducer);
  assert(keyR_IsValid(&pProducer->keyRing, pProducer));

  assert(objH_IsUserPinned(pProducer) == false);
    
  /* Zapping the key ring will help if producer was a page or
   * was of perfect height -- ensures that the PTE in the next
   * higher level of the table gets zapped.
   */
  keyR_UnprepareAll(&pProducer->keyRing);

  if ( pProducer->obType == ot_NtSegment ) {
    assert(! node_IsKernelPinned(objH_ToNode(pProducer)));
    /* FIX: What follows is perhaps a bit too strong:
     * 
     * Unpreparing the producer will have invalidated ALL of it's
     * products, including this one.  We should probably just be
     * disassociating THIS product from the producer.
     * 
     * While this is overkill, it definitely works...
     */

    node_Unprepare((Node *)pProducer, false);

  } else {
    assert((pProducer->obType == ot_PtDataPage)
           || (pProducer->obType == ot_PtDevicePage) );
    assert(! pageH_IsKernelPinned(objH_ToPage(pProducer)));
  }

  if (pageH_GetObType(pObj) == ot_PtMappingPage) {
    kva_t pgva = pageH_GetPageVAddr(pObj);
    DEBUG(map)
      printf("Blasting mapping page at 0x%08x\n", pgva);
    pte_ZapMappingPage(pgva);
  }

  ReleasePageFrame(pObj);

  /* This product (and all it's siblings) are now on the free
   * list.  The possibility exists, however, that we contrived
   * to invalidate some address associated with the current
   * activity by yanking this mapping table, so we need to do a
   * Yield() here to force the current process to retry:
   */
  /* Activity::Current() changed to act_Current() */
  act_Wakeup(act_Current());
  act_Yield(act_Current());
}

void
pageH_mdType_EvictFrame(PageHeader * pageH)
{
  assert(pageH_GetObType(pageH) == ot_PtMappingPage);
  ReleaseMappingFrame(pageH);
}

bool
pageH_mdType_AgingExempt(PageHeader * pageH)
{
  assert(pageH_GetObType(pageH) == ot_PtMappingPage);
  /* Mapping pages cannot go out if their producer is pinned,
  because they are likely to be involved in page translation. */

  ObjectHeader * pProducer = pageH->kt_u.mp.producer;
  if (objH_IsUserPinned(pProducer))
    return true;
  // if (objH_IsKernelPinned(pProducer)) return true;
  return false;
}

bool	// return true iff page was freed
pageH_mdType_AgingClean(PageHeader * pageH)
{
  assert(pageH_GetObType(pageH) == ot_PtMappingPage);
  /* It's a lot cheaper to regenerate a mapping page than to
   * read some other page back in from the disk...
   */
  ReleaseMappingFrame(pageH);
  return true;
}

bool
pageH_mdType_AgingSteal(PageHeader * pageH)
{
  assert(pageH_GetObType(pageH) == ot_PtMappingPage);
  /* Mapping pages should never make it to PageOut age, because
   * they should be zapped at the invalidate age. It's
   * relatively cheap to rebuild them, and zapping them eagerly
   * has the desirable consequence of keeping their associated
   * nodes in memory if the process is still active.
   */
  assert(false);
  return false;
}

#ifdef OPTION_DDB

void
pageH_mdType_dump_pages(PageHeader * pageH)
{
  char producerType;

  assert(pageH_GetObType(pageH) == ot_PtMappingPage);

  if (pageH->kt_u.mp.producer == 0) {
    producerType = '?';
  }
  else if (pageH->kt_u.mp.producer->obType <= ot_NtLAST_NODE_TYPE) {
    producerType = 'n';
  }
  else {
    producerType = 'p';
  }
  printf("prod %c\n", producerType);
}

void
pageH_mdType_dump_header(PageHeader * pageH)
{
  printf("    prodBlss=%d rwProd=%c producer=0x%08x\n",
	 pageH->kt_u.mp.producerBlss,
         pageH->kt_u.mp.rwProduct ? 'y' : 'n',
         pageH->kt_u.mp.producer );
}

#endif

