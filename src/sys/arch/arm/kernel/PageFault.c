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
#include <kerninc/SegWalk.h>
#include <arch-kerninc/Process.h>
#include <arch-kerninc/PTE.h>
#include <arch-kerninc/IRQ-inline.h>

unsigned int AllocateSmallSpace(ObjectHeader * producer);
unsigned int EnsureSSDomain(unsigned int ssid);

#define dbg_pgflt	0x1	/* steps in taking snapshot */

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
void
pte_ddb_dump(PTE* thisPtr)
{
  extern void db_printf(const char *fmt, ...);

  uint32_t pte = pte_AsWord(thisPtr);
  
  switch (pte & PTE_VALIDBITS) {
  case 1:
  case 3:
    printf("bad 0x%08x\n", pte);
    break;

  case 0:
    if (pte & PTE_CACHEABLE) {
      printf("tracking LRU ");	// and fall into the valid case
    } else if (pte & PTE_BUFFERABLE) {
      printf("in progress\n");
      break;
    } else {
      printf("no access\n");
      break;
    }
  case PTE_SMALLPAGE:
  {
    // All AP fields should match.
    unsigned int ap = (pte >> 10) & 3;
    if (((pte >> 8) & 3) != ap
        || ((pte >> 6) & 3) != ap
        || ((pte >> 4) & 3) != ap ) {
      printf("bad 0x%08x\n", pte);
    } else {
      printf("Pg Frame 0x%08x ap=%d %c%c",
             pte_PageFrame(thisPtr), ap,
             (pte & PTE_CACHEABLE  ? 'C' : ' '),
             (pte & PTE_BUFFERABLE ? 'B' : ' ') );
      switch (ap) {
      case 0:
        printf(" kro\n");
        break;
      case 1:
        printf(" krw\n");
        break;
      case 2:
        if (pte & PTE_BUFFERABLE) {
          printf(" uro\n");
        } else {
          printf(" uro tracking dirty\n");
        }
        break;
      case 3:
        printf(" urw\n");
      }
    }
  }
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

/* Make a new second-level page table. */
// May Yield.
static MapTabHeader *
MakeNewPageTable(SegWalk * wi /*@ not null @*/, uint32_t ndx)
{
  DEBUG(pgflt) printf("MakeNewPageTable ");

  MapTabHeader * mth = AllocateCPT();

  mth->producerBlss = wi->segBlss;
  mth->producerNdx = ndx;
  mth->redSeg = wi->redSeg;
  mth->wrapperProducer = wi->segObjIsWrapper;
  mth->redSpanBlss = wi->redSpanBlss;
  mth->rwProduct = 1;
  mth->caProduct = 1;
  void * tableAddr = MapTabHeaderToKVA(mth);

  DEBUG(pgflt) printf("physAddr=0x%08x\n", VTOP((kva_t)tableAddr));

  // PTE_ZAPPED == 0, so we can just clear the table:
  bzero(tableAddr, CPT_SIZE);

  objH_AddProduct(wi->segObj, mth);

  return mth;
}

/* Walk the node's products looking for an acceptable product: */
static MapTabHeader *
objH_FindProduct(ObjectHeader * thisPtr, SegWalk * wi /*@not null@*/ ,
                 unsigned int tblSize, 
                 unsigned int producerNdx, 
                 bool rw, bool ca, ula_t cacheAddr)
{
  uint32_t blss = wi->segBlss;

#if 0
  printf("Search for product blss=%d ndx=%d, rw=%c producerTy=%d\n",
	       blss, ndx, rw ? 'y' : 'n', obType);
#endif
  
/* #define FINDPRODUCT_VERBOSE */

  MapTabHeader * product;
  
  for (product = thisPtr->prep_u.products;
       product; product = product->next) {
    if ((uint32_t) product->producerBlss != blss) {
#ifdef FINDPRODUCT_VERBOSE
      printf("Producer BLSS not match\n");
#endif
      continue;
    }
    if (product->redSeg != wi->redSeg) {
#ifdef FINDPRODUCT_VERBOSE
      printf("Red seg not match\n");
#endif
      continue;
    }
    if (product->redSeg) {
      if (product->wrapperProducer != wi->segObjIsWrapper) {
#ifdef FINDPRODUCT_VERBOSE
	printf("redProducer not match\n"); 
#endif
	continue;
      }
      if (product->redSpanBlss != wi->redSpanBlss) {
#ifdef FINDPRODUCT_VERBOSE
	printf("redSpanBlss not match: prod %d wi %d\n",
		       product->redSpanBlss, wi->redSpanBlss);
#endif
	continue;
      }
    }
    if ((uint32_t) product->tableSize != tblSize) {
#ifdef FINDPRODUCT_VERBOSE
      printf("tableSize not match\n");
#endif
      continue;
    }
    if ((uint32_t) product->producerNdx != producerNdx) {
#ifdef FINDPRODUCT_VERBOSE
      printf("producerNdx not match\n");
#endif
      continue;
    }
    if ((uint32_t) product->tableCacheAddr != cacheAddr) {
#ifdef FINDPRODUCT_VERBOSE
      printf("cacheAddr not match\n");
#endif
      continue;
    }
    if (product->rwProduct != (rw ? 1 : 0)) {
#ifdef FINDPRODUCT_VERBOSE
      printf("rwProduct not match\n");
#endif
      continue;
    }
    if (product->caProduct != (ca ? 1 : 0)) {
#ifdef FINDPRODUCT_VERBOSE
      printf("caProduct not match\n");
#endif
      continue;
    }

    /* WE WIN! */
    break;
  }

  if (product) {
    assert(product->producer == thisPtr);
  }

#if 0
  if (wi.segBlss != wi.pSegKey->GetBlss())
    dprintf(true, "Found product 0x%x segBlss %d prodKey 0x%x keyBlss %d\n",
		    product, wi.segBlss, wi.pSegKey, wi.pSegKey->GetBlss());
#endif

#ifdef FINDPRODUCT_VERBOSE
  printf("0x%08x->FindProduct(blss=%d,ndx=%d,rw=%c,ca=%c,"
		 "producerTy=%d) => 0x%08x\n",
		 thisPtr,
		 blss, producerNdx, rw ? 'y' : 'n', ca ? 'y' : 'n',
                 thisPtr->obType,
		 product);
#endif

  return product;
}

/* Handle page fault from user or system mode.
   This is called from the Abort exception handlers.
   This procedure does not return - it calls proc_Resume().  */
void
PageFault(bool prefetch,	/* else data abort */
          uint32_t fsr,	/* fault status */
          ula_t fa)	/* fault address, if data abort */
{
  Process * proc = act_CurContext();
  bool writeAccess = false;
  uva_t va;	/* the unmodified virtual address */

  assert(irq_DisableDepth == 0);
  irq_DisableDepth = 1;	/* disabled by the exception */

  if (prefetch) {
    writeAccess = false;
    va = proc->trapFrame.r15;	/* his pc */
    DEBUG(pgflt)
      printf("Prefetch PageFault fa=0x%08x pc=0x%08x",
             va, proc->trapFrame.r15);
  } else {
    /* fa has the modified virtual address */
    ula_t pidAddr = proc->md.pid;
    if ((fa & PID_MASK) == pidAddr) {
      /* Fault address could have been generated by adding the PID;
         assume it was. */
      va = fa - pidAddr;
    } else {
      va = fa;
    }
    DEBUG(pgflt)
      printf("Data PageFault fa=0x%08x fsr=0x%08x pc=0x%08x r12=0x%08x sp=0x%08x r14=0x%08x\n",
             va, fsr, proc->trapFrame.r15, proc->trapFrame.r12,
             proc->trapFrame.r13, proc->trapFrame.r14);

    /* What is the problem? */
    switch (fsr & 0xd) {
    /* the following are all the cases that the ARM920T generates. */
    case 0x1:	/* alignment fault */
      fatal("Alignment fault at 0x%08x, unimplemented\n", fa);
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
  }
  /* va and writeAccess are now set. */

  proc->stats.pfCount++;

  KernStats.nPfTraps++;
  if (writeAccess) KernStats.nPfAccess++;

  objH_BeginTransaction();

  (void) proc_DoPageFault(proc, va, writeAccess, false);

  /* We succeeded (wonder of wonders) -- release pinned resources. */
  objH_ReleasePinnedObjects();

  /* No need to release uncommitted I/O page frames -- there should
   * not be any.
   */

  act_Reschedule();
  proc_Resume();
}

#define DATA_PAGE_FLAGS  (PTE_ACC|PTE_USER|PTE_V)

uint32_t DoPageFault_CallCounter;

INLINE uint64_t
BLSS_MASK64(uint32_t blss, uint32_t frameBits)
{
  uint32_t bits_to_shift =
    (blss - EROS_PAGE_BLSS) * EROS_NODE_LGSIZE + frameBits; 

  uint64_t mask = (1ull << bits_to_shift);
  mask -= 1ull;
  
  return mask;
}

#define WALK_LOUD
#define FAST_TRAVERSAL
/* Returns ... */
// May Yield.
bool
proc_DoPageFault(Process * p, uva_t va, bool isWrite, bool prompt)
{
  const int walk_root_blss = 4 + EROS_PAGE_BLSS;
  const int walk_top_blss = 2 + EROS_PAGE_BLSS;
  SegWalk wi;
  PTE *pTable;
  
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
    dprintf(true, "Process accessed kernel space, pc=0x%08x\n",
            p->trapFrame.r15);
    proc_SetFault(p, FC_InvalidAddr, va, false);
    return false;
  }
  /* Small space only for now. */
  if (va >= 0x02000000) {
    dprintf(true, "Process accessed large space at 0x%08x\n", va);
    proc_SetFault(p, FC_InvalidAddr, va, false);
    return false;
  }

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

	/* for now, just get it working */
  /* Begin the traversal... */
  if ( !proc_WalkSeg(p, &wi, walk_root_blss, p, 0) ) {
    proc_SetFault(p, wi.faultCode, va, false);

    return false;
  }
  
  /* Is it a small or full space? */
  /* (For now, small space only.) */
  /* Does the object produce a small space? */
  unsigned int ssid;
  if (wi.segObj->ssid == 0) {
    /* This object needs to produce a small space. */
    ssid = AllocateSmallSpace(wi.segObj);
    if (ssid == 0) fatal("Could not allocate small space?\n");
    wi.segObj->ssid = ssid;
  } else {
    ssid = wi.segObj->ssid;
  }
  p->md.flmtProducer = wi.segObj;
  p->md.firstLevelMappingTable = FLPT_FCSEPA;
  p->md.pid = ssid << PID_SHIFT;

  /* Ensure the small space has a domain assigned. */
  unsigned int domain = EnsureSSDomain(ssid);
  p->md.dacr = (0x1 << (domain * 2)) | 0x1 /* include domain 0 for kernel */;

  const ula_t mva = va + p->md.pid;

  /* Small space only for now. */
  uint32_t * theFLPTEntry = & FLPT_FCSEVA[mva >> L1D_ADDR_SHIFT];

  if (*theFLPTEntry & L1D_VALIDBITS) {	// already valid
    assert((*theFLPTEntry & L1D_VALIDBITS) == (L1D_COARSE_PT & L1D_VALIDBITS));
  } else {
    if (*theFLPTEntry & ~L1D_VALIDBITS) {
      printf("L1D temporarily invalid - need to optimize.\n");
    }
  }

  /* Translate bits 31:22 of the address: */
  if ( !proc_WalkSeg(p, &wi, walk_top_blss, theFLPTEntry, 1) )
    return false;

  // printf("wi.offset=0x%08x ", wi.offset);
  uint32_t productNdx = wi.offset >> L1D_ADDR_SHIFT;

  MapTabHeader * mth =
    objH_FindProduct(wi.segObj, &wi, 0, productNdx, true, true, 
                     mva >> L1D_ADDR_SHIFT);

  if (mth == 0) {
    mth = MakeNewPageTable(&wi, productNdx);
    mth->tableCacheAddr = mva >> L1D_ADDR_SHIFT;
  }
  assert(wi.segBlss == mth->producerBlss);
  assert(wi.segObj == mth->producer);
  assert(wi.redSeg == mth->redSeg);

  /* On x86, the page table is always RW product, and we rely on
     the write permission bit at the PDE level: ??? */
  assert(mth->rwProduct);

  pTable = (PTE *) MapTabHeaderToKVA(mth);

  /* Set the entry in the first-level page table to refer to
     the second-level table. */
  * theFLPTEntry =
    VTOP((kva_t)pTable) + (domain << L1D_DOMAIN_SHIFT) + L1D_COARSE_PT;
  
  /* Now find the page and fill in the PTE. */
  uint32_t pteNdx = (mva >> 12) & 0xff;
  PTE * thePTE = &pTable[pteNdx];
    
    /* Translate the remaining bits of the address: */
    if ( !proc_WalkSeg(p, &wi, EROS_PAGE_BLSS, thePTE, 2) )
      return false;
    
    assert(wi.segObj);
    assert(wi.segObj->obType == ot_PtDataPage ||
	   wi.segObj->obType == ot_PtDevicePage);

    if (isWrite)
      pageH_MakeDirty(objH_ToPage(wi.segObj));

    kpa_t pageAddr = VTOP(pageH_GetPageVAddr(objH_ToPage(wi.segObj)));

  pte_Invalidate(thePTE);	/* if it was valid, remember to purge TLB */
#if 0
  printf("Setting PTE 0x%08x addr 0x%08x write %c cache %c\n",
         thePTE, pageAddr,
         (isWrite ? 'y' : 'n'),
         (wi.canCache ? 'y' : 'n') );
#endif
  PTE_Set(thePTE, pageAddr + PTE_SMALLPAGE
	/* AP bits, 11 for write, 10 for read */
          + (isWrite ? 0xff0
                       + (wi.canCache ? PTE_CACHEABLE : 0) 
// we never use write-back mode
                     : 0xaa0
                       + (wi.canCache ? PTE_CACHEABLE | PTE_BUFFERABLE : 0)
// for read-only access, PTE_BUFFERABLE is not significant.
// See comments in PTEarm.h for why we set it here.
         ) );

  return true;
}
