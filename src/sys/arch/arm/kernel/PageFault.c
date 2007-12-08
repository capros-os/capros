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
Research Projects Agency under Contract Nos. W31P4Q-06-C-0040 and
W31P4Q-07-C-0070.  Approved for public release, distribution unlimited. */

#include <string.h>
#include <kerninc/kernel.h>
#include <kerninc/Check.h>
#include <kerninc/KernStats.h>
#include <kerninc/Activity.h>
#include <kerninc/Node.h>
#include <kerninc/ObjectCache.h>
#include <kerninc/Machine.h>
#include <kerninc/Debug.h>
#include <kerninc/Process.h>
#include <kerninc/GPT.h>
#include <arch-kerninc/Process.h>
#include <arch-kerninc/PTE.h>
#include <arch-kerninc/IRQ-inline.h>
#include <idl/capros/GPT.h>

MapTabHeader * AllocateSmallSpace(void);
unsigned int EnsureSSDomain(unsigned int ssid);

// #define WALK_LOUD

#define dbg_pgflt	0x1	/* steps in taking snapshot */
#define dbg_cache	0x2

/* Following should be an OR of some of the above */
#define dbg_flags   ( 0u )

#define DEBUG(x) if (dbg_##x & dbg_flags)

/* Possible outcomes of a user-level page fault:
 * 
 * 1. Fault was due to a not-present page, and address is not valid in
 * address space segment.  Process should be faulted and an attempt
 * made to invoke appropriate keeper.
 * 
 * 2. Fault was due to a not-present page, but address IS valid in
 * address space segment.
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
 * CapROS does not provide write-only or execute-only mappings.
 */

#ifndef NDEBUG
extern void end();
extern void start();
#endif

#ifdef OPTION_DDB
#define db_printf printf

void
pte_ddb_dump(PTE* thisPtr)
{
  uint32_t pte = pte_AsWord(thisPtr);
  
  switch (pte & PTE_VALIDBITS) {
  case 1:
  case 3:
    db_printf("bad 0x%08x\n", pte);
    break;

  case 0:
    if (pte & PTE_CACHEABLE) {
      db_printf("tracking LRU ");	// and fall into the valid case
    } else if (pte & PTE_BUFFERABLE) {
      db_printf("in progress\n");
      break;
    } else {
      db_printf("no access\n");
      break;
    }
  case PTE_SMALLPAGE:
  {
    // All AP fields should match.
    unsigned int ap = (pte >> 10) & 3;
    if (((pte >> 8) & 3) != ap
        || ((pte >> 6) & 3) != ap
        || ((pte >> 4) & 3) != ap ) {
      db_printf("bad 0x%08x\n", pte);
    } else {
      uint32_t frm = pte_PageFrame(thisPtr);
      db_printf("Pg Frame 0x%08x ap=%d %c%c",
             pte_PageFrame(thisPtr), ap,
             (pte & PTE_CACHEABLE  ? 'C' : ' '),
             (pte & PTE_BUFFERABLE ? 'B' : ' ') );
      switch (ap) {
      case 0:
        db_printf(" kro\n");
        break;
      case 1:
        db_printf(" krw\n");
        break;
      case 2:
        if (pte & PTE_BUFFERABLE) {
          db_printf(" uro\n");
        } else {
          db_printf(" uro tracking dirty\n");
        }
        break;
      case 3:
        db_printf(" urw\n");
      }

      PageHeader * pHdr = objC_PhysPageToObHdr(frm);
      if (pHdr == 0)
	db_printf("*** NOT A VALID USER FRAME!\n");
      else if (pageH_GetObType(pHdr) != ot_PtDataPage)
	db_printf("*** FRAME IS INVALID TYPE!\n");
    }
  }
  }
}

void
db_show_mappings_md(uint32_t spaceAddr, uint32_t base, uint32_t nPages)
{
  uint32_t * space = KPAtoP(uint32_t *, spaceAddr);
  uint32_t top = base + (nPages * EROS_PAGE_SIZE);
  while (base < top) {
    uint32_t hi = base >> L1D_ADDR_SHIFT;
    uint32_t lo = (base & CPT_ADDR_MASK) >> EROS_PAGE_LGSIZE;

    uint32_t l1d = space[hi];	// level 1 descriptor
    db_printf("0x%08x L1D ", base);
    switch (l1d & L1D_VALIDBITS) {
    case 0:
      db_printf("invalid\n");
      base = base | 0xff000;	// skip all other pages in this section
      break;

    case 1:	// CPT descriptor
      db_printf("CPT pa=0x%08x, dom=%d ",
                l1d & L1D_COARSE_PT_ADDR, 
                (l1d & L1D_DOMAIN_MASK) >> L1D_DOMAIN_SHIFT);

      PTE * pte = KPAtoP(PTE *, l1d & L1D_COARSE_PT_ADDR);
      pte += lo;

      db_printf("PTE ");
      pte_ddb_dump(pte);

      break;

    case 2:	// section descriptor
      db_printf("Section pa=0x%08x, ap=%d, dom=%d, c=%d, b=%d\n",
                l1d & 0xfff00000,
                (l1d >> 10) & 0x3,
                (l1d & L1D_DOMAIN_MASK) >> L1D_DOMAIN_SHIFT,
                BoolToBit(l1d & PTE_CACHEABLE),
                BoolToBit(l1d & PTE_BUFFERABLE) );
      base = base | 0xff000;	// skip all other pages in this section
      break;

    case 3:	// fine page table descriptor, not used
      assert(false);
      break;
    }

    base += EROS_PAGE_SIZE;
  }
}
#endif

#ifndef NDEBUG
bool
pte_ObIsNotWritable(PageHeader * pageH)
{
  uint32_t pf;
  uint32_t ent;
  
  /* Start by building a writable PTE for the page: */
  uint32_t pagePA = VTOP(pageH_GetPageVAddr(pageH));

#ifdef OPTION_SMALL_SPACES
  /* Check small spaces first: */
  for (ent = 0; ent < (UserEndVA >> L1D_ADDR_SHIFT); ent++) {
    if ((FLPT_FCSEVA[ent] & L1D_VALIDBITS) == (L1D_COARSE_PT & L1D_VALIDBITS)) {
      if ((FLPT_FCSEVA[ent] & L1D_COARSE_PT_ADDR) == pagePA) {
        dprintf(true, "ObIsNotWriteable failed\n");
        return false;
      }
    }
  }
#endif

  for (pf = 0; pf < objC_TotalPages(); pf++) {
    PageHeader * pHdr = objC_GetCorePageFrame(pf);

    if (pageH_GetObType(pHdr) != ot_PtMappingPage2)
      continue;

    uint32_t * ptepg;
    
    if (pHdr->kt_u.mp.hdrs[0].tableSize) {
      /* Scan the entries in this FLPT. */
      ptepg = (uint32_t *) pageH_GetPageVAddr(pHdr);
      for (ent = 0; ent < (UserEndVA >> L1D_ADDR_SHIFT); ent++) {
        if ((ptepg[ent] & L1D_VALIDBITS) == (L1D_COARSE_PT & L1D_VALIDBITS)) {
          if ((ptepg[ent] & L1D_COARSE_PT_ADDR) == pagePA) {
            dprintf(true, "ObIsNotWriteable failed\n");
            return false;
          }
        }
      }
    } else {
      int i;
      for (i=0; i<4; i++) {
        MapTabHeader * mth = & pHdr->kt_u.mp.hdrs[i];
        if (! mth->isFree) {
          ptepg = (uint32_t *) MapTabHeaderToKVA(mth);
          /* Scan the entries in this page table. */
          for (ent = 0; ent < CPT_ENTRIES; ent++) {
            if ((ptepg[ent] & PTE_VALIDBITS) == PTE_SMALLPAGE) {
              if ((ptepg[ent] & PTE_FRAMEBITS) == pagePA	/* this page */
                  && (ptepg[ent] & 0xff0) == 0xff0 ) {	/* and writeable */
                dprintf(true, "ObIsNotWriteable failed\n");
                return false;
              }
            }
          }
        }
      }
    }
  }

  return true;
}
#endif /* !NDEBUG */

/* Initialize a new MapTabHeader. */
static void
InitMapTabHeader(MapTabHeader * mth,
                 SegWalk * wi /*@ not null @*/, uint32_t ndx)
{
  DEBUG(pgflt) printf("InitMapTabHeader ");

  mth->producerNdx = ndx;
  mth->backgroundGPT = wi->backgroundGPT;
  mth->readOnly = BOOL(wi->restrictions & capros_Memory_readOnly);

  objH_AddProduct(wi->memObj, mth);
}

static void
pageH_MakeUncached(PageHeader * pageH)
{
  KernStats.nPageUncache++;

  /* Unmap all references to this page, because they are cacheable. */
  keyR_UnmapAll(&pageH_ToObj(pageH)->keyRing);
  pageH->kt_u.ob.cacheAddr = CACHEADDR_UNCACHED;
  /* If and when PTEs to this page are rebuilt, they will be uncached. */
  flushCache = true;
}

/* Walk the current object's products looking for an acceptable product: */
static MapTabHeader *
FindProduct(SegWalk * wi,
            unsigned int tblSize, 
            unsigned int producerNdx, 
            ula_t va)
{
  ObjectHeader * thisPtr = wi->memObj;

#if 0
  printf("Search for product ndx=%d, producerTy=%d\n",
	       ndx, obType);
#endif
  
// #define FINDPRODUCT_VERBOSE

  MapTabHeader * product;
  
  for (product = thisPtr->prep_u.products;
       product; product = product->next) {
    if ((uint32_t) product->tableSize != tblSize) {
#ifdef FINDPRODUCT_VERBOSE
	printf("tableSize not match: prod %d caller %d\n",
		       product->tableSize, tblSize);
#endif
      continue;
    }
    if ((uint32_t) product->producerNdx != producerNdx) {
      continue;
    }

    // Use ! below to convert to 0 or 1. 
    int rw = ! (wi->restrictions & capros_Memory_readOnly);
    if (product->readOnly == rw) {
#ifdef FINDPRODUCT_VERBOSE
	printf("ro not match: prod %d caller %x\n",
		       product->readOnly, wi->restrictions);
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

    if (tblSize == 0) {	// second level page table
      // va is the MVA where the table will be mapped.
      if (rw) {
        // If writeable, tableCacheAddr must match.
        if (product->tableCacheAddr != va) {
#ifdef FINDPRODUCT_VERBOSE
          printf("tableCacheAddr not match: prod 0x%08x caller 0x%08x\n",
		       product->tableCacheAddr, va);
#endif
          continue;
        }
      }
    } else {	// first level page table
      /* If all the above tests passed, a producer should not produce
      both a large and a small space. */
    }

    /* WE WIN! */
    break;
  }

  if (product) {
    assert(product->producer == thisPtr);
  }

#ifdef FINDPRODUCT_VERBOSE
  printf("0x%08x->FindProduct(sz=%d,ndx=%d,"
		 "producrTy=%d) => 0x%08x\n",
		 thisPtr, tblSize,
		 producerNdx,
                 thisPtr->obType,
		 product);
#endif

  return product;
}

/* Handle page fault from user or system mode.
   This is called from the Abort exception handlers.
   This procedure does not return - it calls ExitTheKernel().  */
void
PageFault(unsigned int type,
          uint32_t fsr,	/* fault status */
          ula_t fa)	/* fault address, if data abort */
{
  Process * proc = act_CurContext();
  bool writeAccess = false;
  uva_t va;	/* the unmodified virtual address */

  assert(irq_DISABLE_DEPTH() == 1);	// disabled right after exception

  switch (type) {
  case prefetchAbort:
    writeAccess = false;
    va = proc->trapFrame.r15;	/* his pc */

#ifndef NDEBUG
    if (dbg_inttrap)
      printf("Prefetch PageFault fa=0x%08x pc=0x%08x\n",
             va, proc->trapFrame.r15);
#endif
    DEBUG(pgflt)
      printf("Prefetch PageFault fa=0x%08x pc=0x%08x\n",
             va, proc->trapFrame.r15);
    break;

  case dataAbort: ;
    /* fa has the modified virtual address */
    ula_t pidAddr = proc->md.pid;
    if ((fa & PID_MASK) == pidAddr) {
      /* Fault address could have been generated by adding the PID;
         assume it was. */
      va = fa - pidAddr;
    } else {
      va = fa;
    }

#ifndef NDEBUG
    if (dbg_inttrap)
      printf("Data PageFault fa=0x%08x fsr=0x%08x pc=0x%08x r12=0x%08x sp=0x%08x r14=0x%08x\n",
             va, fsr, proc->trapFrame.r15, proc->trapFrame.r12,
             proc->trapFrame.r13, proc->trapFrame.r14);
#endif
    DEBUG(pgflt)
      printf("Data PageFault fa=0x%08x fsr=0x%08x pc=0x%08x r12=0x%08x sp=0x%08x r14=0x%08x\n",
             va, fsr, proc->trapFrame.r15, proc->trapFrame.r12,
             proc->trapFrame.r13, proc->trapFrame.r14);

    /* What is the problem? */
    switch (fsr & 0xd) {
    /* the following are all the cases that the ARM920T generates. */
    case 0x1:	/* alignment fault */
      proc_SetFault(proc, capros_Process_FC_Alignment, va);
      ExitTheKernel();
      break;

    case 0x5:	/* translation fault */
      /* There was a non-valid section or page descriptor.
         We don't know whether writing was attempted, so assume not. */
      writeAccess = false;
      break;

    case 0x9:	/* domain fault */
      /* He accessed a domain for which the DACR says "no access". */
      if (proc->md.dacr == 0x1) {
        /* He does not have any domain set up. */
        writeAccess = false;
      } else {
        /* He referenced a domain other than his own. Fault him. */
        printf("Domain fault at 0x%08x.", fa);
        fatal(" trap unimplemented");
      }
      break;

    case 0xd: {	/* permission fault */
      /* Get the descriptor to find out if the fault is due to
         lack of write access. */
      uint32_t * flmt = KPAtoP(uint32_t *,
                               proc->md.firstLevelMappingTable);
      uint32_t l1d = flmt[fa >> L1D_ADDR_SHIFT];	/* level 1 descriptor */
      uint32_t pte = 0;
      PTE * cpt;
      switch (l1d & 0x3) {
      case 0x0:	/* fault - can't happen, because this generates
		a   translation fault, not a permission fault */
      case 0x3:	/* fine page table - we never use this */
        fatal("Permission fault with invalid L1D\n");

      case 0x1:	/* coarse page table */
        cpt = KPAtoP(PTE *, l1d & L1D_COARSE_PT_ADDR);
        pte = pte_AsWord(& cpt[(fa & CPT_ADDR_MASK) >> EROS_PAGE_ADDR_BITS]);
        /* Must be a small page, because we never user large or tiny pages,
           and this is a permission fault. */
        assert((pte & PTE_VALIDBITS) == PTE_SMALLPAGE);
        /* All access permission fields are the same. */
        break;

      case 0x2:	/* section */
        pte = l1d;	/* just using bits 10:11 */
      }
      /* If he has read-only access and got a permission fault,
      it must be because he attempted to write.
      From User mode, that occurs when the AP bits (10:11) are 0b10.
      From System (privileged) mode, that occurs when writing to
      kernel code, which shouldn't happen. */
      writeAccess = (pte & 0xc00) == 0x800;
      break;
    }

    case 0x8:	/* external abort */
        printf("External abort at 0x%08x.", fa);
        fatal(" trap unimplemented");
    }
    break;

  case CSwapLoad:
    /* We faulted on the read portion of a CSwap.
    The write portion is not inevitable (if the old value doesn't match),
    so just treat as a read fault. */
    writeAccess = false;
    va = proc->trapFrame.r0;
    break;

  case CSwapStore:
    writeAccess = true;
    va = proc->trapFrame.r0;
    break;

  default: assert(false);
    va = 0;	// to avert a compiler warning
  }
  /* va and writeAccess are now set. */

  proc->stats.pfCount++;

  KernStats.nPfTraps++;
  if (writeAccess) KernStats.nPfAccess++;

  objH_BeginTransaction();

  (void) proc_DoPageFault(proc, va, writeAccess, false);

  /* No need to release uncommitted I/O page frames -- there should
   * not be any.
   */

  ExitTheKernel();
}

static void
SegWalk_InitFromMT(SegWalk * wi, MapTabHeader * mth)
{
  wi->backgroundGPT = mth->backgroundGPT;
  wi->keeperGPT = SEGWALK_GPT_UNKNOWN;
  wi->memObj = mth->producer;
  wi->restrictions = mth->readOnly ? capros_Memory_readOnly : 0;
}

uint32_t DoPageFault_CallCounter;

/* Returns ... */
// May Yield.
bool
proc_DoPageFault(Process * p, uva_t va, bool isWrite, bool prompt)
{
  const int walk_root_l2v = 32;
  const int walk_top_l2v = L1D_ADDR_SHIFT;
  
#ifdef DBG_WILD_PTR
  if (dbg_wild_ptr)
    check_Consistency("Top of DoPageFault()");
#endif

  DoPageFault_CallCounter++;
  
  DEBUG(pgflt) {
    printf("DoPageFault: proc=0x%08x PC 0x%08x va=0x%08x, isWrite=%c prompt=%c\n",
		   p,
		   p->trapFrame.r15,
		   va,
		   isWrite ? 't' : 'f',
		   prompt ? 't' : 'f');
  }
  
  /* If va is simply out of range, then forget the whole thing: */
  if (va >= UserEndVA) {
#if 0
    dprintf(true, "Process accessed kernel space at 0x%08x, pc=0x%08x\n",
            va, p->trapFrame.r15);
#endif
    proc_SetFault(p, capros_Process_FC_InvalidAddr, va);
    return false;
  }

  MapTabHeader * mth1;
  if (p->md.firstLevelMappingTable == FLPT_FCSEPA) {
    if (p->md.pid != 0 && p->md.pid != PID_IN_PROGRESS) {
      // has a small space
      if (va & PID_MASK) {	// needs a large space
        // large space not implemented yet
        fatal("Process accessed large space at 0x%08x\n", va);
      }
      mth1 = SmallSpace_GetMth(p->md.pid >> PID_SHIFT);
    } else {
      mth1 = 0;
    }
  } else {		// has a full space
    mth1 = & objC_PhysPageToObHdr(p->md.firstLevelMappingTable)
               ->kt_u.mp.hdrs[0];
    if ((p->md.dacr & 0xc) == 0) {	// has access to domain 1?
      // no, tracking LRU
      mth1->mthAge = age_LiveProc;	// it's been used
      p->md.dacr |= 0x4;	// restore access to domain 1
    }
  }

  /* Set up a SegWalk structure and start building the necessary
   * mapping table, PDE, and PTE entries.
   */
  SegWalk wi;
  wi.needWrite = isWrite;
  wi.traverseCount = 0;
  if (! mth1) {
    // Need to find the first-level page table.
    if (! segwalk_init(&wi, node_GetKeyAtSlot(p->procRoot, ProcAddrSpace),
                       va, p, 0)) {
      goto fault_exit;
    }

    proc_ResetMappingTable(p);
    p->md.pid = PID_IN_PROGRESS;
    /* Note: WalkSeg below may Yield, so the state of p->md has to be
       valid for execution. */
  
    /* Begin the traversal... */
    if ( ! WalkSeg(&wi, walk_root_l2v, p, 0) ) {
      /* We could clear PID_IN_PROGRESS here, but it's not necessary,
      as we already have to handle the possibility of the entry being
      left as PID_IN_PROGRESS if WalkSeg Yields. */
      goto fault_exit;
    }

    DEBUG(pgflt)
      printf("Traversed to top level\n");

    /* It is unlikely but possible that one of the depend entries created
    in walking this portion of the tree was reclaimed by a later entry
    created in walking this portion. Check for that here. */
    if (p->md.pid != PID_IN_PROGRESS)
      act_Yield();	// try again

    uint32_t productNdx = wi.offset >> 32;
    mth1 = FindProduct(&wi, 1, productNdx, va /* not used */);
    if (mth1 == 0) {
      // None found. Need to produce a mapping table. 
      // Large or small space?
      /* Note: We used to produce a small space if va would fit in one. 
      The following example shows why that is wrong.
      Suppose a process has a map with pages at 0x0 and 0x04000000.
      It references 0, gets a small space, and happens to get pid 0x04000000.
      Now it references 0x04000000.
      It will get the aliased page at 0x0, not the page that should be at
      0x04000000. 

      We must produce a large space if the space has anything mapped above
      0x01ffffff. */
      if (wi.memObj->obType <= ot_NtLAST_NODE_TYPE) {
        // It is a GPT, not a single page.
        GPT * gpt = objH_ToNode(wi.memObj);
        uint8_t l2vField = gpt_GetL2vField(gpt);
        unsigned int l2v = l2vField & GPT_L2V_MASK;
        if (l2v > PID_SHIFT - EROS_NODE_LGSIZE) {
          if (l2v > PID_SHIFT) {
            /* Slot 0 could refer to a GPT with addresses above
            (1 << PID_SHIFT). */
 largespace:
            fatal("Process is using large space at 0x%08x\n", va);
          }
          unsigned int startSlot = 1ul << (PID_SHIFT - l2v);
          /* The first startSlot slots span a small space.
          If all the other slots are void, this GPT can produce a small space.
          */
          unsigned int maxSlot;
          if (l2vField & GPT_KEEPER) {        // it has a keeper
            maxSlot = capros_GPT_keeperSlot -1;
          }
          else maxSlot = capros_GPT_nSlots -1;
          if (l2vField & GPT_BACKGROUND) {
            maxSlot = capros_GPT_backgroundSlot -1;
              /* backgroundSlot < keeperSlot, so this must come last. */
          }

          unsigned int i;
          for (i = startSlot; i <= maxSlot; i++) {
            Key * k = node_GetKeyAtSlot(gpt, i);
            if (! keyBits_IsVoidKey(k))
              goto largespace;
          }

          // It can produce a small space.
          /* Must hazard the void keys. If any is changed, the problem
          in the example above could occur. */
          for (i = startSlot; i <= maxSlot; i++) {
            Key * k = node_GetKeyAtSlot(gpt, i);
            keyBits_SetWrHazard(k);
          }
        }
        // else its l2v is too small to produce a large space.
      }
      // else it's a page, therefore can't be a large space.

      mth1 = AllocateSmallSpace();
      if (mth1 == 0) fatal("Could not allocate small space?\n");
      InitMapTabHeader(mth1, &wi, productNdx);
    }

    // Have a large or small space?
    unsigned int ssid = mth1->tableCacheAddr >> PID_SHIFT;
    if (ssid == 0)	// if large space
      assert(false);	// because large space not implemented yet
    p->md.flmtProducer = wi.memObj;
    p->md.firstLevelMappingTable = FLPT_FCSEPA;
    p->md.pid = ssid << PID_SHIFT;

    /* Ensure the small space has a domain assigned. */
    unsigned int domain = EnsureSSDomain(ssid);
    p->md.dacr = (0x1 << (domain * 2)) | 0x1 /* include domain 0 for kernel */;

#ifndef NDEBUG
    if (dbg_inttrap)
      printf("Got small space\n");
#endif
  } else {
    assert(mth1->producerNdx == 0);
    wi.offset = va;
    SegWalk_InitFromMT(&wi, mth1);

    DEBUG(pgflt)
      printf("Found top level, mth1=0x%x\n", mth1);
  }

  const ula_t mva = (va + p->md.pid) & ~EROS_PAGE_MASK;

  /* Small space only for now. */
  uint32_t * theFLPTEntryP = & FLPT_FCSEVA[mva >> L1D_ADDR_SHIFT];
  uint32_t theFLPTEntry = *theFLPTEntryP;

  PTE * pTable;
  MapTabHeader * mth2 = 0;
  if (theFLPTEntry & L1D_VALIDBITS) {	// already valid
    // Can't be a section descriptor, because we never fault on those.
    assert((theFLPTEntry & L1D_VALIDBITS) == (L1D_COARSE_PT & L1D_VALIDBITS));
    kpa_t pa = theFLPTEntry & L1D_COARSE_PT_ADDR;	// phys addr of CPT
    PageHeader * pageH = objC_PhysPageToObHdr(pa & ~ EROS_PAGE_MASK);
    mth2 = & pageH->kt_u.mp.hdrs[(pa & EROS_PAGE_MASK) >> CPT_LGSIZE];
    // FIXME: can avoid the table walk since we already have the CPT
  } else {
    if (theFLPTEntry & ~L1D_VALIDBITS) {
      // The entry is not valid, but it has other bits on.

      /* It's possible for an entry to be left as PTE_IN_PROGRESS.
         If so, ignore it as if it were PTE_ZAPPED. */
      if (theFLPTEntry != PTE_IN_PROGRESS) {
        kpa_t pa = theFLPTEntry & L1D_COARSE_PT_ADDR;	// phys addr of CPT
        PageHeader * pageH = objC_PhysPageToObHdr(pa & ~ EROS_PAGE_MASK);
        mth2 = & pageH->kt_u.mp.hdrs[(pa & EROS_PAGE_MASK) >> CPT_LGSIZE];
        // FIXME: can avoid the table walk since we already have the CPT
        if (theFLPTEntry & L1D_DOMAIN_MASK) {
          // Tracking LRU.
          mth2->mthAge = age_LiveProc;	// it's been used
          theFLPTEntry |= L1D_COARSE_PT;
        } else {
          // This small space has had its domain stolen.
          unsigned int domain = EnsureSSDomain(mva >> PID_SHIFT);
          printf("Reassigning domain to L1D.\n");
          theFLPTEntry |= (domain << L1D_DOMAIN_SHIFT) + L1D_COARSE_PT;
        }
        * theFLPTEntryP = theFLPTEntry;
      }
    } else {
      assert(theFLPTEntry == PTE_ZAPPED);
    }
  }

  unsigned int domain = (theFLPTEntry & L1D_DOMAIN_MASK) >> L1D_DOMAIN_SHIFT;

  if (! mth2) {
    // Need to find the needed CPT.

    // Entry was invalid, but could there be cache entries dependent on it?
    if (theFLPTEntry & L1D_COARSE_PT_ADDR)
      flushCache = true;
    * theFLPTEntryP = PTE_IN_PROGRESS;

    /* Translate bits 31:22 of the address: */
    if ( ! WalkSeg(&wi, walk_top_l2v, theFLPTEntryP, 1) ) {
      /* We could clear PTE_IN_PROGRESS here, but it's not necessary,
      as we already have to handle the possibility of the entry being
      left as PTE_IN_PROGRESS if WalkSeg Yields. */
      goto fault_exit;
    }

#ifndef NDEBUG
    if (dbg_inttrap)
      printf("Traversed to second level\n");
#endif
    DEBUG(pgflt)
      printf("Traversed to second level\n");

    if (* theFLPTEntryP != PTE_IN_PROGRESS)
      // Entry was zapped, try all over again.
      act_Yield();

    // printf("wi.offset=0x%08x ", wi.offset);
    uint32_t productNdx = wi.offset >> L1D_ADDR_SHIFT;
    ula_t cacheAddr = mva >> L1D_ADDR_SHIFT << L1D_ADDR_SHIFT;
    mth2 = FindProduct(&wi, 0, productNdx, cacheAddr);

    if (mth2 == 0) {
      mth2 = AllocateCPT();
      InitMapTabHeader(mth2, &wi, productNdx);
      mth2->tableCacheAddr = cacheAddr;
      void * tableAddr = MapTabHeaderToKVA(mth2);

      DEBUG(pgflt) printf("physAddr=0x%08x\n", VTOP((kva_t)tableAddr));

      // PTE_ZAPPED == 0, so we can just clear the table:
      bzero(tableAddr, CPT_SIZE);
    }
    assert(wi.memObj == mth2->producer);

    pTable = (PTE *) MapTabHeaderToKVA(mth2);

    /* Set the entry in the first-level page table to refer to
       the second-level table. */
    * theFLPTEntryP =
      VTOP((kva_t)pTable) + (domain << L1D_DOMAIN_SHIFT) + L1D_COARSE_PT;
  } else {	// already have the CPT
    wi.offset = (wi.offset & ((1ul << L1D_ADDR_SHIFT) -1))
                + (((uint32_t)mth2->producerNdx) << L1D_ADDR_SHIFT);
    SegWalk_InitFromMT(&wi, mth2);

    pTable = (PTE *) MapTabHeaderToKVA(mth2);

    DEBUG(pgflt)
      printf("Found second level, mth2=0x%x\n", mth2);
  }
  
  /* Now find the page and fill in the PTE. */
  PTE * thePTEP = &pTable[(mva >> EROS_PAGE_ADDR_BITS) & 0xff];
  uint32_t thePTE = thePTEP->w_value;

  bool havePage = false;
  if (thePTE & PTE_VALIDBITS) {
    unsigned int domainAccess = (p->md.dacr >> (domain * 2)) & 0x3;
    if (domainAccess == 0x2) {	// has client access
      unsigned int ap = (thePTE >> 4) & 0x3;
      switch (ap) {
      case 2:	// readonly
        if (! thePTE & PTE_BUFFERABLE) {
          // tracking dirty
          PageHeader * pageH = objC_PhysPageToObHdr(thePTE & PTE_FRAMEBITS);
          assert(pageH && false);	// mark page dirty
          (void)pageH;		// keep the compiler happy
          thePTE |= 0xff0
#ifdef OPTION_WRITEBACK
                          | PTE_BUFFERABLE
#endif
                                           ;
          thePTEP->w_value = thePTE;
        } else {
          if (! isWrite)
            havePage = true;	// have all the access we need
        }
      case 3:	// has RW access
        havePage = true;
        break;

      default:	// 0 or 1 - kernel access only
        break;
      }
    }
    // else something mapped but not for this process.
  } else {	// not valid
    if (thePTE & PTE_CACHEABLE) {
      // tracking LRU
      PageHeader * pageH = objC_PhysPageToObHdr(thePTE & PTE_FRAMEBITS);
      pageH->objAge = age_LiveProc;	// it's been used
      thePTE |= PTE_SMALLPAGE;
      thePTEP->w_value = thePTE;
      havePage = true;
    }
    // else really not valid
  }

  if (! havePage) {
    /* We need to have a value other than PTE_ZAPPED in the PTE
    so we can recognize if a Depend zap occurs. 
    If the PTE was valid (but lacked needed rw access), leave it valid
    while we call WalkSeg, in case we fault or Yield. */
    if (thePTE == PTE_ZAPPED)
      thePTEP->w_value = PTE_IN_PROGRESS;
    
    /* Translate the remaining bits of the address: */
    if ( ! WalkSeg(&wi, EROS_PAGE_LGSIZE, thePTEP, 2) ) {
      /* We could clear PTE_IN_PROGRESS here, but it's not necessary,
      as we already have to handle the possibility of the entry being
      left as PTE_IN_PROGRESS if WalkSeg Yields. */
      goto fault_exit;
    }

    DEBUG(pgflt)
      printf("Traversed to page\n");

    if (thePTEP->w_value == PTE_ZAPPED) {
      // Entry was zapped by Depend, try all over again.

#ifndef NDEBUG
      if (dbg_inttrap)
        printf("Depend zap at page\n");
#endif

      act_Yield();
    }
    
    assert(wi.memObj);
    assert(wi.memObj->obType == ot_PtDataPage ||
           wi.memObj->obType == ot_PtDevicePage);
    PageHeader * const pageH = objH_ToPage(wi.memObj);

    if (isWrite)
      pageH_MakeDirty(pageH);

    pte_Invalidate(thePTEP);	/* remember to purge TLB and cache */

    // Ensure cache coherency. 
    unsigned int cacheClass = pageH->kt_u.ob.cacheAddr & EROS_PAGE_MASK;
    bool canCache = true;	// until proven otherwise

    switch (cacheClass) {
    case CACHEADDR_NONE:	// not previously mapped
      if (isWrite) {
        assert(! mth2->readOnly);
        DEBUG(cache) printf("0x%08x CACHEADDR_NONE to WRITEABLE\n", pageH);
        pageH->kt_u.ob.cacheAddr = mva | CACHEADDR_WRITEABLE;
			// Note, mva has low bits clear
      } else {
        if (! mth2->readOnly) {
          DEBUG(cache) printf("0x%08x CACHEADDR_NONE to ONEREADER\n", pageH);
          pageH->kt_u.ob.cacheAddr = mva | CACHEADDR_ONEREADER;
        } else {	// page table could be mapped at multiple MVAs
          DEBUG(cache) printf("0x%08x CACHEADDR_NONE to READERS\n", pageH);
          pageH->kt_u.ob.cacheAddr = mva | CACHEADDR_READERS;
        }
      }
      break;
    case CACHEADDR_ONEREADER:	// read only at one MVA
      if (! mth2->readOnly	// table is at a single MVA
          && pageH->kt_u.ob.cacheAddr == mva) {
        if (isWrite) {
          DEBUG(cache) printf("0x%08x CACHEADDR_ONEREADER to WRITEABLE\n", pageH);
          pageH->kt_u.ob.cacheAddr |= CACHEADDR_WRITEABLE;
        }
      } else {	// different address
        if (isWrite) {
          DEBUG(cache) printf("0x%08x CACHEADDR_ONEREADER to UNCACHED\n", pageH);
          pageH_MakeUncached(pageH);
          canCache = false;
        } else {
          DEBUG(cache) printf("0x%08x CACHEADDR_ONEREADER to READERS\n", pageH);
          pageH->kt_u.ob.cacheAddr = CACHEADDR_READERS;
        }
      }
      break;
    case CACHEADDR_WRITEABLE:	// writeable at one MVA
      if (! mth2->readOnly	// table is at a single MVA
          && (pageH->kt_u.ob.cacheAddr & ~EROS_PAGE_MASK) == mva) {
        // Nothing to do.
      } else {	// different address
        if (isWrite) {
          DEBUG(cache) printf("0x%08x CACHEADDR_WRITEABLE to UNCACHED\n", pageH);
          pageH_MakeUncached(pageH);
          canCache = false;
        } else {
          /* This page was previously written.
          We could go to the CACHEADDR_UNCACHED state.
          But since the writer may have stopped writing, let's try
          going to CACHEADDR_READERS. */
          /* We must unmap all writeable PTEs. The following unmaps
          read-only PTEs too, which is somewhat unfortunate. */
          DEBUG(cache) printf("0x%08x CACHEADDR_WRITEABLE to READERS\n", pageH);
          keyR_UnmapAll(&pageH_ToObj(pageH)->keyRing);
          pageH->kt_u.ob.cacheAddr = CACHEADDR_READERS;
        }
      }
      break;
    case CACHEADDR_READERS:	// readers at muiltiple MVAs
      if (isWrite) {
        DEBUG(cache) printf("0x%08x CACHEADDR_READERS to UNCACHED\n", pageH);
        pageH_MakeUncached(pageH);
        canCache = false;
      }
      // else nothing to do
      break;
    case CACHEADDR_UNCACHED:	// writer(s) and multiple MVAs
      canCache = false;
      break;
    default: assert(false);
    }

    kpa_t pageAddr = VTOP(pageH_GetPageVAddr(pageH));

#ifndef NDEBUG
    if (dbg_inttrap)
      printf("Traversed to page at 0x%08x\n", pageAddr);
#endif

#if 0
    printf("Setting PTE 0x%08x addr 0x%08x write %c cache %c\n",
           thePTEP, pageAddr,
           (isWrite ? 'y' : 'n'),
           (canCache ? 'y' : 'n') );
#endif
    PTE_Set(thePTEP, pageAddr + PTE_SMALLPAGE
  	/* AP bits, 11 for write, 10 for read */
            + (isWrite ? 0xff0
                         + (canCache ? PTE_CACHEABLE
#ifdef OPTION_WRITEBACK
                                          | PTE_BUFFERABLE
#endif
                            : 0) 
                       : 0xaa0
                         + (canCache ? PTE_CACHEABLE | PTE_BUFFERABLE : 0)
// for read-only access, PTE_BUFFERABLE is not significant.
// See comments in PTEarm.h for why we set it here.
           ) );
  } else {
    DEBUG(pgflt)
      printf("Found PTE\n");
  }

  DEBUG(pgflt)
    printf("Finished page fault\n");

  return true;

fault_exit:
#ifndef NDEBUG
  if (dbg_inttrap)
    dprintf(true, "Pagefault fault, prompt=%d, wi=0x%08x\n", prompt, &wi);
#endif

  if (!prompt) {
    proc_InvokeSegmentKeeper(p, &wi, true, va);
  }
  return false;
}

// May Yield.
void
LoadWordFromUserVirtualSpace(uva_t userAddr, uint32_t * resultP)
{
  // Try a simple load first.
  if (LoadWordFromUserSpace(userAddr, resultP))
    return;

  (void) proc_DoPageFault(act_CurContext(), userAddr,
           false /* not isWrite */ , false);

  // Should work now.
  if (LoadWordFromUserSpace(userAddr, resultP))
    return;

  assert(false);	// DoPageFault didn't Yield and didn't fix it?
}
