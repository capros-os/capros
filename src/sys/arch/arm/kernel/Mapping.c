/*
 * Copyright (C) 2006, 2007, 2008, 2009, Strawberry Development Group.
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
#include <kerninc/util.h>
#include <kerninc/KernStream.h>
#include <kerninc/Machine.h>
#include <kerninc/Process.h>
#include <kerninc/GPT.h>
#include <kerninc/Depend.h>
#include <kerninc/KernStats.h>
#include <kerninc/ObjectCache.h>
#include <kerninc/Activity.h>
#include <kerninc/PhysMem.h>
#include <arch-kerninc/PTE.h>
#include "arm.h"

#define dbg_track	0x2
#define dbg_map		0x4	/* migration state machine */

#define DEBUG(x) if (dbg_##x & dbg_flags)

/* Following should be an OR of some of the above */
#define dbg_flags   ( 0u )

extern kva_t heap_start;
extern kva_t heap_end;
extern kva_t heap_defined;
kva_t heap_havePTs;	/* second level page tables exist to here */
extern kva_t heap_bound;

/* FLPT_FCSEPA is the physical address of the First Level Page Table that is
   used for processes using the Fast Context Switch Extension. */
kpa_t FLPT_FCSEPA;
uint32_t * FLPT_FCSEVA;	/* Virtual address of the above */

/* Virtual address of the coarse page table mapped at 0xfff00000: */
uint32_t * HighCPTVA;

/* FLPT_NullPA is the physical address of the Null First Level Page Table.
   This is used for a process when its map isn't known yet. */
kpa_t FLPT_NullPA;
uint32_t * FLPT_NullVA;	/* Virtual address of the above */

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
the following states are possible for SmallSpaces[i]
(NOTE not for SmallSpaces[0]):
1. mth.isFree is nonzero. Nothing is mapped in this small space. 
2. mth.isFree is zero and domain is nonzero. 
   This domain is assigned to this space.
   Page tables may be mapped in this space using this domain. 
   SmallSpaces[i].producer->ssid == i.
3. mth.isFree is zero and domain is zero. 
   This space has no domain assigned.
   This space may have second-level descriptors that are like maps to page
   tables, but with zero in the domain field and 0b00 in the valid bits.
   SmallSpaces[i].producer->ssid == i.

Invariant: SmallSpaces[i].mth.tableCacheAddr == i << PID_SHIFT.
*/
struct SmallSpace {
  Link procs;		/* Processes using this small space */
  MapTabHeader mth;
  unsigned char domain;	/* the domain owned by this PID, 0 if none */
} SmallSpaces[NumSmallSpaces];


/********************** First level mapping table entries ****************/

// Invalidate a first level mapping table entry.
void
FLMTE_Invalidate(uint32_t * descrP)
{
  uint32_t descr = * descrP;
  if (descr & L1D_VALIDBITS) {	// it was valid
    SetMapWork_TLBCache();
  } else {
    // It was invalid, but could there be cache entries dependent on it?
    if (descr == PTE_ZAPPED) return;
    if (descr != PTE_IN_PROGRESS)
      SetMapWork_InvalidateCache();
    // Even if it was invalid, we want to change it to PTE_ZAPPED.
  }
  *descrP = PTE_ZAPPED;
}

static void
FLMTE_TrackReferenced(uint32_t * pFLMTE)
{
  // Turn this FLMTE into the form used for tracking LRU.
  const uint32_t val = *pFLMTE;
  if (val & L1D_VALIDBITS) {	// it was valid
    // Mark it temporarily invalid:
    *pFLMTE &= ~L1D_VALIDBITS;
    SetMapWork_TLB();
    DEBUG(track) printf("Tracking LRU, FLMTE %#x -> %#x\n",
                         val, *pFLMTE);
    // Leave any cache entries intact.
  }
}

static void
FLMTE_MakeRO(uint32_t * pFLMTE)
{
  assert(false);	// this shouldn't happen
}

static void
FLMTE_TrackDirty(uint32_t * pFLMTE)
{
  assert(false);	// this shouldn't happen
}

/*************************** Page table entries *********************/

/* Call this before reducing the authority of a PTE. */
void 
pte_Reduce(uint32_t pteval)
{
  if (pteval & PTE_VALIDBITS) {	// it was valid
    SetMapWork_TLBCache();
  } else {
    // It was invalid, but could there be cache entries dependent on it?
    if (pteval == PTE_ZAPPED) return;
    if (pteval != PTE_IN_PROGRESS)
      SetMapWork_InvalidateCache();
    // Even if it was invalid, we want to change it to PTE_ZAPPED.
  }
}

void 
pte_Invalidate(PTE * thisPtr)
{
  pte_Reduce(thisPtr->w_value);
  thisPtr->w_value = PTE_ZAPPED;
}

static void
PTE_TrackReferenced(PTE * pPTE)
{
  // Turn this PTE into the form used for tracking LRU.
  const uint32_t pteval = pPTE->w_value;
  if (pteval & PTE_VALIDBITS) {	// it was valid
    assert((pteval & PTE_VALIDBITS) == PTE_SMALLPAGE);
    pPTE->w_value = pteval & ~PTE_VALIDBITS;
    SetMapWork_TLB();
    DEBUG(track) printf("Tracking LRU, PTE %#x -> %#x\n",
                        pteval, pPTE->w_value);
    // Leave any cache entries intact.
  }
}

static void
PTE_MakeRO(PTE * pPTE)
{
  const uint32_t pteval = pPTE->w_value;
  if ((pteval & (PTE_VALIDBITS | 0xff0)) == (PTE_SMALLPAGE | 0xff0)) {
    // It's a PTE that grants user write access.
    // Change to read-only.
    pPTE->w_value = (pteval & ~0xff0) | 0xaa0
#ifndef OPTION_WRITEBACK
                    | (pteval & PTE_CACHEABLE ? PTE_BUFFERABLE : 0)
#endif
                    ;
    SetMapWork_TLB();
    DEBUG(track) dprintf(true, "Tracking dirty, PTE %#x -> %#x\n",
                         pteval, pPTE->w_value);
    // Leave any cache entries intact.
  }
}

static void
PTE_TrackDirty(PTE * pPTE)
{
  // Turn this PTE into the form used for tracking Dirty.
  const uint32_t pteval = pPTE->w_value;
  if ((pteval & (PTE_VALIDBITS | 0xff0)) == (PTE_SMALLPAGE | 0xff0)) {
    // It's a PTE that grants user write access.
    pPTE->w_value = (pteval & ~(PTE_CACHEABLE | PTE_BUFFERABLE | 0xff0))
                    | (PTE_CACHEABLE | 0xaa0);
    SetMapWork_TLB();
    DEBUG(track) dprintf(true, "Tracking dirty, PTE %#x -> %#x\n",
                         pteval, pPTE->w_value);
    // Leave any cache entries intact.
  }
}

void
proc_ResetMappingTable(Process * p)
{
  p->md.flmtProducer = NULL;
  p->md.firstLevelMappingTable = FLPT_NullPA;
  p->md.dacr = 0x1;	/* client access for domain 0 only,
	which means all user-mode accesses will fault. */
  p->md.pid = 0;	// this may overwrite PID_IN_PROGRESS
  link_Unlink(&p->md.pidLink);
}

static void
KeyDependEntry_Track(KeyDependEntry * kde,
  void (*FLMTEFunc)(uint32_t *),
  void (*PTEFunc)(PTE *))
{
  if (kde->pteCount == 0) {
    Process * const p = (Process *) kde->start;
    assert(IsInProcess(kde->start));
    proc_ResetMappingTable(p);
	       
    /* If this product is the active mapping table:
    Nothing to be done; TTBR, PID, and DACR will be reloaded
    when we next go to user mode. */
  } else {
    int i;
    kva_t mapping_page_kva = (kva_t)kde->start & ~EROS_PAGE_MASK;
    PageHeader * pMappingPage = objC_PhysPageToObHdr(VTOP(mapping_page_kva));
    unsigned int obType;
    if (pMappingPage) {
      obType = pageH_GetObType(pMappingPage);
    } else {
      /* pMappingPage could be zero,
      if the mapping page was allocated at kernel initialization
      and therefore doesn't have an associated PageHeader. */
      kva_t mapping_page_kva_16k = (kva_t)kde->start & ~(0x4000 - 1);

      // The null table shouldn't have any depend entries:
      assert(mapping_page_kva_16k != (kva_t)FLPT_NullVA);
      if (mapping_page_kva_16k == (kva_t)FLPT_FCSEVA)
        obType = ot_PtMappingPage1;
      else
        obType = ot_PtMappingPage2;
    }

    switch (obType) {
    default:
      /* If it is no longer a mapping page, stop. */
      kde->start = 0;
      return;

    case ot_PtMappingPage1: ;
      uint32_t * pFLMTE = (uint32_t *)kde->start;
      for (i = 0; i < kde->pteCount; i++, pFLMTE++) {
        (*FLMTEFunc)(pFLMTE);
      }
      break;
    
    case ot_PtMappingPage2: ;
      PTE * pPTE = (PTE *)kde->start;
      for (i = 0; i < kde->pteCount; i++, pPTE++) {
        (*PTEFunc)(pPTE);
      }
    }
  }
}

void
KeyDependEntry_Invalidate(KeyDependEntry * kde)
{
  KernStats.nDepInval++;
  
  KeyDependEntry_Track(kde, &FLMTE_Invalidate, &pte_Invalidate);

  kde->start = 0;
}

void
KeyDependEntry_MakeRO(KeyDependEntry * kde)
{
  KernStats.nDepMakeRO++;
  
  KeyDependEntry_Track(kde, &FLMTE_MakeRO, &PTE_MakeRO);
}

void
KeyDependEntry_TrackReferenced(KeyDependEntry * kde)
{
  KernStats.nDepTrackRef++;
  
  KeyDependEntry_Track(kde, &FLMTE_TrackReferenced, &PTE_TrackReferenced);
}

void
KeyDependEntry_TrackDirty(KeyDependEntry * kde)
{
  KernStats.nDepTrackDirty++;
  
  KeyDependEntry_Track(kde, &FLMTE_TrackDirty, &PTE_TrackDirty);
}

/* Zap all references to this mapping table. */
void
MapTab_ClearRefs(MapTabHeader * mth)
{
  ObjectHeader * const producer = mth->producer;
  keyR_ClearWriteHazard(& producer->keyRing);

  /* If the producer also produces a higher table, there could be
     a reference from that table to this one,
     that isn't tracked by Depend because there is no mediating key. */
  if (mth->tableSize == 0) {
    MapTabHeader * product;
    for (product = producer->prep_u.products;
         product;
         product = product->next) {
      if (product->tableSize) {	// if a first level table
        if (product->tableCacheAddr) {	// small space
          uint32_t ndx = product->tableCacheAddr >> L1D_ADDR_SHIFT;
          FLMTE_Invalidate(& FLPT_FCSEVA[ndx]);
        } else {		// large space
          // void * pte = MapTabHeaderToKVA(product);
          assert(false); //// FIXME need to invalidate 0th entry
        }
      }
    }
  }
}

void
node_ClearGPTHazard(Node * gpt, uint32_t ndx)
{
  Key * k = node_GetKeyAtSlot(gpt, ndx);

  Depend_InvalidateKey(k);

  if (keyBits_IsVoidKey(k)) {
    uint8_t l2vField = gpt_GetL2vField(gpt);
    unsigned int l2v = l2vField & GPT_L2V_MASK;

    if (l2v <= PID_SHIFT
        && ndx >= (1ul << (PID_SHIFT - l2v)) ) {
      /* There is a possibility that this GPT produced a small
      space, but can no longer do so. Invalidate any small spaces it produces.
      (objH_InvalidateProducts would be overkill - we don't want to
      invalidate large spaces.) */
      MapTabHeader * mth;
      MapTabHeader * nextMth;
      for (mth = node_ToObj(gpt)->prep_u.products; mth; mth = nextMth) {
        nextMth = mth->next;
        if (mth->tableSize && mth->tableCacheAddr) {	// a small space
          ReleaseProduct(mth);
        }
      }
    }
  }
}

/* Ensure there are enough mapping tables for the heap. */
static void
ensureHeapTables(kva_t target)
{
  while (heap_havePTs < target) {
    uint32_t s;
    kpa_t paddr = heap_AcquirePage();	/* get a page for mapping tables */
    for (s = EROS_PAGE_SIZE; s > 0;
         paddr += CPT_SIZE, s -= CPT_SIZE, heap_havePTs += CPT_SPAN) {
      memset(KPAtoP(void *, paddr), CPT_SIZE, 0);	/* clear it */
      unsigned int FLPTIndex = heap_havePTs >> 20;
      /* Update the FLPT_FCSE, which at this point is the only valid FLPT.
      If this procedure is called late, we must find and update *all* FLPT's. */
      FLPT_FCSEVA[FLPTIndex] = paddr + L1D_COARSE_PT;	/* domain 0 */
    }
  }
}

void
mach_HeapInit(kpsize_t heap_size)
{
  unsigned int i;

  FLPT_FCSEVA = KPAtoP(uint32_t *, FLPT_FCSEPA);

  // The CPT mapped at 0xfff00000 was allocated just above FLPT_FCSEPA.
  // Get its address now:
  HighCPTVA = KPAtoP(uint32_t *, FLPT_FCSEPA + 0x4000);

  /* Initialize the heap space. */
  heap_start = HeapVA;
  heap_end = HeapVA;
  heap_defined = HeapVA;
  heap_havePTs = HeapVA;
  heap_bound = HeapEndVA;

  /* Allocate page tables for the heap. */
  ensureHeapTables(heap_start + heap_size);

  /* Initialize the Null FLPT. */
  // Space for it was reserved just before the FLPT_FCSE.
  FLPT_NullPA = FLPT_FCSEPA - 0x4000;
  FLPT_NullVA = KPAtoP(uint32_t *, FLPT_NullPA);

  // All of user space is not mapped.
  kzero(FLPT_NullVA, UserEndVA >> (L1D_ADDR_SHIFT - 2));

  // Kernel space is mapped in every address space.
  memcpy(&FLPT_NullVA[UserEndVA >> L1D_ADDR_SHIFT],
         &FLPT_FCSEVA[UserEndVA >> L1D_ADDR_SHIFT],
         0x4000 - (UserEndVA >> (L1D_ADDR_SHIFT - 2)) );

  /* Initialize domains */
  for (i = 0; i < 16; i++) {
    MMUDomains[i].pid = 0;
  }

  /* Initialize SmallSpaces */
  for (i = 0; i < NumSmallSpaces; i++) {
    SmallSpaces[i].mth.isFree = 1;
    SmallSpaces[i].mth.tableCacheAddr = i << PID_SHIFT;
    SmallSpaces[i].mth.tableSize = 1;
    SmallSpaces[i].domain = 0;
    link_Init(&SmallSpaces[i].procs);
  }
}

MapTabHeader *
SmallSpace_GetMth(unsigned int ss)
{
  return & SmallSpaces[ss].mth;
}

static void
ReleaseSmallSpace(unsigned int pid)
{
  struct SmallSpace * ss = &SmallSpaces[pid];

  // Stop any processes from using this small space.
  while (! link_isSingleton(&ss->procs)) {
    Link * l = ss->procs.next;
    link_Unlink(l);
    Process * proc = container_of(l, Process, md.pidLink);
    assert(IsValidProcPtr(proc));
    proc_ResetMappingTable(proc);
  }

  MapTabHeader * mth = &ss->mth;

  objH_DelProduct(mth->producer, mth);

  // Invalidate the PTEs in the space.
  uint32_t ndx = pid << (PID_SHIFT - L1D_ADDR_SHIFT);
  unsigned int i;
  for (i = 0; i < 1ul << (PID_SHIFT - L1D_ADDR_SHIFT); i++)
    FLMTE_Invalidate(& FLPT_FCSEVA[ndx+i]);

  // Free the domain for this small space if any.
  unsigned int domain = ss->domain;
  if (domain) {
    ss->domain = 0;	// no longer has it
    MMUDomains[domain].pid = 0;
  }

  /* There should be no processes holding this pid; they should
  have had their pid invalidated above. */
#ifndef NDEBUG
  uint32_t pid32 = pid << PID_SHIFT;
  for (i = 0; i < KTUNE_NCONTEXT; i++) {
    Process * p = &proc_ContextCache[i];
    if (p->procRoot) {
      assertex(p, p->md.pid != pid32
                  || (pid32 == PID_IN_PROGRESS
                      && p->md.firstLevelMappingTable == FLPT_NullPA));
    }
  }
#endif

  mth->isFree = 1;
}

unsigned int lastSSAssigned = 1;
unsigned int lastSSStolen = 1;
/* Some day this might have to return failure? */
MapTabHeader *
AllocateSmallSpace(void)
{
  int i;

  for (i = 1; i < NumSmallSpaces; i++) {
    // Advance the cursor, counting down and wrapping.
    if (--lastSSAssigned == 0) lastSSAssigned = NumSmallSpaces-1;
    if (SmallSpaces[lastSSAssigned].mth.isFree) {
      goto assignSS;
    }
  }
  KernStats.nSmallSpaceSteal++;
  for (i = 0; i < KTUNE_NSSSTEAL; i++) {
    // Advance the cursor, counting down and wrapping.
    if (--lastSSStolen == 0) lastSSStolen = NumSmallSpaces-1;
    if (i == 0) lastSSAssigned = lastSSStolen;
    /* We don't care if the small space is kernelPinned,
     * because reconstructing it doesn't require any I/O. */

    ReleaseSmallSpace(lastSSStolen);
  }

assignSS:
  SmallSpaces[lastSSAssigned].mth.isFree = 0;	/* grab it */
  SmallSpaces[lastSSAssigned].mth.kernelPin = 0;
  assert(SmallSpaces[lastSSAssigned].domain == 0);
#if 0
  printf("Allocating small space %d\n", lastSSAssigned);
#endif
  return &SmallSpaces[lastSSAssigned].mth;
}

/* This process is now to use the specified small space. */
void
AssignSSToProc(Process * proc, unsigned int ssid)
{
  proc->md.firstLevelMappingTable = FLPT_FCSEPA;
  proc->md.pid = ssid << PID_SHIFT;
  link_insertAfter(&SmallSpaces[ssid].procs, &proc->md.pidLink);
}

unsigned int lastDomainAssigned = 1;
unsigned int lastDomainStolen = 1;
unsigned int
EnsureSSDomain(unsigned int ssid)
{
  int i;

  if (SmallSpaces[ssid].domain == 0) {
    for (i = 1; i < 16; i++) {
      // Advance the cursor, counting down and wrapping.
      if (--lastDomainAssigned == 0) lastDomainAssigned = 15;
      if (MMUDomains[lastDomainAssigned].pid == 0) {
        /* Found an unused domain. */
        goto assignDomain;
      }
    }
    // Need to steal a domain.
    /* FIXME: before stealing resources (such as a domain)
    from other processes, we should temporarily disfavor running
    this process, giving the other processes more opportunity
    to use the resources before they are stolen. */
    KernStats.nDomainSteal++;
    for (i = 0; i < KTUNE_NDOMAINSTEAL; i++) {
      // Advance the cursor, counting down and wrapping.
      if (--lastDomainStolen == 0) lastDomainStolen = 15;
      if (i == 0) lastDomainAssigned = lastDomainStolen;
      unsigned int pid = MMUDomains[lastDomainStolen].pid; // nonzero
      assert(SmallSpaces[pid].domain == lastDomainStolen);

      // Stealing the current pid is suboptimal:
      if ((act_CurContext()->md.pid >> PID_SHIFT) == pid) {
        continue;
      }

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

      /* For all processes that were using this pid,
      we must remove the stolen domain from their dacr. */
      MapTab_ClearRefs(&SmallSpaces[pid].mth);	// this is overkill

      SmallSpaces[pid].domain = 0;	// no longer has it
      MMUDomains[lastDomainStolen].pid = 0;
    }
    // remember to flush the TLB (but not the cache)
    SetMapWork_TLB();

assignDomain:
    // Assign the domain we found.
    MMUDomains[lastDomainAssigned].pid = ssid;
    SmallSpaces[ssid].domain = lastDomainAssigned;
#if 0
    printf("Allocating domain %d\n", lastDomainAssigned);
#endif
    return lastDomainAssigned;
  }
  return SmallSpaces[ssid].domain;
}

unsigned int
PIDToDomain(unsigned int ssid)
{
  return SmallSpaces[ssid].domain;
}

/* mach_EnsureHeap() must find (or clear) an available physical page and
 * cause it to become mapped at the end of the physical memory map.
 */
// May Yield.
void
mach_EnsureHeap(kva_t target)
{
  kpa_t paddr;
  unsigned FLPTIndex;	/* index into FLPT */
  unsigned CPTIndex;	/* index into CPT */

  assert(target <= heap_bound);
  assert((heap_defined & EROS_PAGE_MASK) == 0);

  /* Ensure there are enough pages. */
  while (heap_defined < target) {
    assert(heap_havePTs >= target);	/* Otherwise, we would have to
		allocate mapping tables here, The hard part is
		finding all FLPT's to map them. */

    paddr = heap_AcquirePage();	/* get a page for the heap */
    FLPTIndex = heap_defined >> 20;
    CPTIndex = (heap_defined >> 12) & (CPT_ENTRIES -1);
    PTE * cpt = KPAtoP(PTE *, FLPT_FCSEVA[FLPTIndex] & L1D_COARSE_PT_ADDR);
    PTE_Set(&cpt[CPTIndex],
            paddr + 0x550	/* AP=0b01 */
              + PTE_CACHEABLE + PTE_SMALLPAGE
#ifdef OPTION_WRITEBACK
    /* The heap is a good candidate for using write-back mode. */
              + PTE_BUFFERABLE
#endif
           );

    heap_defined += EROS_PAGE_SIZE;
  }

}

/* Procedure used by Check: */

bool
pageH_mdType_CheckPage(PageHeader * pPage, unsigned int * nmtf)
{
  PTE* pte;
  uint32_t ent;
  PTE* thePTE;

  assert(pageH_GetObType(pPage) == ot_PtMappingPage2);
  assert(pPage->kt_u.mp.hdrs[0].tableSize == 0);
	// top level table not fully implemented

  (*nmtf)++;

  int i;
  for (i=0; i<4; i++) {
    MapTabHeader * mth = & pPage->kt_u.mp.hdrs[i];
    if (mth->isFree) {
    } else {
      // Check a second-level mapping table (coarse page table).
      pte = (PTE*) pageH_GetPageVAddr(pPage) + i * CPT_ENTRIES;

      for (ent = 0; ent < CPT_ENTRIES; ent++) {
        thePTE = &pte[ent];
        if (pte_isValid(thePTE)) {
          uint32_t pteWord = pte_AsWord(thePTE);

          if ((pteWord & 0x30) == 0x30) {	// writeable by some user
            kpa_t pageFrame = pte_PageFrame(thePTE);
            PageHeader * thePageHdr = objC_PhysPageToObHdr(pageFrame);
            if (! thePageHdr || ! pageH_IsObjectType(thePageHdr)) {
              printf("Writable PTE=%#x at %#x (map hdr %#x) refers to non-object,"
  		       " thePageHdr=%#x\n",
		       pteWord, thePTE, pPage, thePageHdr);
              return false;
            }

            if (objH_IsKRO(pageH_ToObj(thePageHdr))) {
              printf("Writable PTE=%#x at %#x (map hdr %#x) KRO pg %#x\n",
		       pteWord, thePTE, pPage, thePageHdr);

              return false;
            }
            if (!pageH_IsDirty(thePageHdr)) {
              printf("Writable PTE=%#x at %#x (map hdr %#x) clean pg %#x\n",
		         pteWord, thePTE, pPage, thePageHdr);

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
// May Yield.
MapTabHeader *
AllocateCPT(void)
{
  MapTabHeader * mth;

  if (! freeCPTs) {
    PageHeader * pageH = objC_GrabPageFrame2(true);
    physMem_numMapTabPageFrames++;
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
  mth->isFree = 0;
  mth->kernelPin = 0;
  return mth;
}

// If all the mapping tables in this page are free, free the page.
bool
Check2ndLevelMappingTableFree(PageHeader * pageH)
{
  int i;
  for (i=0; i<4; i++) {
    if (! pageH->kt_u.mp.hdrs[i].isFree)
      return false;
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
  physMem_numMapTabPageFrames--;
  return true;
}

/* After calling this procedure, the caller must call
Check2ndLevelMappingTableFree(MapTab_ToPageH(mth)). */
static void
Release2ndLevelMappingTable(MapTabHeader * mth)
{
  MapTab_ClearRefs(mth);

  objH_DelProduct(mth->producer, mth);

  /* Don't need to invalidate the entries in the page. */

  FreeCPT(mth);
}

void
ReleaseProduct(MapTabHeader * mth)
{
  assert(! mth->isFree);
  if (mth->tableSize == 0) {	// second level table
    Release2ndLevelMappingTable(mth);
    Check2ndLevelMappingTableFree(MapTab_ToPageH(mth));
  } else {
    uint32_t pid32 = mth->tableCacheAddr;
    if (pid32 == 0) {
      assert(false);	/* FIXME: First level tables not supported yet. */
    } else {	// product is a small space
      ReleaseSmallSpace(pid32 >> PID_SHIFT);
    }
  }
}

#if 0
void
pageH_mdType_EvictFrame(PageHeader * pageH)
{
  assert(pageH_GetObType(pageH) == ot_PtMappingPage2);
	// first level page tables not fully implemented yet.
  int i;
  for (i=0; i<4; i++) {
    MapTabHeader * mth = &pageH->kt_u.mp.hdrs[i];
    if (! mth->isFree) {
      assert(! mth->kernelPin);
      Release2ndLevelMappingTable(mth);
    }
  }
  // Page can now be freed. The following call cleans up and frees the page:
  Check2ndLevelMappingTableFree(pageH);
}
#endif

static void
MapTab_TrackReferenced(MapTabHeader * mth)
{
  ObjectHeader * producer = mth->producer;
  keyR_ProcessAllMaps(& producer->keyRing, &KeyDependEntry_TrackReferenced);

  /* If the producer also produces a higher table, there could be
     a reference from that table to this one,
     that isn't tracked by Depend because there is no mediating key. */
  if (mth->tableSize == 0) {
    MapTabHeader * product;
    for (product = producer->prep_u.products;
         product;
         product = product->next) {
      if (product->tableSize) {	// if a first level table
        if (product->tableCacheAddr) {	// small space
          uint32_t ndx = product->tableCacheAddr >> L1D_ADDR_SHIFT;
          uint32_t * FLMTE = & FLPT_FCSEVA[ndx];
          FLMTE_TrackReferenced(FLMTE);
        } else {		// large space
          // void * pte = MapTabHeaderToKVA(product);
          assert(false); //// FIXME need to invalidate 0th entry
        }
      }
    }
  }
}

bool	// return true iff page was freed
pageH_mdType_Aging(PageHeader * pageH)
{
  int i;

  switch (pageH_GetObType(pageH)) {
  default:
    assert(false);
    return false;

  case ot_PtMappingPage1:
    assert(false);	// not implemented yet
    return false;

  case ot_PtMappingPage2:
    for (i=0; i<4; i++) {
      MapTabHeader * mth = & pageH->kt_u.mp.hdrs[i];
      if (! mth->isFree) {
        /* Pinning a page or node also pins any produced mapping tables. */
        if (! objH_IsUserPinned(mth->producer)
            && objH_GetFlags(mth->producer, OFLG_Cleanable)
            && ! mth->kernelPin ) {
          switch (mth->mthAge) {
          case age_MTInvalidate:
            MapTab_TrackReferenced(mth);
          default:
            mth->mthAge++;
            break;

          case age_MTSteal:
            Release2ndLevelMappingTable(mth);
          }
        }
      }
    }
    return Check2ndLevelMappingTableFree(pageH);
  }
}

/* When the kernel needs to read or write a page that is also accessed
 * by processes, it needs to be concerned with cache coherency. 
 * When reading, it calls pageH_MapCoherentRead before reading, and
 * pageH_UnmapCoherentRead afterwards. 
 * When writing, it calls pageH_MapCoherentWrite before writing, and
 * pageH_UnmapCoherentWrite afterwards.
 *
 * You can have one read map and one write map active at once.
 * The read map uses TempMap0VA and the write map uses TempMap1VA. */

kva_t
pageH_MapCoherentRead(PageHeader * pageH)
{
  switch (pageH->kt_u.ob.cacheAddr & EROS_PAGE_MASK) {
  default: ;
    assert(false);

  case CACHEADDR_WRITEABLE:	// writeable at one MVA
    // There may be dirty cache entries for this page, so clean them.
    // If we also need to invalidate the cache, this is a good time
    // to do it.
    mach_DoCacheWork(mapWork | MapWork_UserDirtyWrong);
  case CACHEADDR_NONE:	// not previously mapped
  case CACHEADDR_ONEREADER:	// read only at one MVA
  case CACHEADDR_UNCACHED:	// writer(s) and multiple MVAs

  /* For case CACHEADDR_READERS, we could just access the page
   * at pageH_GetPageVAddr(pageH).
   * But since there could be stale cache entries at that address,
   * we would have to call
   * mach_DoCacheWork(MapWork_UserCacheWrong | MapWork_UserDirtyWrong).
   * Instead, just map it uncached.
   * Since we generally only read the page once,
   * we are going to have to fetch from RAM in either case.
   * This way, we avoid disturbing the useful stuff in the cache. */
  case CACHEADDR_READERS:	// readers at multiple MVAs

    // Map the page uncached.
    HighCPTVA[(TempMap0VA & CPT_ADDR_MASK) >> EROS_PAGE_LGSIZE]
      = pageH_GetPhysAddr(pageH) + 0x000 + PTE_SMALLPAGE;
    mach_FlushBothTLBs();
    return TempMap0VA;
  }
}

void
pageH_UnmapCoherentRead(PageHeader * pageH)
{
  /* Leave the mapping there. We will purge the TLB the next time we use it. */
}

kva_t
pageH_MapCoherentWrite(PageHeader * pageH)
{
  switch (pageH->kt_u.ob.cacheAddr & EROS_PAGE_MASK) {
  default: ;
    assert(false);

  case CACHEADDR_NONE:	// not previously mapped
  case CACHEADDR_UNCACHED:	// writer(s) and multiple MVAs
    // Map the page uncached.
    HighCPTVA[(TempMap1VA & CPT_ADDR_MASK) >> EROS_PAGE_LGSIZE]
      = pageH_GetPhysAddr(pageH) + 0x550 + PTE_SMALLPAGE;
    mach_FlushBothTLBs();
    return TempMap1VA;

  case CACHEADDR_WRITEABLE:	// writeable at one MVA
    // There may be dirty cache entries for this page, so clean them:
    // If we also need to invalidate the cache, this is a good time
    // to do it.
    mach_DoCacheWork(mapWork | MapWork_UserDirtyWrong);
  case CACHEADDR_ONEREADER:	// read only at one MVA
  case CACHEADDR_READERS:	// readers at multiple MVAs
    return pageH_GetPageVAddr(pageH);
  }
}

void
pageH_UnmapCoherentWrite(PageHeader * pageH)
{
  /* Page was mapped uncached or write-through, so no need to clean cache. */
}

void
pageH_PrepareForDMAOutput(PageHeader * pageH)
{
  switch (pageH->kt_u.ob.cacheAddr & EROS_PAGE_MASK) {
  default: ;
    assert(false);

  case CACHEADDR_WRITEABLE:	// writeable at one MVA
    // There may be dirty cache entries for this page, so clean them:
    // If we also need to invalidate the cache, this is a good time
    // to do it.
    mach_DoCacheWork(mapWork | MapWork_UserDirtyWrong);
    break;

  case CACHEADDR_NONE:	// not previously mapped
  case CACHEADDR_ONEREADER:	// read only at one MVA
  case CACHEADDR_UNCACHED:	// writer(s) and multiple MVAs
  case CACHEADDR_READERS:	// readers at multiple MVAs
    break;
  }
  mach_DrainWriteBuffer();
}

/* There must be no user cache entries for this page. */
void
pageH_PrepareForDMAInput(PageHeader * pageH)
{
  switch (pageH_GetObType(pageH)) {
  case ot_PtKernelUse:
  case ot_PtTagPot:
  case ot_PtHomePot:
  case ot_PtLogPot:
  case ot_PtMappingPage1:
  case ot_PtMappingPage2: ;
    // This page is for kernel use,
    // so we mustn't have any stale cache entries:
    mach_DoCacheWork(MapWork_UserCacheWrong | MapWork_UserDirtyWrong);
    break;

  default: ;
    break;
  }
  mach_DrainWriteBuffer();
}

bool
check_ProcessMD(Process * proc)
{
  if (proc->md.firstLevelMappingTable == FLPT_FCSEPA) {	// small space
    assert(proc->md.flmtProducer);
    if (proc->md.firstLevelMappingTable != FLPT_FCSEPA) {
      printf("proc %#x has pid but FMLT not FCSE!\n", proc);
      return false;
    }
    // It should be on the small space list.
    int ss = proc->md.pid >> 25;
    Link * l;
    for (l = SmallSpaces[ss].procs.next;
         l != &SmallSpaces[ss].procs;
         l = l->next) {
      if (l == &proc->md.pidLink)
        goto found;
    }
    printf("proc %#x not in pid list at %#x!\n", proc, &SmallSpaces[ss]);
    return false;
  found: ;
  } else if (proc->md.firstLevelMappingTable == FLPT_NullPA) {	// no space
    assert(! proc->md.pid
           || proc->md.pid == PID_IN_PROGRESS);
  } else {			// full space
    assert(proc->md.flmtProducer);
    assert(! proc->md.pid);
  }
  return true;
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
  printf("    producer=0x%08x\n",
         mth->producer );
}

void
pageH_mdFields_dump_header(PageHeader * pageH)
{
  printf(" cacheAddr=%#x\n", pageH->kt_u.ob.cacheAddr);
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

