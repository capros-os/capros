/*
 * Copyright (C) 1998, 1999, 2001, Jonathan S. Shapiro.
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
Research Projects Agency under Contract No. W31P4Q-07-C-0070.
Approved for public release, distribution unlimited. */

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
#include <kerninc/Process.h>
#include <kerninc/GPT.h>
#include <arch-kerninc/Process.h>
#include "Process486.h"
#include <arch-kerninc/PTE.h>
#include "IDT.h"
#include "lostart.h"
#include "Segment.h"

// #define WALK_LOUD

#define dbg_pgflt	0x1	/* steps in taking snapshot */

/* Following should be an OR of some of the above */
#define dbg_flags   ( 0u )

#define DEBUG(x) if (dbg_##x & dbg_flags)

INLINE bool
obj_IsDirectory(PageHeader * pageH) /* pageH->obType must be ot_PtMappingPage */
{
  return pageH->kt_u.mp.tableSize /* == 1 */ ;
}

/* Possible outcomes of a user-level page fault:
 * 
 * 1. Fault was due to a not-present page, and address is not valid in
 * address space segment.  Process should be faulted and an attempt
 * made to invoke appropriate keeper.
 * 
 * 2. Fault was due to a not-present page, but address IS valid in
 * address space segment.  Segment indicates access is valid.
 * Construct the PDE and PTE for this mapping. If the object
 * referenced is hazarded, construct a write-hazard mapping even if
 * the object is in principle writable. Restart the process.
 * 
 * 3. Fault was an access violation due to writing a page that in
 * principle was read-only.  Process should be faulted and an attempt
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

static PageHeader *
MakeNewPageTable(SegWalk* wi /*@ not null @*/ ); 
static PageHeader *
proc_MakeNewPageDirectory(SegWalk* wi /*@ not null @*/); 


#ifdef OPTION_DDB
#define db_printf printf

void
pte_ddb_dump(PTE* thisPtr)
{
  char attrs[64];
  char *nxtAttr = attrs;
  db_printf("Pg Frame 0x%08x [", pte_PageFrame(thisPtr));

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
  db_printf("%s]\n", attrs);
}

static void
pte_print(uint32_t addr, char *note, PTE *pte)
{
  db_printf("0x%08x %s ", addr, note);
  pte_ddb_dump(pte);
}

void
db_show_mappings_md(uint32_t spaceAddr, uint32_t base, uint32_t nPages)
{
  PTE * space = KPAtoP(PTE *, spaceAddr);
  uint32_t top = base + (nPages * EROS_PAGE_SIZE);

  while (base < top) {
    uint32_t hi = (base >> 22) & 0x3ffu;
    uint32_t lo = (base >> 12) & 0x3ffu;

    PTE * pde = space + hi;
    pte_print(base, "PDE ", pde);
    if (!pte_isValid(pde))
      db_printf("0x%08x PTE <invalid>\n", base);
    else {
      PTE *pte = KPAtoP(PTE *,pte_PageFrame(pde));
      uint32_t frm = 0;
      pte += lo;

      pte_print(base, "PTE ", pte);
      frm = pte_PageFrame(pte);
      PageHeader * pHdr = objC_PhysPageToObHdr(frm);

      if (pHdr == 0)
	db_printf("*** NOT A VALID USER FRAME!\n");
      else if (pageH_GetObType(pHdr) != ot_PtDataPage)
	db_printf("*** FRAME IS INVALID TYPE!\n");

    }

    base += EROS_PAGE_SIZE;
  }
}
#endif

#ifndef NDEBUG
bool
pte_ObIsNotWritable(PageHeader * pageH)
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
  uint32_t kvaw = pageH_GetPageVAddr(pageH);

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
		      pageH, kvaw,
		      pageH, &pte[i]);
      result = false;
    }
  }
#endif

  for (pf = 0; pf < objC_TotalPages(); pf++) {
    PageHeader * pHdr = objC_GetCorePageFrame(pf);

    if (pageH_GetObType(pHdr) != ot_PtMappingPage)
      continue;

    ptepg = (PTE *) pageH_GetPageVAddr(pHdr);
    
    limit = NPTE_PER_PAGE;
    if (obj_IsDirectory(pHdr))
      limit = (UMSGTOP >> 22);	/* PAGE_ADDR_BITS + PAGE_TABLE_BITS */

    for (ent = 0; ent < limit; ent++) {
      if ((ptepg[ent].w_value & (PTE_FRAMEBITS|PTE_W)) == kvaw) {
	dprintf(true,
			"Checking pobj 0x%x with frame at 0x%x\n"
			"Page hdr 0x%x retains writable PTE at 0x%x\n",
			pageH, kvaw,
			pHdr, &ptepg[ent]);
	result = false;
      }
    }
  }

  return result;
}
#endif /* !NDEBUG */

/* Walk the current object's products looking for an acceptable product: */
static PageHeader *
FindProduct(SegWalk * wi,
            unsigned int tblSize, 
            bool rw)
{
  ObjectHeader* thisPtr = wi->memObj;
  
  /* #define FINDPRODUCT_VERBOSE */

#ifdef FINDPRODUCT_VERBOSE
  printf("Search for product rw=%c tblSize=%d\n",
	       rw ? 'y' : 'n', tblSize);
#endif

  MapTabHeader * product;
  
  for (product = thisPtr->prep_u.products;
       product; product = product->next) {
    assert(pageH_GetObType(MapTab_ToPageH(product)) == ot_PtMappingPage);
    if ((uint32_t) product->tableSize != tblSize) {
#ifdef FINDPRODUCT_VERBOSE
      printf("tableSize not match\n");
#endif
      continue;
    }
    if (product->readOnly == rw) {	// rw is 0 or 1
#ifdef FINDPRODUCT_VERBOSE
      printf("rw not match\n");
#endif
      continue;
    }
    if (product->backgroundGPT != wi->backgroundGPT) {
#ifdef FINDPRODUCT_VERBOSE
      printf("backgroundGPT not match: prod 0x%x caller 0x%x\n",
             product->backgroundGPT, wi->backgroundGPT);
#endif
      continue;
    }

    /* WE WIN! */
    break;
  }

  if (product) {
    assert(product->producer == thisPtr);
  }

#ifdef FINDPRODUCT_VERBOSE
  printf("FindProduct() => 0x%08x\n", product);
#endif

  return MapTab_ToPageH(product);
}

#ifdef INVOKE_TIMING
extern "C" {
  uint64_t rdtsc();
};
#endif

/* May Yield. */
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
  Process *ctxt;

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

    fatal("Kernel page fault\n"
		  " SaveArea      =  0x%08x  EIP           =  0x%08x\n"
		  " Fault address =  0x%08x  Code          =  0x%08x\n"
		  " CS            =  0x%08x\n",
		  sa, sa->EIP,
		  la, error, sa->CS);
  }

  /* Process page fault. */

  ctxt = act_CurContext();

  assert(& ctxt->trapFrame == sa);

  ctxt->stats.pfCount++;

  KernStats.nPfTraps++;
  if (writeAccess) KernStats.nPfAccess++;

  objH_BeginTransaction();

  (void) proc_DoPageFault(ctxt, la, writeAccess, false);

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

static void
SegWalk_InitFromMT(SegWalk * wi, MapTabHeader * mth)
{
  wi->backgroundGPT = mth->backgroundGPT;
  wi->keeperGPT = SEGWALK_GPT_UNKNOWN;
  wi->memObj = mth->producer;
}

uint32_t DoPageFault_CallCounter;

/* May Yield. */
bool
proc_DoPageFault(Process * p, ula_t la, bool isWrite, bool prompt)
{
  const int walk_root_l2v = 32;
  const int walk_top_l2v = 22;

#ifdef DBG_WILD_PTR
  if (dbg_wild_ptr)
    check_Consistency("Top of DoPageFault()");
#endif

  DoPageFault_CallCounter++;
  
  DEBUG(pgflt) {
    printf("DoPageFlt: proc=0x%08x EIP 0x%08x la=0x%08x smallPTE=0x%x %s%s\n",
		   p,
		   p->trapFrame.EIP,
		   la,
                   p->md.smallPTE,
		   isWrite ? "wt" : "rd",
		   prompt ? " prompt" : "" );
  }
  
#ifdef OPTION_SMALL_SPACES
  uva_t va = la - p->md.bias;
  if (va >= p->md.limit) {
    /* If va is simply out of range, then forget the whole thing: */
    if (va >= UMSGTOP) {
      dprintf(true, "Process accessed va 0x%08x, too high.\n",
	      (uint32_t) va);
      proc_SetFault(p, FC_InvalidAddr, va, false);  
      return false;
    }
    /* The address exceeds the small space limit. We need to
      convert the current address space into a large space.
    
      Set smallPTE to zero and YIELD, because we
      might have been called from inside the IPC code.  This will
      induce another page fault, which will follow the large space path.

      This should not happen on the invoker side, where the addresses
      were validated during page probe (causing SS fault or GP fault).
      On the invokee side, however, it is possible that we forced the
      invokee to reload, in which case it loaded as a small space, and
      we then tried to validate the receive address.  In that case,
      proc_SetupExitString() called us here with an out of bounds virtual
      address.  We reset the process mapping state and Yield(),
      allowing the correct computation to be done in the next pass.
      */

    /* This code seems not to be reached.
       If the address exceeds the small space limit, the fault seems 
       to come in as a General Protection fault. */

#if 0
    dprintf(true, "!! la=0x%08x va=0x%08x\n"
		    "Switching process 0x%X to large space\n",
		    la, va,
		    procRoot->ob.oid);
#endif

    proc_SwitchToLargeSpace(p);
    act_Yield();
  }
#else
  uva_t va = la;
  
  /* If va is simply out of range, then forget the whole thing: */
  if (va >= UMSGTOP) {
    dprintf(true, "Process accessed va 0x%08x, too high.\n",
	    (uint32_t) va);
    proc_SetFault(p, FC_InvalidAddr, va, false);  
    return false;
  }
#endif

  /* Set up a SegWalk structure. */
  SegWalk wi;
  wi.needWrite = isWrite;
  wi.traverseCount = 0;

  PTE * thePTE;
#ifdef OPTION_SMALL_SPACES
  if (p->md.smallPTE) {
    assert (va < (SMALL_SPACE_PAGES * EROS_PAGE_SIZE));

#if 0
    dprintf(true, "SmallPgFlt w/ la=0x%08x, bias=0x%08x,"
		  " limit=0x%08x\n", la, p->md.bias, p->md.limit);
#endif

#if 0
    /* This assertion can be false given that the process can run from
     * any address space and the save logic may subsequently save that
     * address space pointer. */
    assert ( fixRegs.md.MappingTable == KERNPAGEDIR );
#endif

    /* A small space uses only a fraction of a page table.
       In any case, all the page tables for small spaces are contigous.
       So just index off of smallPTE. */
    thePTE = &p->md.smallPTE[(va >> EROS_PAGE_ADDR_BITS)
                             % SMALL_SPACE_PAGES ];

    DEBUG(pgflt)
      printf("Found small pgdir\n");

    /* In a small space, the PTE is dependent on the entire path
       from the root: */
    if (! segwalk_init(&wi, node_GetKeyAtSlot(p->procRoot, ProcAddrSpace),
                       va, thePTE, 2)) {
      goto fault_exit;
    }

#ifdef WALK_LOUD
    dprintf(false, "have small pde, ");
#endif
  }
  else	// the large space case follows
#endif
  {	// beginning of large space case
    PTE * pTable;
    PageHeader * pTableHdr;

    /* See if there is already a page directory. */
    if (p->md.MappingTable == KernPageDir_pa
        || p->md.MappingTable == PTE_IN_PROGRESS ) {	// Need a page directory
      p->md.MappingTable = PTE_IN_PROGRESS;
  
      if (! segwalk_init(&wi, node_GetKeyAtSlot(p->procRoot, ProcAddrSpace),
                         va, p, 0)) {
        goto fault_exit;
      }

      /* Begin the traversal... */
      if ( ! WalkSeg(&wi, walk_root_l2v,
		     p, 0) ) { 
        goto fault_exit;
      }

      DEBUG(pgflt)
        printf("Traversed to top level\n");

      /* If a depend entry was reclaimed, we may have just zapped
       * the very mapping table entry we are building: */
      if (p->md.MappingTable != PTE_IN_PROGRESS) {
        dprintf(true, "Zapped MT root\n");
        act_Yield();
      }
  
      /* See if a mapping table has already been built for this address
       * space.  If so, just use it. */
      pTableHdr =
        FindProduct(&wi, 1, ! (wi.restrictions & capros_Memory_readOnly));
    
      if (pTableHdr == 0) {
        pTableHdr = proc_MakeNewPageDirectory(&wi);

#ifdef WALK_LOUD
        dprintf(false, "new pgdir, ");
#endif
      } else {
#ifdef WALK_LOUD
        dprintf(false, "found pgdir, ");
#endif
      }

      assert(pTableHdr && pageH_GetObType(pTableHdr) == ot_PtMappingPage);

      pTable = (PTE *) pageH_GetPageVAddr(pTableHdr);

      /* Note, the physical address of the page directory must be
         representable in 32 bits, even in PAE mode. */
      p->md.MappingTable = (kpmap_t)VTOP(pTable);

    } else {		// Already have a page directory
      pTable = (PTE *) (PTOV(p->md.MappingTable) & ~EROS_PAGE_MASK);
      pTableHdr = objC_PhysPageToObHdr(PtoKPA(pTable));
      assert(pTableHdr && pageH_GetObType(pTableHdr) == ot_PtMappingPage);

      DEBUG(pgflt)
        printf("Found top level\n");
    
      SegWalk_InitFromMT(&wi, &pTableHdr->kt_u.mp);
      wi.offset = va;
      wi.restrictions = pTableHdr->kt_u.mp.readOnly
                        ? capros_Memory_readOnly : 0;
   
      if (isWrite && pTableHdr->kt_u.mp.readOnly) {
        wi.faultCode = FC_Access;
        goto fault_exit;
      }

#ifdef WALK_LOUD
      dprintf(false, "have pgdir, ");
#endif
    }
  
    assert(wi.memObj == pTableHdr->kt_u.mp.producer);
    
    /* pTable is a page directory conveying suitable access rights from
       the top, and pTableHdr is its header.
       wi reflects the path to this point. */

    /* See if the PDE has the necessary permissions: */
    uint32_t pdeNdx = (la >> 22) & 0x3ffu;
    PTE * thePDE = &pTable[pdeNdx];

    if ( pte_is(thePDE, PTE_V) ) {
      if (isWrite && ! pte_is(thePDE, PTE_W)) {
        wi.faultCode = FC_Access;
        goto fault_exit;
      }

      /* We have a valid PDE with the necessary permissions! */
      pTable = KPAtoP(PTE *, pte_PageFrame(thePDE));
      pTableHdr = objC_PhysPageToObHdr(PtoKPA(pTable));
      assert(pTableHdr && pageH_GetObType(pTableHdr) == ot_PtMappingPage);

      DEBUG(pgflt)
        printf("Found second level\n");
        
      SegWalk_InitFromMT(&wi, &pTableHdr->kt_u.mp);
      wi.offset = va & ((1ul << 22) - 1);
      /* wi.restrictions & capros_Memory_readOnly must be 0.
         wi.restrictions & capros_Memory_noCall is not significant
         because wi.keeperGPT is SEGWALK_GPT_UNKNOWN. */
      wi.restrictions = 0;

#ifdef WALK_LOUD
      dprintf(false, "have pt, ");
#endif
    } else {
      // user PDE is not valid.

      /* Start building the PDE entry: */

      thePDE->w_value = PTE_IN_PROGRESS;

      /* Translate the top 8 (10) bits of the address: */
      if ( ! WalkSeg(&wi, walk_top_l2v, thePDE, 1) ) {
        goto fault_exit;
      }

      DEBUG(pgflt)
        printf("Traversed to second level\n");

      if (thePDE->w_value == PTE_ZAPPED)
        act_Yield();
      assert(thePDE->w_value == PTE_IN_PROGRESS);

      /* If we get this far, we need the page table to proceed further.
       * See if we need to build a new page table: */

      /* Level 0 product is never a read-only product.  We use
       * the write permission bit at the PDE level.
       */
      pTableHdr = FindProduct(&wi, 0, true);

      if (pTableHdr == 0) {
        pTableHdr = MakeNewPageTable(&wi);

#ifdef WALK_LOUD
        dprintf(false, "new pt, ");
#endif
      } else {
#ifdef WALK_LOUD
        dprintf(false, "found pt, ");
#endif
      }
      assert(pageH_GetObType(pTableHdr) == ot_PtMappingPage);

      /* On x86, the page table is always RW, and we rely on
         the write permission bit at the PDE level: */
      assert(! pTableHdr->kt_u.mp.readOnly);

      pTable = (PTE *) pageH_GetPageVAddr(pTableHdr);

      thePDE->w_value = 0;    
      pte_set(thePDE, (VTOP((kva_t)pTable) & PTE_FRAMEBITS));
      pte_set(thePDE, PTE_ACC|PTE_USER|PTE_V);
      if (! (wi.restrictions & capros_Memory_readOnly))
        pte_set(thePDE, PTE_W);
      else {
        /* Having captured the readOnly restriction to this point,
           the PTE must be RO only if there is an RO flag
           from this point forward, because we may share this page table
           with address spaces where it is writeable. */
        wi.restrictions &= ~ capros_Memory_readOnly;
      }
    }

    // Now we have a good PDE.
  
    thePTE = &pTable[(la >> 12) & 0x3ff];
  }	// end of large space case

  /* thePTE points to the PTE in question. */

  if (pte_is(thePTE, PTE_V)) {
    if (! isWrite || pte_is(thePTE, PTE_W)) {
#ifdef WALK_LOUD
      dprintf(false, "have pte\n");
#endif
      return true;	/* This PTE already has everything we need. */
    }
    /* The old PTE had insufficient permission.
    Leave it valid while we call WalkSeg, in case we fault or Yield. */
  } else {
    /* We need to have a value other than PTE_ZAPPED in the PTE
    so we can recognize if a Depend zap occurs. */
    thePTE->w_value = PTE_IN_PROGRESS;
  }
  
  /* Do the traversal... */
  if ( ! WalkSeg(&wi, EROS_PAGE_LGSIZE, thePTE, 2) ) {
    goto fault_exit;
  }

  DEBUG(pgflt)
    printf("Traversed to page\n");

  if (thePTE->w_value == PTE_ZAPPED)
    /* Depend entry triggered -- retry the fault */
    act_Yield();
    
  assert(wi.memObj->obType == ot_PtDataPage
         || wi.memObj->obType == ot_PtDevicePage);
  
  if (isWrite) {
    pageH_MakeDirty(objH_ToPage(wi.memObj));
  }

  kpa_t pageAddr = VTOP(pageH_GetPageVAddr(objH_ToPage(wi.memObj)));

  assert ((pageAddr & EROS_PAGE_MASK) == 0);
  // Must not be within the kernel:
  assert (pageAddr < PtoKPA(&_start) || pageAddr >= PtoKPA(&end));
  
  pte_Invalidate(thePTE);	/* remember to purge TLB */

  pte_set(thePTE, (pageAddr & PTE_FRAMEBITS) | (PTE_ACC|PTE_USER|PTE_V));
  if (isWrite)
    pte_set(thePTE, PTE_W);
#ifdef WRITE_THROUGH
  if (CpuType >= 5)
    pte_set(thePTE, PTE_WT);
#endif

#ifdef WALK_LOUD
      dprintf(false, "set pte\n");
#endif

#if 0
  if ((wi.memObj->kt_u.ob.oid & OID_RESERVED_PHYSRANGE) 
	== OID_RESERVED_PHYSRANGE)
    pte_set(thePTE, PTE_CD);
#endif
    
#ifdef DBG_WILD_PTR
  if (dbg_wild_ptr)
    check_Consistency("End of DoPageFault()");
#endif

  DEBUG(pgflt)
    printf("Finished page fault\n");

  return true;

 fault_exit:
#ifdef WALK_LOUD
    dprintf(true, "flt WalkSeg 0x%x\n", &wi);
#endif
  if (!prompt)
    proc_InvokeSegmentKeeper(p, &wi, true, va);
  return false;
}

/* May Yield. */
static PageHeader *
proc_MakeNewPageDirectory(SegWalk* wi /*@ not null @*/)
{
  PageHeader * pTable = objC_GrabPageFrame();

  pTable->kt_u.mp.obType = ot_PtMappingPage;
  pTable->kt_u.mp.tableSize = 1;

  pTable->kt_u.mp.backgroundGPT = wi->backgroundGPT;
  pTable->kt_u.mp.readOnly = BOOL(wi->restrictions & capros_Memory_readOnly);

  kva_t tableAddr = pageH_GetPageVAddr(pTable);

  bzero((void *) tableAddr, EROS_PAGE_SIZE);

  {	// Copy the kernel address space from UMSGTOP up.
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
    /* Note, we are not using PAE or PSE-36, so can only use 
       32-bit physical address. */
    pte_set(&udir[KVTOL(KVA_FROMSPACE) >> 22],
            ((uint32_t)(VTOP(udir)) & PTE_FRAMEBITS) );
    pte_set(&udir[KVTOL(KVA_FROMSPACE) >> 22], PTE_W|PTE_V );
  }

  objH_AddProduct(wi->memObj, &pTable->kt_u.mp);

  return pTable;
}

/* May Yield. */
static PageHeader *
MakeNewPageTable(SegWalk* wi /*@ not null @*/ )
{
  /* Need to make a new mapping table: */
  PageHeader * pTable = objC_GrabPageFrame();
  pTable->kt_u.mp.obType = ot_PtMappingPage;
  pTable->kt_u.mp.tableSize = 0;
  
  pTable->kt_u.mp.backgroundGPT = wi->backgroundGPT;
  /* All page tables have readOnly == 0. We use the readOnly protection
  in the page directory. */
  pTable->kt_u.mp.readOnly = 0;

  kva_t tableAddr = pageH_GetPageVAddr(pTable);

  bzero((void *)tableAddr, EROS_PAGE_SIZE);

#if 0
  printf("0x%08x->MkPgTbl(blss=%d,ndx=%d,rw=%c,ca=%c,"
		 "producerTy=%d) => 0x%08x\n",
		 wi.memObj,
		 wi.segBlss, ndx, 'y', 'y', wi.memObj->obType,
		 pTable);
#endif

  objH_AddProduct(wi->memObj, &pTable->kt_u.mp);

  return pTable;
}
