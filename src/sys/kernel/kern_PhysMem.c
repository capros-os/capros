/*
 * Copyright (C) 1998, 1999, Jonathan S. Shapiro.
 * Copyright (C) 2005, 2006, 2007, 2008, Strawberry Development Group
 *
 * This file is part of the CapROS Operating System,
 * and is derived from the EROS Operating System.
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

#include <kerninc/kernel.h>
#include <kerninc/util.h>
#include <kerninc/PhysMem.h>
#include <eros/ffs.h>
#include <eros/fls.h>
#include <idl/capros/DevPrivs.h>

#define dbg_pgalloc	0x1
#define dbg_avail	0x2
#define dbg_alloc	0x4
#define dbg_new		0x8

/* Following should be an OR of some of the above */
#define dbg_flags   ( 0u )

#define DEBUG(x) if (dbg_##x & dbg_flags)

/* Implementation of Memory Constraints */
/* physMem_pages: must be page aligned */
PmemConstraint physMem_pages = { (kpa_t) 0u, ~((kpa_t)0u), EROS_PAGE_SIZE };
/* physMem_any: must be word aligned */
PmemConstraint physMem_any = { (kpa_t) 0u, ~((kpa_t)0u), sizeof(uint32_t) };


static PmemInfo PhysicalMemoryInfo[MAX_PMEMINFO];
PmemInfo *physMem_pmemInfo = &PhysicalMemoryInfo[0];
unsigned long physMem_nPmemInfo;

kpg_t physMem_numFreePageFrames = 0;
kpg_t physMem_numMapTabPageFrames = 0;

#define logMaxFreePageBlock capros_DevPrivs_logMaxDMABlockSize
// largest block size we can allocate

// freePages[n] has free blocks of size 2**n pages.
Link freePages[logMaxFreePageBlock + 1];

// pmi_ToPhysPgNum is for use when we already have pmi handy.
static kpg_t
pmi_ToPhysPgNum(const PageHeader * pageH, const PmemInfo * pmi)
{
  return pageH - pmi->firstObHdr + pmi->firstObPgAddr;
}

kpg_t
pageH_ToPhysPgNum(const PageHeader * pageH)
{
  PmemInfo * pmi = pageH->physMemRegion;
  return pmi_ToPhysPgNum(pageH, pmi);
}

kpa_t
pageH_GetPhysAddr(const PageHeader * pageH)
{
  return ((kpa_t) pageH_ToPhysPgNum(pageH)) << EROS_PAGE_LGSIZE;
}

static PageHeader *
physMem_PhysPgNumToPageH(PmemInfo * pmi, kpg_t pgNum)
{
  PageHeader * pageH = &pmi->firstObHdr[pgNum - pmi->firstObPgAddr];

  assert(pageH->physMemRegion == pmi);

  return pageH;
}

PageHeader *
objC_PhysPageToObHdr(kpa_t pagepa)
{
  unsigned rgn;
  kpg_t pgNum = pagepa >> EROS_PAGE_LGSIZE;
    
  for (rgn = 0; rgn < physMem_nPmemInfo; rgn++) {
    PmemInfo * pmi= &physMem_pmemInfo[rgn];
    if (pmi->type != MI_MEMORY && pmi->type != MI_DEVICEMEM
        && pmi->type != MI_BOOTROM)
      continue;

    kpg_t endPgNum = pmi->firstObPgAddr + pmi->nPages;

    if (pgNum >= pmi->firstObPgAddr && pgNum < endPgNum) {
      return physMem_PhysPgNumToPageH(pmi, pgNum);
    }
  }

  return NULL;
}

/*
Free a block, after it has been determined that it is not being
merged with a buddy.

pageH is the first page in the block.
2**j is the size of the block in pages.
*/
static void
pmi_FreeOneBlock(PageHeader * pageH, unsigned int j)
{
  pageH->kt_u.free.obType = ot_PtFreeFrame;
  pageH->kt_u.free.log2Pages = j;
  link_insertAfter(&freePages[j], &pageH->kt_u.free.freeLink);
}

/* Find the free block that contains pageH and split it into two free blocks. */
void
physMem_SplitContainingFreeBlock(PageHeader * pageH)
{
  kpg_t pgNum;
  PmemInfo * pmi = pageH->physMemRegion;

  // Find the containing block.
  while (pageH_GetObType(pageH) != ot_PtFreeFrame) {
    assert(pageH_GetObType(pageH) == ot_PtSecondary);
    pgNum = pmi_ToPhysPgNum(pageH, pmi);
    // Remove the lowest-order 1 bit:
    pgNum &= pgNum - 1;
    pageH = physMem_PhysPgNumToPageH(pmi, pgNum);
  }

  pgNum = pmi_ToPhysPgNum(pageH, pmi);
  unsigned int j = pageH->kt_u.free.log2Pages;
  // Remove from free list.
  link_Unlink(&pageH->kt_u.free.freeLink);
  // Free each half.
  j--;
  pmi_FreeOneBlock(physMem_PhysPgNumToPageH(pmi, pgNum), j);
  pmi_FreeOneBlock(physMem_PhysPgNumToPageH(pmi, pgNum + (1U << j)), j);
}

/* Free a block, merging with buddies as needed. */
void
physMem_FreeBlock(PageHeader * pageH, unsigned int numPages)
{
  DEBUG(pgalloc) printf("FreeBlock pageH=%#x num=%#x\n", pageH, numPages);

  physMem_numFreePageFrames += numPages;

  PmemInfo * pmi = pageH->physMemRegion;
  kpg_t pgNum = pmi_ToPhysPgNum(pageH, pmi);

  // pgNum is a multiple of 2**n, where 2**n >= numPages
  assert((pgNum & -pgNum) == 0 || (pgNum & -pgNum) >= numPages);
  do {
    // Free the smallest block at the end.
    unsigned int j = ffs32(numPages);
    if (j > logMaxFreePageBlock) {
      j = logMaxFreePageBlock;
    }
    unsigned int jSize = 1U << j;
    kpg_t jPgNum = pgNum + numPages - jSize;	/* beginning of
					small block at the end */
    PageHeader * jPageH = physMem_PhysPgNumToPageH(pmi, jPgNum);
    assert(! pageH_IsFree(jPageH));

    // Buddy system liberation
recheck:
    if (j < logMaxFreePageBlock) {
      kpg_t bPgNum = jPgNum ^ jSize;	// potential buddy
      // Is the potential buddy in the same region?
      // We never merge between regions.
      /* Note: we do not simply use pmi->firstObPgAddr as the origin
      for all pgNum's (which would eliminate the first comparison below),
      because we want blocks to be aligned on a physical address
      that is a multiple of their size. */
      if (bPgNum >= pmi->firstObPgAddr
          && bPgNum < (pmi->firstObPgAddr + pmi->nPages)) {
        PageHeader * bPageH = physMem_PhysPgNumToPageH(pmi, bPgNum);
        if (pageH_IsFree(bPageH) && bPageH->kt_u.free.log2Pages == j) {
          // Merge with buddy.
          link_Unlink(&bPageH->kt_u.free.freeLink);	// remove from free list
          j++;
          if (bPgNum < jPgNum) {
            pageH_ToObj(jPageH)->obType = ot_PtSecondary;
            jPgNum = bPgNum;
            jPageH = bPageH;
          } else {
            pageH_ToObj(bPageH)->obType = ot_PtSecondary;
          }
          goto recheck;
        }
      }
    }
    // No buddy to merge with.
    pmi_FreeOneBlock(jPageH, j);

    numPages -= jSize;
  } while (numPages);
}

/* Allocate a block.
Returns NULL if none available. */
PageHeader *
physMem_AllocateBlock(unsigned int numPages)
{
  unsigned int k = fls32(numPages - 1);
  // 2**(k-1) < numPages <= 2**(k)
  assert(k <= logMaxFreePageBlock);	// else block is too big

  // Look for a free block.
  unsigned int j;
  for (j = k; j <= logMaxFreePageBlock; j++)
    if (! link_isSingleton(&freePages[j]))
      goto foundj;
  
  return NULL;

foundj: ;
  // Take the first block from the list.
  PageHeader * pageH = container_of(freePages[j].next,
                                     PageHeader, kt_u.free.freeLink);
  assert(pageH_IsFree(pageH));
  link_Unlink(&pageH->kt_u.free.freeLink);
  pageH_ToObj(pageH)->obType = ot_PtNewAlloc;	// not free

  PmemInfo * pmi = pageH->physMemRegion;
  kpg_t jPgNum = pmi_ToPhysPgNum(pageH, pmi);

  physMem_numFreePageFrames -= numPages;

  kpg_t xPgNum = jPgNum;
  kpg_t jSize = ((kpg_t)1) << j;

  DEBUG(pgalloc) printf("Alloc %#x from %#x pageH %#x\n",
                        numPages, jSize, pageH);

  while (jSize != numPages) {
    j--;
    jSize = ((kpg_t)1) << j;
    if (jSize >= numPages) {
      // Liberate upper half, repeat on lower half
      pmi_FreeOneBlock(physMem_PhysPgNumToPageH(pmi, xPgNum + jSize), j);
    } else {
      // Repeat on upper half of the block.
      numPages -= jSize;
      xPgNum += jSize;
      assert(pageH_GetObType(physMem_PhysPgNumToPageH(pmi, xPgNum))
             == ot_PtSecondary);
    }
  }

  return pageH;
}

void
physMem_Init(void)
{
  physMem_Init_MD();

  int i;
  for (i = 0; i <= logMaxFreePageBlock; i++) {
    link_Init(&freePages[i]);
  }
}

void
physMem_FreeAll(PmemInfo * pmi)
{
  kpg_t pgNum = pmi->firstObPgAddr;
  kpg_t nPages = pmi->nPages;

  while (nPages > 0) {
    PageHeader * pageH = physMem_PhysPgNumToPageH(pmi, pgNum);
    unsigned int j = ffs32(pgNum);
    DEBUG(pgalloc) printf("FreeAll pgnum %#x j %d\n", pgNum, j);
    kpg_t pgs = ((kpg_t)1) << j;
    if (pgs > nPages) {		// the last one can have any number of pages
      physMem_FreeBlock(pageH, nPages);
      break;
    } else {
      physMem_FreeBlock(pageH, pgs); // we know it will have no buddy
    }
    
    pgNum += pgs;
    nPages -= pgs;
  }
}

PmemInfo *
physMem_AddRegion(kpa_t base, kpa_t bound, uint32_t type, bool readOnly)
{
  PmemInfo *kmi = &physMem_pmemInfo[physMem_nPmemInfo];
  unsigned int i = 0;

  if (type == MI_DEVICEMEM) {
    /* Do not do this check for the bootup cases, as some of those
     * actually do overlap. */
    for (i = 0; i < physMem_nPmemInfo; i++) {
      if (base >= physMem_pmemInfo[i].base && base < physMem_pmemInfo[i].bound)
	return 0;

      if (bound > physMem_pmemInfo[i].base && bound <= physMem_pmemInfo[i].bound)
	return 0;
    }
  }

  assert (physMem_nPmemInfo < MAX_PMEMINFO);

  kmi->base = base;
  kmi->bound = bound;
  kmi->type = type;
  kmi->nPages = 0;
  kmi->firstObPgAddr = 0;
  kmi->firstObHdr = 0;
  kmi->readOnly = readOnly;

  if (type == MI_MEMORY) {
    kmi->allocBase = base;
    kmi->allocBound = bound;
  }
  else {
    kmi->allocBase = 0;
    kmi->allocBound = 0;
  }

  physMem_nPmemInfo++;

  return kmi;
}

kpsize_t
PmemInfo_ContiguousPages(const PmemInfo * pmi)
{
  uint32_t base = pmi->allocBase;
  uint32_t bound = pmi->allocBound;
  base = align_up(base, EROS_PAGE_SIZE);

  if (base >= bound)
    return 0;

  return (bound - base) / EROS_PAGE_SIZE;
}

kpsize_t
physMem_MemAvailable(PmemConstraint *mc, unsigned unitSize)
{
  unsigned nUnits = 0;
  unsigned rgn = 0;

  for (rgn = 0; rgn < physMem_nPmemInfo; rgn++) {
    PmemInfo *kmi = &physMem_pmemInfo[rgn];
    uint32_t base = kmi->allocBase;
    uint32_t bound = kmi->allocBound;

    if (kmi->type != MI_MEMORY)
      continue;

    base = max(base, mc->base);
    bound = min(bound, mc->bound);

    /* The region (partially) overlaps the requested region */
    base = align_up(base, mc->align);

    if (base >= bound)
      continue;

    nUnits += (bound - base) / unitSize;
  }

  DEBUG(avail) {
    printf("%d units of %d %% %d bytes available\n",
		   nUnits, unitSize, mc->align );
  }

  return nUnits;
}

/* Preferentially allocate out of higher memory, because legacy DMA
 * controllers tend to have restricted addressable bounds in physical
 * memory. */
static PmemInfo *
preferred_region(PmemInfo *rgn1, PmemInfo *rgn2)
{
  if (rgn1 == 0)
    return rgn2;

  if (rgn2 && rgn1->base < rgn2->base)
    return rgn2;

  return rgn1;
}

PmemInfo *
physMem_ChooseRegion(kpsize_t nBytes, PmemConstraint *mc)
{
  PmemInfo *allocTarget = 0;
  unsigned rgn = 0;

  for (rgn = 0; rgn < physMem_nPmemInfo; rgn++) {
    PmemInfo *kmi = &physMem_pmemInfo[rgn];
    kpa_t base = kmi->allocBase;
    kpa_t bound = kmi->allocBound;
    kpa_t where;

    if (kmi->type != MI_MEMORY)
      continue;

    base = max(base, mc->base);
    bound = min(bound, mc->bound);

    if (base >= bound)
      continue;

    /* The region (partially) overlaps the requested region. See if it
     * has enough suitably aligned space: */
    
    where = bound - nBytes;
    where = align_down(where, mc->align);

    if (where >= base)
      allocTarget = preferred_region(allocTarget, kmi);
  }

  return allocTarget;
}

/* Allocate nBytes from available physical memory with constraint mc.
   nBytes must be a multiple of mc->align. */
kpa_t	// returns -1 if can't allocate
physMem_Alloc(kpsize_t nBytes, PmemConstraint *mc)
{
  PmemInfo *allocTarget;

  assert(((unsigned)nBytes & (mc->align - 1)) == 0);  

  DEBUG(alloc)
     printf("PhysMem::Alloc: nBytes=0x%x, "
                    "mc->base=0x%x, mc->bound=0x%x, mc->align=0x%x\n",
                    (unsigned)nBytes, (unsigned)mc->base,
                    (unsigned)mc->bound, mc->align);

  allocTarget = physMem_ChooseRegion(nBytes, mc);

  if (allocTarget) {
    /* We are willing to waste space at either the beginning or the end
       if necessary for alignment. */
    kpa_t allocTargetBaseAligned = align_up(allocTarget->allocBase, mc->align);
    kpa_t allocTargetBoundAligned = align_down(allocTarget->allocBound,
                                               mc->align);

    /* Apply the constraint. */
    kpa_t base  = max(allocTargetBaseAligned, mc->base);
    kpa_t bound = min(allocTargetBoundAligned, mc->bound);

    /* We will only grab from the end or from the beginning -- not
     * from the middle. In fact, we will only grab from the beginning
     * if we absolutely have to... */

    kpa_t where
        /* The following value is used in the "grab from end" case.
           We assign it here to avoid a compiler warning. */
        = allocTargetBoundAligned - nBytes;
    if (bound == allocTargetBoundAligned) {
      /* We can grab from the end, discarding only bytes lost to alignment. */
      where = bound - nBytes;
      allocTarget->allocBound = where;
    } else if (base == allocTargetBaseAligned) {
      /* We can grab from the beginning,
         discarding only bytes lost to alignment. */
      where = base;
      allocTarget->allocBase = where + nBytes;
    } else {
      /* May need to split the region. */
      /* Physical memory must be split on a page boundary: */
      const kpa_t split = align_up(base, EROS_PAGE_SIZE);
      if (split < allocTarget->allocBound) {
        /* Split the region */
        PmemInfo * newPmi;
#if 0
        printf("Splitting: 0x%x 0x%x 0x%x 0x%x\n",
                       (unsigned)allocTarget->allocBase,
                       (unsigned)allocTarget->allocBound,
                       (unsigned)mc->base, (unsigned)mc->bound );
#endif
        where = base;
        /* Add the region with its full physical memory. */
        newPmi = physMem_AddRegion(split, allocTarget->allocBound,
                                   MI_MEMORY, allocTarget->readOnly);
        newPmi->allocBase = max(split, where + nBytes);
        allocTarget->bound = split;
        allocTarget->allocBound = where;
      } else {
        /* Just allocate from the end, discarding fewer than
           EROS_PAGE_SIZE bytes. */
        where = bound - nBytes;
        allocTarget->allocBound = where;
      }
    }

    assert(where >= base);
    assert(where + nBytes <= bound);
      
    DEBUG(alloc)
      printf("Alloc %u %% %u at 0x%08x. "
		     "Rgn 0x%08x now [0x%08x,0x%08x)\n", 
		     (uint32_t) nBytes, mc->align, (uint32_t) where, 
		     allocTarget, 
		     (unsigned) allocTarget->allocBase, 
		     (unsigned) allocTarget->allocBound);

    return where;
  }

  return - (kpa_t)1;
}

void
physMem_ReserveExact(kpa_t base, kpsize_t size)
{
  PmemConstraint constraint;
  
  constraint.base = base;
  constraint.align = 1;
  constraint.bound = base + size;
  kpa_t mem = physMem_Alloc(size, &constraint);
  assert(mem == base);	// failed to allocate it?
  (void)mem;	// Just reserve, don't return this.
}

#ifdef OPTION_DDB
void
physMem_ddb_dump()
{
  unsigned rgn = 0;
  extern void db_printf(const char *fmt, ...);

  for (rgn = 0; rgn < physMem_nPmemInfo; rgn++) {
    PmemInfo *kmi = &physMem_pmemInfo[rgn];

    kpa_t base = kmi->allocBase;
    kpa_t bound = kmi->allocBound;
    unsigned long unallocated = bound - base;
    unsigned long total = kmi->bound - kmi->base;

    printf("Rgn 0x%08x ty=%d: [0x%08x,0x%08x) %u/%u bytes available\n", 
	   kmi,
	   kmi->type,
	   (unsigned long) kmi->base,
	   (unsigned long) kmi->bound,
	   unallocated,
	   total);
    if (kmi->firstObHdr) {
      printf("  allocBase 0x%08x allocBound 0x%08x\n",
	     (unsigned long) (kmi->allocBase),
	     (unsigned long) (kmi->allocBound));
      printf("  nPages 0x%08x (%d) firstObPgAddr 0x%08x firstObHdr 0x%08x\n",
	     kmi->nPages,
	     kmi->nPages,
	     kmi->firstObPgAddr,
	     kmi->firstObHdr);
    }
  }
}
#endif
