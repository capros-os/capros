/*
 * Copyright (C) 1998, 1999, 2001, Jonathan S. Shapiro.
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

INLINE bool
obj_IsFLPT(ObjectHeader * ob) /* ob->obType must be ot_PtMappingPage */
{
  /* tableSize is 0 for page table, 1 for first level page table */
  return ob->kt_u.mp.tableSize /* == 1 */ ;
}

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

#ifndef NDEBUG
/* pObj must be a page type. */
bool
pte_ObIsNotWritable(ObjectHeader *pObj)
{
  uint32_t pf;
  uint32_t ent;
  
  /* Start by building a writable PTE for the page: */
  uint32_t pagePA = VTOP(objC_ObHdrToPage(pObj));

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
    ObjectHeader *pHdr = objC_GetCorePageFrame(pf);

    if (pHdr->obType != ot_PtMappingPage)
      continue;

    uint32_t * ptepg = (uint32_t *) objC_ObHdrToPage(pHdr);
    
    if (obj_IsFLPT(pHdr)) {
      /* Scan the entries in this FLPT. */
      for (ent = 0; ent < (UserEndVA >> L1D_ADDR_SHIFT); ent++) {
        if ((ptepg[ent] & L1D_VALIDBITS) == (L1D_COARSE_PT & L1D_VALIDBITS)) {
          if ((ptepg[ent] & L1D_COARSE_PT_ADDR) == pagePA) {
            dprintf(true, "ObIsNotWriteable failed\n");
            return false;
          }
        }
      }
    } else {
      /* Scan the entries in this page table. */
      for (ent = 0; ent < 256; ent++) {
        if ((ptepg[ent] & CPT_VALIDBITS) == CPT_SMALL_PAGE) {
          if ((ptepg[ent] & CPT_PAGE_MASK) == pagePA	/* this page */
              && (ptepg[ent] & 0xff0) == 0xff0 ) {	/* and writeable */
            dprintf(true, "ObIsNotWriteable failed\n");
            return false;
          }
        }
      }
    }
  }

  return true;
}
#endif /* !NDEBUG */

/* Make a new second-level page table.
Note: as a temporary expedient, we allocate a page and waste 75% of it. */
static ObjectHeader *
MakeNewPageTable(SegWalk * wi /*@ not null @*/, uint32_t ndx)
{
  DEBUG(pgflt) printf("MakeNewPageTable ");
  ObjectHeader *pTable = objC_GrabPageFrame();
  kva_t tableAddr;
  assert (keyR_IsValid(&pTable->keyRing, pTable));
  pTable->obType = ot_PtMappingPage;
  pTable->kt_u.mp.tableSize = 0;
  pTable->kt_u.mp.producerBlss = wi->segBlss;
  pTable->kt_u.mp.producerNdx = ndx;
  
  pTable->kt_u.mp.redSeg = wi->redSeg;
  pTable->kt_u.mp.wrapperProducer = wi->segObjIsWrapper;
  pTable->kt_u.mp.redSpanBlss = wi->redSpanBlss;
  pTable->kt_u.mp.rwProduct = 1;
  pTable->kt_u.mp.caProduct = 1;
  objH_SetDirtyFlag(pTable);

  tableAddr = objC_ObHdrToPage(pTable);
  DEBUG(pgflt) printf("physAddr=0x%08x\n", VTOP(tableAddr));

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

/* Walk the node's products looking for an acceptable product: */
ObjectHeader *
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

  ObjectHeader * product;
  
  for (product = thisPtr->prep_u.products; product; product = product->next) {
    assert(product->obType == ot_PtMappingPage);
    if ((uint32_t) product->kt_u.mp.producerBlss != blss) {
#ifdef FINDPRODUCT_VERBOSE
      printf("Producer BLSS not match\n");
#endif
      continue;
    }
    if (product->kt_u.mp.redSeg != wi->redSeg) {
#ifdef FINDPRODUCT_VERBOSE
      printf("Red seg not match\n");
#endif
      continue;
    }
    if (product->kt_u.mp.redSeg) {
      if (product->kt_u.mp.wrapperProducer != wi->segObjIsWrapper) {
#ifdef FINDPRODUCT_VERBOSE
	printf("redProducer not match\n"); 
#endif
	continue;
      }
      if (product->kt_u.mp.redSpanBlss != wi->redSpanBlss) {
#ifdef FINDPRODUCT_VERBOSE
	printf("redSpanBlss not match: prod %d wi %d\n",
		       product->kt_u.mp.redSpanBlss, wi->redSpanBlss);
#endif
	continue;
      }
    }
    if ((uint32_t) product->kt_u.mp.tableSize != tblSize) {
#ifdef FINDPRODUCT_VERBOSE
      printf("tableSize not match\n");
#endif
      continue;
    }
    if ((uint32_t) product->kt_u.mp.producerNdx != producerNdx) {
#ifdef FINDPRODUCT_VERBOSE
      printf("producerNdx not match\n");
#endif
      continue;
    }
    if ((uint32_t) product->kt_u.mp.tableCacheAddr != cacheAddr) {
#ifdef FINDPRODUCT_VERBOSE
      printf("cacheAddr not match\n");
#endif
      continue;
    }
    if (product->kt_u.mp.rwProduct != (rw ? 1 : 0)) {
#ifdef FINDPRODUCT_VERBOSE
      printf("rwProduct not match\n");
#endif
      continue;
    }
    if (product->kt_u.mp.caProduct != (ca ? 1 : 0)) {
#ifdef FINDPRODUCT_VERBOSE
      printf("caProduct not match\n");
#endif
      continue;
    }

    /* WE WIN! */
    break;
  }

  if (product) {
    assert(product->kt_u.mp.producer == thisPtr);
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
   This procedure does not return - it should call proc_Resume().  */
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

  PteZapped = false;

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
    dprintf(true, "Process accessed large space\n");
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
  if ( !proc_WalkSeg(p, &wi, walk_root_blss,
  /* bug: following isn't really a PTE */
		     (PTE *)&p->md.firstLevelMappingTable, 0, false) ) { 
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

  ula_t la = va + p->md.pid;

  /* Small space only for now. */
  uint32_t * theFLPTEntry = & FLPT_FCSEVA[la >> L1D_ADDR_SHIFT];

  /* Translate the top 10 bits of the address: */
  if ( !proc_WalkSeg(p, &wi, walk_top_blss,
                     (PTE *)theFLPTEntry, 0, true) )
    return false;

////printf("wi.offset=0x%08x ", wi.offset);////
  uint32_t productNdx = wi.offset >> 20;

  /* Level 0 product need never be a read-only product.  We use
   * the write permission bit at the PDE level.
     ???
   */
  ObjectHeader * pTableHdr =
    objH_FindProduct(wi.segObj, &wi, 0, productNdx, true, true, 
                     la >> L1D_ADDR_SHIFT);

  if (pTableHdr == 0) {
    pTableHdr = MakeNewPageTable(&wi, productNdx);
    pTableHdr->kt_u.mp.tableCacheAddr = la >> L1D_ADDR_SHIFT;
  }
  assert(pTableHdr->obType == ot_PtMappingPage);
  assert(wi.segBlss == pTableHdr->kt_u.mp.producerBlss);
  assert(wi.segObj == pTableHdr->kt_u.mp.producer);
  assert(wi.redSeg == pTableHdr->kt_u.mp.redSeg);

  /* On x86, the page table is always RW product, and we rely on
     the write permission bit at the PDE level: ??? */
  assert(pTableHdr->kt_u.mp.rwProduct);

  pTable = (PTE *) objC_ObHdrToPage(pTableHdr);

  /* Set the entry in the first-level page table to refer to
     the second-level table. */
  * theFLPTEntry =
    VTOP((kva_t)pTable) + (domain << L1D_DOMAIN_SHIFT) + L1D_COARSE_PT;
  
  /* Now find the page and fill in the PTE. */
  uint32_t pteNdx = (va >> 12) & 0xff;
  PTE * thePTE = &pTable[pteNdx];
    
    /* Translate the remaining bits of the address: */
    if ( !proc_WalkSeg(p, &wi, EROS_PAGE_BLSS, thePTE, 0, true) )
      return false;
    
    assert(wi.segObj);
    assert(wi.segObj->obType == ot_PtDataPage ||
	   wi.segObj->obType == ot_PtDevicePage);

    if (isWrite)
      objH_MakeObjectDirty(wi.segObj);

    kpa_t pageAddr = VTOP(objC_ObHdrToPage(wi.segObj));

    if (pageAddr == 0)
      dprintf(true, "wi.segObj 0x%08x at addr 0x%08x!! (wi=0x%08x)\n",
		      wi.segObj, pageAddr, &wi);
  pte_Invalidate(thePTE);	/* if it was valid, remember to purge TLB */
#if 0
  printf("Setting PTE 0x%08x addr 0x%08x write %c cache %c\n",
         thePTE, pageAddr,
         (isWrite ? 'y' : 'n'),
         (wi.canCache ? 'y' : 'n') );
#endif
  PTE_Set(thePTE, pageAddr + PTE_SMALLPAGE
          + (isWrite ? 0xff0 : 0xaa0)	/* AP bits, 11 for write, 10 for read */
          + (wi.canCache ? PTE_CACHEABLE | PTE_BUFFERED : 0) );

  UpdateTLB();

  return true;
}
