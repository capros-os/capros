/*
 * Copyright (C) 2006, 2007, Strawberry Development Group.
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
Research Projects Agency under Contract Nos. W31P4Q-06-C-0040 and
W31P4Q-07-C-0070.  Approved for public release, distribution unlimited. */

#include <string.h>
#include <kerninc/kernel.h>
#include <kerninc/KernStream.h>
#include <kerninc/Machine.h>
#include <kerninc/Process.h>
#include <kerninc/Depend.h>
#include <kerninc/KernStats.h>
#include <kerninc/ObjectCache.h>
#include <kerninc/Activity.h>
#include <arch-kerninc/PTE.h>
#include "arm.h"
#include "PTEarm.h"

#define dbg_map		0x4	/* migration state machine */

#define DEBUG(x) if (dbg_##x & dbg_flags)

/* Following should be an OR of some of the above */
#define dbg_flags   ( 0u )

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

MapTabHeader * freeCPTs = 0;	// list of free coarse page tables

/* MMU Domains

*/
struct MMUDomain {
  unsigned char pid;	/* the process ID that owns this domain, 0 if none */
} MMUDomains[16];

/* Small Spaces

One address space (FLPT_FCSE) uses the Fast Context Switch Extension to divide 
the space into multiple small spaces. 
(This lets us avoid flushing the cache and TLB when switching from one
small space to another.)
A process using a small address space runs with the Process ID for that space.

SmallSpaces[0] is unused.

For 0 < i < NumSmallSpaces,
the following states are possible for SmallSpaces[i]:
1. producer is zero. Nothing is mapped in this small space. 
2. producer is nonzero and domain is nonzero. 
   This domain is assigned to this space.
   Page tables may be mapped in this space using this domain. 
   SmallSpaces[i].producer->ssid == i.
3. producer is nonzero and domain is zero. 
   This space has no domain assigned.
   This space may have second-level descriptors that are like maps to page
   tables, but with zero in the domain field and 0b00 in the valid bits.
   SmallSpaces[i].producer->ssid == i.

For all ObjectHeaders objH,
objH->ssid is zero or SmallSpace[objH->ssid].producer == objH.
*/
struct SmallSpace {
  ObjectHeader * producer;	/* or NULL if space is not assigned */
  unsigned char domain;	/* the domain owned by this PID, 0 if none */
} SmallSpaces[NumSmallSpaces];

void
proc_ResetMappingTable(Process * p)
{
  p->md.flmtProducer = NULL;
  p->md.firstLevelMappingTable = FLPT_FCSEPA;
  p->md.dacr = 0x1;	/* client access for domain 0 only,
	which means all user-mode accesses will fault. */
  p->md.pid = 0;
}

void
KeyDependEntry_Invalidate(KeyDependEntry * kde)
{
  if (kde->start == 0) {	// unused entry
    return;
  }
  
  KernStats.nDepInval++;
  
#ifdef DEPEND_DEBUG
  printf("Invalidating key entries start=0x%08x, count=%d\n",
	      kde->start, kde->pteCount);
#endif
  if (kde->pteCount == 0) {
    Process * const p = (Process *) kde->start;
    assert(IsInProcess(kde->start));
    proc_ResetMappingTable(p);
	       
#if 0
     /* If this product is the active mapping table:
     Nothing to be done; TTBR, PID, and DACR will be reloaded
     when we next go to user mode. */
  
    if (p == act_CurContext()) {
      //// mach_LoadTTBR(FLPT_FCSEPA);
    }
#endif
  } else {
    kva_t mapping_page_kva = ((kva_t)kde->start & ~EROS_PAGE_MASK);
    PageHeader * pMappingPage = objC_PhysPageToObHdr(VTOP(mapping_page_kva));
    /* pMappingPage could be zero, if the mapping page was allocated
       at kernel initialization and therefore doesn't have an
       associated PageHeader. */

    /* If it is no longer a mapping page, stop. */
    // FIXME: support first level mapping pages too.
    if (pMappingPage && (pageH_GetObType(pMappingPage) != ot_PtMappingPage2)) {
      kde->start = 0;
      return;
    }

    PTE * pPTE = (PTE *)kde->start;
    int i;
    for (i = 0; i < kde->pteCount; i++, pPTE++) {
      pte_Invalidate(pPTE);
    }
  }

  kde->start = 0;
}

/* Zap all references to this mapping table. */
void
MapTab_ClearRefs(MapTabHeader * mth)
{
  ObjectHeader * const producer = mth->producer;
  keyR_ClearWriteHazard(& producer->keyRing);

  /* If the producer produces a higher table, there could be
     a reference from that table to this one. */
  if (mth->tableSize == 0) {
    MapTabHeader * product;
    for (product = producer->prep_u.products;
         product;
         product = product->next) {
      if (product->tableSize) {	// if a directory
        // void * pte = MapTabHeaderToKVA(product);
        assert(false); //// FIXME need to find th right entry and invalidate it
      }
    }
  }
}

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
#if 0
         printf("Allocating domain %d\n", i);
#endif
         return i;
       }
    }
    // Need to steal a domain.
    /* FIXME: before stealing resources (such as a domain)
    from other processes, we should temporarily disfavor running
    this process, giving the other processes more opportunity
    to use the resources before they are stolen. */
    /* Since we have to flush TLB and cache here, we might as well
    steal all the domains at once. */
#if 0
    printf("Recycling domains\n");
#endif
    for (i = 1; i < 16; i++) {
      unsigned int pid = MMUDomains[i].pid; // nonzero
      assert(SmallSpaces[pid].domain == i);
      /* Invalidate all level 1 descriptors for this pid. */
      /* Note: we invalidate the descriptors, but leave some information
      behind so they can be quickly revalidated if we assign a new
      domain to this pid. */
      int j;
      for (j = 0; j < 1ul << (PID_SHIFT - L1D_ADDR_SHIFT); j++) {
        /* In a user pid's map, there are no section descriptors,
        so an invalid nonzero descriptor is always for a coarse PT. */
        // Invalidate the descriptor and clear the domain field.
        FLPT_FCSEVA[(pid << (PID_SHIFT - L1D_ADDR_SHIFT)) + j]
          &= ~(L1D_DOMAIN_MASK | L1D_VALIDBITS);
      }
      SmallSpaces[pid].domain = 0;	// no longer has it
      MMUDomains[i].pid = 0;
    }
    PteZapped = true;
    // Assign domain 1 for the caller.
    MMUDomains[1].pid = ssid;
    SmallSpaces[ssid].domain = 1;
#if 0
    printf("Allocating domain 1\n");
#endif
  }
  return SmallSpaces[ssid].domain;
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
              + PTE_CACHEABLE + PTE_SMALLPAGE);
    /* The heap might be a good candidate for using write-back mode
    (PTE_BUFFERABLE),
    but if we did that we would have to clean the cache when we flush it. */

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

  assert(pageH_GetObType(pPage) == ot_PtMappingPage2);
  assert(pPage->kt_u.mp.hdrs[0].tableSize == 0);
	// top level table not fully implemented

  int i;
  for (i=0; i<4; i++) {
    MapTabHeader * mth = & pPage->kt_u.mp.hdrs[i];
    if (mth->isFree) {
    } else {
      // Check a second-level mapping table (coarse page table).
      pte = (PTE*) pageH_GetPageVAddr(pPage);

      for (ent = 0; ent < CPT_ENTRIES; ent++) {
        thePTE = &pte[ent];
        if (pte_isValid(thePTE)) {
          uint32_t pteWord = pte_AsWord(thePTE);

          if ((pteWord & 0x30) == 0x30) {	// writeable by some user
            kpa_t pageFrame = pte_PageFrame(thePTE);
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
    }
  }

  return true;
}

// Free a coarse page table.
// Caller must check if this is the last free table in its page.
static void
FreeCPT(MapTabHeader * mth)
{
  mth->isFree = 1;
  mth->next = freeCPTs;
  freeCPTs = mth;
}

// Allocate a coarse page table.
// May Yield?
MapTabHeader *
AllocateCPT(void)
{
  MapTabHeader * mth;

  if (! freeCPTs) {
    PageHeader * pageH = objC_GrabPageFrame();
    pageH->kt_u.mp.obType = ot_PtMappingPage2;
    int i;
    for (i=0; i<4; i++) {
      mth = &pageH->kt_u.mp.hdrs[i];
      mth->tableSize = 0;
      mth->ndxInPage = i;
      FreeCPT(mth);
    }
  }
  // Grab a CPT from the free list.
  mth = freeCPTs;
  freeCPTs = mth->next;
  return mth;
}

// If all the mapping tables in this page are free, free the page.
void
Check2ndLevelMappingTableFree(PageHeader * pageH)
{
  int i;
  for (i=0; i<4; i++) {
    if (! pageH->kt_u.mp.hdrs[i].isFree)
      return;
  }
  // unchain them from free list
  for (i=0; i<4; i++) {
    MapTabHeader * mth = & pageH->kt_u.mp.hdrs[i];
    MapTabHeader * * mthpp = &freeCPTs;
    while (*mthpp != mth) {
      assert(*mthpp);	// else not found in list
      mthpp = &(*mthpp)->next;
    }
    *mthpp = mth->next;	// unchain it
  }
  ReleasePageFrame(pageH);
}

/* After calling this procedure, the caller must call
Check2ndLevelMappingTableFree(MapTab_ToPageH(mth)). */
static void
Release2ndLevelMappingTable(MapTabHeader * mth)
{
  if (mth->isFree) return;	// nothing to do

  MapTab_ClearRefs(mth);

  objH_DelProduct(mth->producer, mth);

  /* Don't need to invalidate the entries in the page. */

  FreeCPT(mth);
}

void
ReleaseProduct(MapTabHeader * mth)
{
  /* First level tables not supported yet. */
  Release2ndLevelMappingTable(mth);
  Check2ndLevelMappingTableFree(MapTab_ToPageH(mth));
}

void
pageH_mdType_EvictFrame(PageHeader * pageH)
{
  assert(pageH_GetObType(pageH) == ot_PtMappingPage2);
	// first level page tables not fully implemented yet.
  int i;
  for (i=0; i<4; i++) {
    Release2ndLevelMappingTable(&pageH->kt_u.mp.hdrs[i]);
  }
  // Page can now be freed. The following call cleans up and frees the page:
  Check2ndLevelMappingTableFree(pageH);
}

bool
pageH_mdType_AgingExempt(PageHeader * pageH)
{
  assert(pageH_GetObType(pageH) == ot_PtMappingPage2);
  /* Pinning a page or node also pins any produced mapping tables. */

  int i;
  for (i=0; i<4; i++) {
    MapTabHeader * mth = & pageH->kt_u.mp.hdrs[i];
    if (objH_IsUserPinned(mth->producer))
      return true;
    // if (objH_IsKernelPinned(mth->producer)) return true;
  }
  return false;
}

bool	// return true iff page was freed
pageH_mdType_AgingClean(PageHeader * pageH)
{
  assert(pageH_GetObType(pageH) == ot_PtMappingPage2);
  /* It's a lot cheaper to regenerate a mapping page than to
   * read some other page back in from the disk...
   */
  pageH_mdType_EvictFrame(pageH);
  return true;
}

bool
pageH_mdType_AgingSteal(PageHeader * pageH)
{
  assert(pageH_GetObType(pageH) == ot_PtMappingPage2);
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
  assert(pageH_GetObType(pageH) == ot_PtMappingPage2);
  printf("\n");
}

void
DumpMapTabHdr(MapTabHeader * mth)
{
  printf("    prodBlss=%d rwProd=%c producer=0x%08x\n",
	 mth->producerBlss,
         mth->rwProduct ? 'y' : 'n',
         mth->producer );
}

void
pageH_mdType_dump_header(PageHeader * pageH)
{
  if (pageH->kt_u.mp.hdrs[0].tableSize) {
    // First Level mapping table
    DumpMapTabHdr(&pageH->kt_u.mp.hdrs[0]);
  } else {
    // Second Level mapping table
    int i;
    for (i=0; i<4; i++) {
      printf("    tbl[%d]: ", i);
      DumpMapTabHdr(&pageH->kt_u.mp.hdrs[i]);
    }
  }
}

#endif

