/*
 * Copyright (C) 1998, 1999, 2001, Jonathan S. Shapiro.
 * Copyright (C) 2006, Strawberry Development Group.
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

/* Drivers for 386 protection faults */

#include <string.h>
#include <kerninc/kernel.h>
#include <eros/ProcessState.h>
#include <kerninc/Check.h>
#include <kerninc/KernStats.h>
#include <kerninc/Activity.h>
#include <kerninc/Node.h>
#include <kerninc/ObjectCache.h>
#include <kerninc/Depend.h>
#include <kerninc/Machine.h>
#include <kerninc/Debug.h>
/*#include <kerninc/util.h>*/
#include <kerninc/Process.h>
#include <kerninc/SegWalk.h>
#include <arch-kerninc/Process.h>
#include "Process486.h"
#include <arch-kerninc/PTE.h>
#include "IDT.h"
#include "lostart.h"
#include "Segment.h"

#define dbg_pgflt	0x1	/* steps in taking snapshot */

/* Following should be an OR of some of the above */
#define dbg_flags   ( 0u )

#define DEBUG(x) if (dbg_##x & dbg_flags)

/* Possible outcomes of a user-level page fault:
 * 
 * 1. Fault was due to a not-present page, and address is not valid in
 * address space segment.  Domain should be faulted and an attempt
 * made to invoke appropriate keeper.
 * 
 * 2. Fault was due to a not-present page, but address IS valid in
 * address space segment.  Segment indicates access is valid.
 * Construct the PDE and PTE for this mapping. If the object
 * referenced is hazarded, construct a write-hazard mapping even if
 * the object is in principle writable. Restart the process.
 * 
 * 3. Fault was an access violation due to writing a page that in
 * principle was read-only.  Domain should be faulted and an attempt
 * made to invoke appropriate keeper.
 * 
 * 4. Fault was an access violation due to writing a page that has a
 * write-hazarded mapping.  Two cases: If the hazard still applies, go
 * to sleep on the hazarded object.  else validate the mapping for
 * writing and restart the operation.
 * 
 * EROS does not provide write-only or execute-only mappings.
 * 
 * 
 * Meaning of the error code value for a page fault:
 * 
 * Bit  Value  Meaning
 * 0    0      Not-present page
 * 0    1      Page-level protection violation
 * 1    0      Access was a read
 * 1    1      Access was a write
 * 2    0      Access was supervisor-mode
 * 2    1      Access was user-mode
 */

extern uint32_t CpuType;
#ifndef NDEBUG
extern void end();
extern void start();
#endif

bool PteZapped = false;

static ObjectHeader *
proc_MakeNewPageTable(SegWalk* wi /*@ not null @*/, uint32_t ndx); 
static ObjectHeader*
proc_MakeNewPageDirectory(SegWalk* wi /*@ not null @*/); 


#ifdef OPTION_DDB
void
pte_ddb_dump(PTE* thisPtr)
{
  extern void db_printf(const char *fmt, ...);

  char attrs[64];
  char *nxtAttr = attrs;
  printf("Pg Frame 0x%08x [", pte_PageFrame(thisPtr));

#define ADDATTR(s) do { const char *sp = (s); *nxtAttr++ = ','; while (*sp) *nxtAttr++ = *sp++; } while (0)
  
  if (pte_is(thisPtr, PTE_V))
    ADDATTR("V");
  else
    ADDATTR("!V");

  if (pte_is(thisPtr, PTE_W)) ADDATTR("W");
  if (pte_is(thisPtr, PTE_USER))
    ADDATTR("U");
  else
    ADDATTR("S");
  if (pte_is(thisPtr, PTE_ACC)) ADDATTR("A");
  if (pte_is(thisPtr, PTE_DRTY)) ADDATTR("D");
  if (pte_is(thisPtr, PTE_PGSZ)) ADDATTR("L");
  if (pte_is(thisPtr, PTE_GLBL)) ADDATTR("G");
  if (pte_is(thisPtr, PTE_WT))
    ADDATTR("WT");
  else
    ADDATTR("!WT");
  if (pte_is(thisPtr, PTE_CD)) ADDATTR("CD");

#undef ADDATTR

  *nxtAttr++ = 0;
  printf("%s]\n", attrs);
}
#endif

void
Depend_InvalidateProduct(ObjectHeader* page)
{
  /* InvalidateProduct is always called after the producing Node has
   * been unprepared (and thus zapped).  If this is a page table, we
   * therefore know that all of it's entries are dead.
   * 
   * If this is a page directory, however, we must find all of the
   * mapping table pointers that name it and zap those.
   */

#if 0
  dprintf(true, "NAILING product 0x%08x\n", page);
#endif
  kpa_t mp_pa;
  kpa_t curmap;
  uint32_t i;
  
  assert (page->obType == ot_PtMappingPage);

  mp_pa = VTOP(objC_ObHdrToPage(page));
	       
  /* MUST BE CAREFUL -- if this product is the active mapping table we
   * need to reset the mapping pointer to the native kernel map!
   */
  
  curmap = mach_GetMappingTable();
  
  if (mp_pa == curmap) {
#if 0
    dprintf(true, "Nailing active mapping table!\n");
#endif
  
    mach_SetMappingTable(KernPageDir_pa);

  }
  
  if (page->producerNdx == EROS_NODE_LGSIZE) {
    for (i = 0; i < KTUNE_NCONTEXT; i++)
      if (proc_ContextCache[i].MappingTable == mp_pa)
	proc_ContextCache[i].MappingTable = 0;
  }
  
  mach_FlushTLB();
}

#ifndef NDEBUG
bool
pte_ObIsNotWritable(ObjectHeader *pObj)
{
  uint32_t nFrames;
  PTE *pte = 0;
  unsigned i;
  bool result = true;
  uint32_t pf;
  PTE *ptepg = 0;
  uint32_t limit;
  uint32_t ent;
  
  /* Start by building a writable PTE for the page: */
  uint32_t kvaw = objC_ObHdrToPage(pObj);

  kvaw |= PTE_W;

#ifdef OPTION_SMALL_SPACES
  /* Check small spaces first: */
  nFrames = KTUNE_NCONTEXT / 32;
  pte = proc_smallSpaces;
  
  for (i = 0; i < nFrames * NPTE_PER_PAGE; i++) {
    if ((pte[i].w_value & (PTE_FRAMEBITS|PTE_W)) == kvaw) {
      dprintf(true,
		      "Checking pobj 0x%x with frame at 0x%x\n"
		      "Pg hdr 0x%x retains writable small PTE at 0x%x\n",
		      pObj, kvaw,
		      pObj, &pte[i]);
      result = false;
    }
  }
#endif

  for (pf = 0; pf < objC_TotalPages(); pf++) {
    ObjectHeader *pHdr = objC_GetCorePageFrame(pf);

    if (pHdr->obType != ot_PtMappingPage)
      continue;

    ptepg = (PTE *) objC_ObHdrToPage(pHdr);
    
    limit = NPTE_PER_PAGE;
    if (pHdr->producerNdx == EROS_NODE_LGSIZE)
      limit = (UMSGTOP >> 22);	/* PAGE_ADDR_BITS + PAGE_TABLE_BITS */

    for (ent = 0; ent < limit; ent++) {
      if ((ptepg[ent].w_value & (PTE_FRAMEBITS|PTE_W)) == kvaw) {
	dprintf(true,
			"Checking pobj 0x%x with frame at 0x%x\n"
			"Page hdr 0x%x retains writable PTE at 0x%x\n",
			pObj, kvaw,
			pHdr, &ptepg[ent]);
	result = false;
      }
    }
  }

  return result;
}
#endif /* !NDEBUG */

void
Depend_WriteDisableProduct(ObjectHeader *pObj)
{
  /* This is trickier than the case above, since we must not
   * write-disable kernel mappings.  On the other hand, we don't need
   * to worry about blasting the current mapping table either.
   */
  kva_t mp_va;
  uint32_t limit;
  PTE* pte = 0;
  uint32_t entry;

  assert (pObj->obType == ot_PtMappingPage);

  mp_va = objC_ObHdrToPage(pObj);

  /* Each mapping table holds 1024 entries, but the uppermost portion
   * of the address space is reserved for the kernel and the small
   * spaces.  Kernel pages should not be write disabled, nor should
   * the small space directory entries (small space page table entries 
   * are write disabled as a special case at the page table level).
   */

  limit = NPTE_PER_PAGE;
  if (pObj->producerNdx == EROS_NODE_LGSIZE)
    limit = (UMSGTOP >> 22);	/* PAGE_ADDR_BITS + PAGE_TABLE_BITS */

  pte = (PTE*) mp_va;
  for (entry = 0; entry < limit; entry++)
    pte_WriteProtect(&pte[entry]);
}

#ifdef INVOKE_TIMING
extern "C" {
  uint64_t rdtsc();
};
#endif

bool
PageFault(savearea_t *sa)
{
#ifdef OPTION_KERN_TIMING_STATS
  uint64_t top_time = rdtsc();
#ifdef OPTION_KERN_EVENT_TRACING
  uint64_t top_cnt0 = Machine::ReadCounter(0);
  uint64_t top_cnt1 = Machine::ReadCounter(1);
#endif
#endif

  ula_t la = sa->ExceptAddr;
  uint32_t error = sa->Error;
  bool writeAccess = (error & 2) ? true : false;
  Process *ctxt = 0;

  /* sa->Dump(); */

  /* If we page faulted from supervisor mode it's trivial: */
  if ( (error & 4) == 0) {
#if 0
    if ( (sa->EIP == (uint32_t) ipc_probe_start)
	 || (sa->EIP == (uint32_t) ipc_probe_top) ) {
      printf("Send mapping fault 0\n");
      sa->EAX = 1;
      sa->EIP = (uint32_t) ipc_probe_end;
      return;
    }
#endif

#if 0
    sa->Dump();
#endif
    fatal("Kernel page fault\n"
		  " SaveArea      =  0x%08x  EIP           =  0x%08x\n"
		  " Fault address =  0x%08x  Code          =  0x%08x\n"
		  " CS            =  0x%08x\n",
		  sa, sa->EIP,
		  la, error, sa->CS);
  }

  /* Domain page fault.  If we take this from kernel mode we are in
   * deep kimchee.
   */

  ctxt = (Process*) act_CurContext();


  assert(& ctxt->trapFrame == sa);

  ctxt->stats.pfCount++;

  KernStats.nPfTraps++;
  if (writeAccess) KernStats.nPfAccess++;

  PteZapped = false;

  objH_BeginTransaction();


  (void) proc_DoPageFault(ctxt, la, writeAccess, false);

  /* We succeeded (wonder of wonders) -- release pinned resources. */
  objH_ReleasePinnedObjects();

  /* No need to release uncommitted I/O page frames -- there should
   * not be any.
   */


  assert(act_CurContext());


#ifdef OPTION_KERN_TIMING_STATS
  {
    extern uint64_t pf_delta_cy;
    uint64_t bot_time = rdtsc();

#ifdef OPTION_KERN_EVENT_TRACING
    extern uint64_t pf_delta_cnt0;
    extern uint64_t pf_delta_cnt1;

    uint64_t bot_cnt0 = Machine::ReadCounter(0);
    uint64_t bot_cnt1 = Machine::ReadCounter(1);
    pf_delta_cnt0 += (bot_cnt0 - top_cnt0);
    pf_delta_cnt1 += (bot_cnt1 - top_cnt1);
#endif
    pf_delta_cy += (bot_time - top_time);
  }
#endif

  return false;
}

#define DATA_PAGE_FLAGS  (PTE_ACC|PTE_USER|PTE_V)

#ifdef OPTION_SMALL_SPACES
bool
proc_DoSmallPageFault(Process * p, ula_t la, bool isWrite,
		      bool prompt)
{
#if 0
  dprintf(true, "SmallPgFlt w/ la=0x%08x, bias=0x%08x,"
		  " limit=0x%08x\n", la, p->bias, p->limit);
#endif

  /* Address is a linear address.  Subtract the base and check the
   * bound.
   */
  uva_t va = la - p->bias;

#if 0
  /* This assertion can be false given that the process can run from
   * any address space and the save logic may subsequently save that
   * address space pointer. The code is preserved here to keep a
   * record of the fact that this may be untrue so that I do not
   * forget and re-insert the assertion.
   */
  assert ( fixRegs.MappingTable == KERNPAGEDIR );
#endif

  SegWalk wi;
  PTE *thePTE = 0;
  kpa_t pageAddr;
  ObjectHeader *pPageHdr = 0;
  bool needInvalidate;

  wi.faultCode = FC_NoFault;
  wi.traverseCount = 0;
  wi.segObj = 0;
  wi.vaddr = va;
  wi.frameBits = EROS_PAGE_ADDR_BITS;
  wi.writeAccess = isWrite;
  wi.invokeKeeperOK = BOOL(!prompt);
  wi.invokeProcessKeeperOK = BOOL(!prompt);
  wi.wantLeafNode = false;

  segwalk_init(&wi, proc_GetSegRoot(p));


  thePTE /*@ not null @*/ = &p->smallPTE[(va >> EROS_PAGE_ADDR_BITS) % SMALL_SPACE_PAGES];

  if (pte_isnot(thePTE, PTE_V))
    thePTE->w_value = PTE_IN_PROGRESS;
    
  /* If the virtual address falls outside the small range and is
   * valid, this walk will result in depend entries that blast the
   * wrong PTE.  No real harm can come of that, since either the
   * address is bad or we will in that case be switching to a large
   * space anyway.
   */
  
  /* Do the traversal... */
  if ( !proc_WalkSeg(p, &wi, EROS_PAGE_BLSS, thePTE, 0, false) ) { 
    proc_SetFault(p, wi.faultCode, va, false);
    return false;
  }

  /* If the wrong dependency entry was reclaimed, we may lost
   * one of the depend entries for the PTE that is under construction,
   * in which case we must yield and retry. This is low-likelihood. */
  if (thePTE->w_value == PTE_ZAPPED)

    act_Yield(act_Current());


  /* If we get this far, there is a valid translation at this address,
   * but if the address exceeds the small space limit we need to
   * convert the current address space into a large space.
   * 
   * If this is necessary, set smallPTE to zero and YIELD, because we
   * might have been called from unside the IPC code.  This will
   * induce another page fault, which will follow the large spaces
   * path.
   */

  /* This should not happen on the invoker side, where the addresses
   * were validated during page probe (causing SS fault or GP fault).
   * On the invokee side, however, it is possible that we forced the
   * invokee to reload, in which case it loaded as a small space, and
   * we then tried to validate the receive address.  In that case,
   * PopulateExitBlock() called us here with an out of bounds virtual
   * address.  We reset the process mapping state and Yield(),
   * allowing the correct computation to be done in the next pass.
   */

  if (va >= p->limit) {
#if 0
    dprintf(true, "!! la=0x%08x va=0x%08x\n"
		    "Switching process 0x%X to large space\n",
		    la, va,
		    procRoot->ob.oid);
#endif

    proc_SwitchToLargeSpace(p);
    act_Yield(act_Current());
  }
     
  pageAddr = 0;
  
  pPageHdr = wi.segObj;
    

  if (isWrite)
    objH_MakeObjectDirty(pPageHdr);

  pageAddr = VTOP(objC_ObHdrToPage(pPageHdr));

  assert ((pageAddr & EROS_PAGE_MASK) == 0);
  assert (pageAddr < PtoKPA(start) || pageAddr >= PtoKPA(end));
	  
  assert (va < (SMALL_SPACE_PAGES * EROS_PAGE_SIZE));

  needInvalidate = false;
  
  if (isWrite && pte_is(thePTE, PTE_V)) {
    /* We are doing this because the old PTE had insufficient
     * permission, so we must zap the TLB.
     */
    needInvalidate = true;
  }
  
  pte_Invalidate(thePTE);
  pte_set(thePTE, (pageAddr & PTE_FRAMEBITS) | DATA_PAGE_FLAGS);
  if (isWrite)
    pte_set(thePTE, PTE_W);
#ifdef WRITE_THROUGH
  if (CpuType >= 5)
    pte_set(thePTE, PTE_WT);
#endif

#if 0
  if ((wi.segObj->kt_u.ob.oid & OID_RESERVED_PHYSRANGE) 
      == OID_RESERVED_PHYSRANGE)
    wi.canCache = false;
#endif

  if (!wi.canCache)
    pte_set(thePTE, PTE_CD);

#if 0
  dprintf(true, "Built PTE at 0x%08x\n", &thePTE);
#endif
  

  if (needInvalidate)
    mach_FlushTLBWith(la);

    
#ifdef DBG_WILD_PTR
  if (dbg_wild_ptr)
    check_Consistency("End of DoSmallPageFault()");
#endif

  return true;
}
#endif

uint32_t DoPageFault_CallCounter;

/* At some point, this logic will need to change to account for
 * background windows.  In the event that we encounter a non-local
 * background window key we will need to do a complete traversal in
 * order to find the background segment, because the background
 * segment slot is not cached.
 * 
 * Actually, this is contingent on a design distinction, which is
 * whether multiple red segments should be tracked on the way down the
 * segment tree.  When we cross a KEPT red segment, we should
 * certainly forget any outstanding background segment -- we do not
 * want the red segment keeper to be able to fabricate a background
 * window key that might reference a segment over which the keeper
 * should not have authority.
 * 
 * A case can be made, however, that a kept red segment should be
 * permitted to contain a NON-kept red segment that specifies a
 * background segment.  The main reason to desire this is to allow
 * (e.g.) VCSK to operate on a background segment that contains a
 * window.
 * 
 * For the moment, we do not support this, and I am inclined to
 * believe that it is unwise to do so until I see a case in which it
 * is useful.
 * 
 * Local windows are not yet supported by the SegWalk code, but none
 * of this is an issue for local windows,
 */

INLINE uint64_t
BLSS_MASK64(uint32_t blss, uint32_t frameBits)
{
  uint32_t bits_to_shift =
    (blss - EROS_PAGE_BLSS) * EROS_NODE_LGSIZE + frameBits; 

  uint64_t mask = (1ull << bits_to_shift);
  mask -= 1ull;
  
  return mask;
}

/* #define WALK_LOUD */
#define FAST_TRAVERSAL
bool
proc_DoPageFault(Process * p, ula_t la, bool isWrite, bool prompt)
{
  const int walk_root_blss = 4 + EROS_PAGE_BLSS;
  const int walk_top_blss = 2 + EROS_PAGE_BLSS;
  uva_t va;
  SegWalk wi;
  PTE *pTable = 0;
  
#ifdef DBG_WILD_PTR
  if (dbg_wild_ptr)
    check_Consistency("Top of DoPageFault()");
#endif

  DoPageFault_CallCounter++;
  
  DEBUG(pgflt) {
    printf("DoPageFault: ctxt=0x%08x EIP 0x%08x la=0x%08x, isWrite=%c prompt=%c\n",
		   p,
		   p->trapFrame.EIP,
		   la,
		   isWrite ? 't' : 'f',
		   prompt ? 't' : 'f');
  }
  
#ifdef OPTION_SMALL_SPACES
  va = la - p->bias;
#else
  uva_t va = la;
#endif
  
  /* If LA is simply out of range, then forget the whole thing: */
  if ( la >= KVA ) {
    dprintf(true, "Domain accessed kernel or small space la 0x%08x\n",
	    (uint32_t) la);
    proc_SetFault(p, FC_InvalidAddr, va, false);  
    return false;
  }

#ifdef OPTION_SMALL_SPACES
  if (p->smallPTE)
    return proc_DoSmallPageFault(p, la, isWrite, prompt);
#endif
			    
  /* If LA is simply out of range, then forget the whole thing: */
  if ( la >= UMSGTOP ) {
    dprintf(true, "Large domain accessed small space la\n");
    proc_SetFault(p, FC_InvalidAddr, va, false);
    return false;
  }

  /* If we discover on the way to load the process that it's mapping
   * table register was voided, we substituted KERNPAGEDIR.  Notice
   * that here:
   */
  if ( p->MappingTable == KernPageDir_pa )
    p->MappingTable = PTE_ZAPPED;

  /* Set up a WalkInfo structure and start building the necessary
   * mapping table, PDE, and PTE entries.
   */

  wi.faultCode = FC_NoFault;
  wi.traverseCount = 0;
  wi.segObj = 0;
  wi.vaddr = va;
  wi.frameBits = EROS_PAGE_ADDR_BITS;
  wi.writeAccess = isWrite;
  wi.invokeKeeperOK = BOOL(!prompt);
  wi.invokeProcessKeeperOK = BOOL(!prompt);
  wi.wantLeafNode = false;
  
  segwalk_init(&wi, proc_GetSegRoot(p));

  /* See if there is already a page table. If not, go find/build one. */
  pTable = (PTE *) (PTOV(p->MappingTable) & ~EROS_PAGE_MASK);

#ifdef WALK_LOUD
  dprintf(false, "pTable is 0x%x\n", pTable);
#endif
  
  if (pTable == 0)
    goto need_pgdir;
  
  {
    ObjectHeader *pTableHdr = objC_PhysPageToObHdr(PtoKPA(pTable));
   
    if (isWrite && !pTableHdr->rwProduct) {
      dprintf(true, "DoPageFault(): isWrite && !pTableHdr->rwProduct hdr 0x%x\n", pTableHdr);
      goto access_fault;
    }

    wi.canCall = BOOL(pTableHdr->caProduct);
    
#ifdef FAST_TRAVERSAL
    /* We have a page directory conveying suitable access rights from
       the top.  See if the PDE has the necessary permissions: */
    {
      uint32_t pdeNdx = (la >> 22) & 0x3ffu;
      PTE* thePDE /*@ not null @*/ = &pTable[pdeNdx];

      if ( pte_is(thePDE, PTE_V|PTE_USER) ) {

	/* We could short-circuit the walk in this case by remembering
	 * the status of /wi.canWrite/ in a spare bit in the PTE, but
	 * at the moment we do not do so because in most cases the
	 * write restriction appears lower down in the segment tree.
	 */

	if ( pte_is(thePDE, PTE_W) || !isWrite ) {
	  /* We have a valid PDE with the necessary permissions! */

	  pTable = KPAtoP(PTE *, pte_PageFrame(thePDE));
	  pTableHdr = objC_PhysPageToObHdr(PtoKPA(pTable));
        
	  
	  wi.offset = wi.vaddr & ((1u << 22) - 1u);
	  wi.segBlss = pTableHdr->producerBlss;
	  wi.segObj = pTableHdr->prep_u.producer;
	  wi.redSeg = pTableHdr->kt_u.mp.redSeg;
	  if (wi.redSeg) {
	    wi.redSpanBlss = pTableHdr->kt_u.mp.redSpanBlss;
	    wi.redSegOffset =
	      ((uint64_t) wi.vaddr) & BLSS_MASK64(wi.redSpanBlss, wi.frameBits);
	  }
	  wi.segObjIsWrapper = BOOL(pTableHdr->kt_u.mp.wrapperProducer);
	  wi.canWrite = BOOL(pTableHdr->rwProduct);
	  wi.canCall = BOOL(wi.canCall && pTableHdr->caProduct);

#if 0
	  if (wi.redSeg && wi.offset != wi.redSegOffset) {
      dprintf(false, "pdr proc_WalkSeg: wi.producer 0x%x, wi.prodBlss %d wi.isRed %d\n"
		      "wi.vaddr 0x%x wi.offset 0x%X flt %d  wa %d segKey 0x%x\n"
		      "canCall %d canWrite %d\n"
		      "redSeg 0x%x redOffset 0x%X\n",
		      wi.segObj, wi.segBlss, wi.segObjIsRed,
		      wi.vaddr, wi.offset, wi.segFault, wi.writeAccess,
		      0x0,
		      wi.canCall, wi.canWrite,
		      wi.redSeg, wi.redSegOffset);

	    dprintf(true, "Found pg dir. Offset 0x%X RedSegOffset 0x%X spanBlss %d\n",
			    wi.offset, wi.redSegOffset, wi.redSpanBlss);
	  }
#endif
	  
	  /* NOTE: This is doing the wrong thing when the red segment
	   * needs to be grown upwards, because the segBlss in that
	   * case is less than the *potential* span of the red
	   * segment.
	   */
#ifdef WALK_LOUD
	  dprintf(false, "have_good_pde\n");
#endif
	  goto have_good_pde;
	}
      }
    }
#endif /* FAST_TRAVERSAL */
    
    wi.offset = wi.vaddr;
    wi.segBlss = pTableHdr->producerBlss;
    wi.segObj = pTableHdr->prep_u.producer;
    wi.redSeg = pTableHdr->kt_u.mp.redSeg;
    if (wi.redSeg) {
      wi.redSpanBlss = pTableHdr->kt_u.mp.redSpanBlss;
      wi.redSegOffset =
	((uint64_t) wi.vaddr) & BLSS_MASK64(wi.redSpanBlss, wi.frameBits);
    }
    wi.segObjIsWrapper = pTableHdr->kt_u.mp.wrapperProducer;
    wi.canWrite = BOOL(pTableHdr->rwProduct);

#ifdef WALK_LOUD
    dprintf(false, "have_pgdir\n");
#endif
    goto have_pgdir;
  }
  
 need_pgdir:
  /* No page directory was found, so we need to construct a page
   * directory. */
  
  p->MappingTable = PTE_IN_PROGRESS;
  
  /* Begin the traversal... */
  if ( !proc_WalkSeg(p, &wi, walk_root_blss,
		     (PTE *)&p->MappingTable, 0, false) ) { 
    proc_SetFault(p, wi.faultCode, va, false);

    return false;
  }

  /* If the wrong depend entry was reclaimed, we may have just lost
   * the mapping table entry. If we are still good to go, : */
  if (p->MappingTable == PTE_ZAPPED)
    act_Yield(act_Current());

  /* Since the address space pointer register lacks permission bits,
   * we cannot be here due to lack of permissions at this
   * level. Therefore, if we are processing this path at all the
   * mapping table must have been invalid, in which case it should now
   * be PTE_IN_PROGRESS. */
  assert (p->MappingTable == PTE_IN_PROGRESS);

  /* We can now reset the value to the zap guard. */
  p->MappingTable = PTE_ZAPPED;
  
  assert (pTable == 0);
  if (pTable == 0) {
    /* See if a mapping table has already been built for this address
     * space.  If so, just use it.  Using wi.segBlss is okay here
     * because the mapping table pointer will be zapped if anything
     * above this point gets changes, whereupon the gunk the the page
     * directory will no longer matter.
     */

    ObjectHeader *pTableHdr =
      objH_FindProduct(wi.segObj, &wi, EROS_NODE_LGSIZE /* ndx */,
                       wi.canWrite, wi.canCall);

    
    if (pTableHdr == 0)
      pTableHdr = proc_MakeNewPageDirectory(&wi);

    pTable = (PTE *) objC_ObHdrToPage(pTableHdr);

    p->MappingTable = VTOP(pTable);
  }

 have_pgdir:
  
#ifndef NDEBUG
  {
    ObjectHeader *pTableHdr = objC_PhysPageToObHdr(PtoKPA(pTable));

    assert(wi.segBlss == pTableHdr->producerBlss);
    assert(wi.segObj == pTableHdr->prep_u.producer);
    assert(wi.redSeg == pTableHdr->kt_u.mp.redSeg);
    assert(BOOL(wi.canWrite) == BOOL(pTableHdr->rwProduct));
  }
#endif

  {
    /* Start building the PDE entry: */
    uint32_t pdeNdx = (la >> 22) & 0x3ffu;
    PTE* thePDE /*@ not null @*/ = &pTable[pdeNdx];

    if (pte_isnot(thePDE, PTE_V))
      thePDE->w_value = PTE_IN_PROGRESS;

    /* Translate the top 8 (10) bits of the address: */
    if ( !proc_WalkSeg(p, &wi, walk_top_blss, thePDE, 0, true) )
      return false;

    if (thePDE->w_value == PTE_ZAPPED)
      act_Yield(act_Current());

    if (thePDE->w_value == PTE_IN_PROGRESS)
      thePDE->w_value = PTE_ZAPPED;    

    /* If we get this far, we need the page table to proceed further.
     * See if we need to build a new page table:
     */

    if (pte_is(thePDE, PTE_V)) {
      pTable = (PTE *) PTOV(pte_PageFrame(thePDE));

      if (wi.canWrite && !pte_is(thePDE, PTE_W)) {
	/* We could only have taken this fault because writing was not
	 * enabled at the directory level, which means that we need to
	 * flush the PDE from the hardware TLB.
	 */
	pte_Invalidate(thePDE);
      }
    }
    else {
      uint32_t productNdx = 0;

      /* Level 0 product need never be a read-only product.  We use
       * the write permission bit at the PDE level.
       */
      ObjectHeader *pTableHdr =
	objH_FindProduct(wi.segObj, &wi, productNdx, true, true);

      if (pTableHdr == 0)
	pTableHdr = proc_MakeNewPageTable(&wi, productNdx);

      assert(wi.segBlss == pTableHdr->producerBlss);
      assert(wi.segObj == pTableHdr->prep_u.producer);
      assert(wi.redSeg == pTableHdr->kt_u.mp.redSeg);

      /* On x86, the page table is always RW product, and we rely on
	 the write permission bit at the PDE level: */
      assert(pTableHdr->rwProduct == true);

      pTable = (PTE *) objC_ObHdrToPage(pTableHdr);
    }

    /* The level 0 page table is still contentless - there is no
     * need to build depend table entries covering it's contents.
     * We simply need to fill in the page directory entry:
     */

    pte_set(thePDE, (VTOP((kva_t)pTable) & PTE_FRAMEBITS));
    pte_set(thePDE, PTE_ACC|PTE_USER|PTE_V);
    
    /* Using /canWrite/ instead of /isWrite/ reduces the number of
     * cases in which we need to rebuild the PDE without altering the
     * actual permissions, and does not require us to dirty a page.
     * This is legal only because on this architecture the node tree
     * and the page tables are congruent at this level.
     */
    if (wi.canWrite)
      pte_set(thePDE, PTE_W);
  }

#ifdef FAST_TRAVERSAL
 have_good_pde:
#endif
  
  {
    uint32_t pteNdx = (la >> 12) & 0x3ffu;
    PTE* thePTE /*@ not null @*/ = &pTable[pteNdx];
    kpa_t pageAddr = 0;

    if (pte_isnot(thePTE, PTE_V))
      thePTE->w_value = PTE_IN_PROGRESS;
    
    /* Translate the remaining bits of the address: */
    if ( !proc_WalkSeg(p, &wi, EROS_PAGE_BLSS, thePTE, 0, true) )
      return false;

    /* Depend entry triggered -- retry the fault */
    if (thePTE->w_value == PTE_ZAPPED)
      act_Yield(act_Current());

    if (thePTE->w_value == PTE_IN_PROGRESS)
      thePTE->w_value = PTE_ZAPPED;    
    
    assert(wi.segObj);
    assert(wi.segObj->obType == ot_PtDataPage ||
	   wi.segObj->obType == ot_PtDevicePage);
    
   

    if (isWrite)
      objH_MakeObjectDirty(wi.segObj);


    pageAddr = VTOP(objC_ObHdrToPage(wi.segObj));

    if (pageAddr == 0)
      dprintf(true, "wi.segObj 0x%08x at addr 0x%08x!! (wi=0x%08x)\n",
		      wi.segObj, pageAddr, &wi);

    assert ((pageAddr & EROS_PAGE_MASK) == 0);
    assert (pageAddr < PtoKPA(start) || pageAddr >= PtoKPA(end));
	  
    if (isWrite && pte_is(thePTE, PTE_V)) {
      /* We are doing this because the old PTE had insufficient
       * permission, so we must zap the TLB.
       */
      pte_Invalidate(thePTE);
    }
  
    pte_set(thePTE, (pageAddr & PTE_FRAMEBITS) );
    pte_set(thePTE, DATA_PAGE_FLAGS);
    if (isWrite)
      pte_set(thePTE, PTE_W);
#ifdef WRITE_THROUGH
    if (CpuType >= 5)
      pte_set(thePTE, PTE_W);
#endif

#if 0
    if ((wi.segObj->kt_u.ob.oid & OID_RESERVED_PHYSRANGE) 
	== OID_RESERVED_PHYSRANGE)
      wi.canCache = false;
#endif

    if (!wi.canCache)
      pte_set(thePTE, PTE_CD);
  }
    
  UpdateTLB();
    
#ifdef DBG_WILD_PTR
  if (dbg_wild_ptr)
    check_Consistency("End of DoPageFault()");
#endif

  return true;

 access_fault:
  wi.faultCode = FC_Access;
  goto fault_exit;

 fault_exit:
#ifdef WALK_LOUD
    dprintf(true, "flt proc_WalkSeg: wi.producer 0x%x, wi.prodBlss %d wi.isRed %d\n"
		    "wi.vaddr 0x%x wi.offset 0x%X flt %d  wa %d\n"
		    "canCall %d canWrite %d\n",
		    wi.segObj, wi.segBlss, wi.segObjIsRed,
		    wi.vaddr, wi.offset, wi.segFault, wi.writeAccess,
		    wi.canCall, wi.canWrite);
#endif
  if (wi.invokeKeeperOK)
    return proc_InvokeSegmentKeeper(p, &wi);
  return false;
}

static ObjectHeader*
proc_MakeNewPageDirectory(SegWalk* wi /*@ not null @*/)
{
  ObjectHeader *pTable = objC_GrabPageFrame();
  kva_t tableAddr;
  assert (keyR_IsValid(&pTable->keyRing, pTable));
  pTable->obType = ot_PtMappingPage;
  pTable->producerNdx = EROS_NODE_LGSIZE;
  pTable->producerBlss = wi->segBlss;

  pTable->kt_u.mp.redSeg = wi->redSeg;
  pTable->kt_u.mp.wrapperProducer = wi->segObjIsWrapper;
  pTable->kt_u.mp.redSpanBlss = wi->redSpanBlss;
  pTable->rwProduct = BOOL(wi->canWrite);
  pTable->caProduct = BOOL(wi->canCall);
  objH_SetDirtyFlag(pTable);

  tableAddr = objC_ObHdrToPage(pTable);

  bzero((void *) tableAddr, EROS_PAGE_SIZE);

  {
    uint32_t *kpgdir = (uint32_t *) KernPageDir;
    uint32_t *upgdir = (uint32_t *) tableAddr;
    uint32_t i;
    
    for (i = (UMSGTOP >> 22); i < NPTE_PER_PAGE; i++)
      upgdir[i] = kpgdir[i];
  }    

 
  {
    PTE *udir = (PTE *)tableAddr;
    /* Set up fromspace recursive mapping. Note that this is a
       supervisor-only mapping: */
    pte_set(&udir[KVTOL(KVA_FROMSPACE) >> 22], (VTOP(udir) & PTE_FRAMEBITS) );
    pte_set(&udir[KVTOL(KVA_FROMSPACE) >> 22], PTE_W|PTE_V );
  }

  objH_AddProduct(wi->segObj, pTable);
 

  return pTable;
}

static ObjectHeader*
proc_MakeNewPageTable(SegWalk* wi /*@ not null @*/, uint32_t ndx)
{
  /* Need to make a new mapping table: */
  ObjectHeader *pTable = objC_GrabPageFrame();
  kva_t tableAddr;
  assert (keyR_IsValid(&pTable->keyRing, pTable));
  pTable->obType = ot_PtMappingPage;
  pTable->producerNdx = ndx;
  pTable->producerBlss = wi->segBlss;
  
  pTable->kt_u.mp.redSeg = wi->redSeg;
  pTable->kt_u.mp.wrapperProducer = wi->segObjIsWrapper;
  pTable->kt_u.mp.redSpanBlss = wi->redSpanBlss;
  pTable->rwProduct = 1;
  pTable->caProduct = 1;	/* we use spare bit in PTE */
  objH_SetDirtyFlag(pTable);

  tableAddr = objC_ObHdrToPage(pTable);

  bzero((void *)tableAddr, EROS_PAGE_SIZE);

#if 0
  printf("0x%08x->MkPgTbl(blss=%d,ndx=%d,rw=%c,ca=%c,"
		 "producerTy=%d) => 0x%08x\n",
		 wi.segObj,
		 wi.segBlss, ndx, 'y', 'y', wi.segObj->obType,
		 pTable);
#endif

  objH_AddProduct(wi->segObj, pTable);

  return pTable;
}
