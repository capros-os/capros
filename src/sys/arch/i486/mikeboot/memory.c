/*
 * Copyright (C) 2001, Jonathan S. Shapiro.
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

#include <kerninc/BootInfo.h>
#include "boot.h"
#include "boot-asm.h"
#include "debug.h"

#define MEM_LIMIT (64 * 1024 * 1024)

#define align_down(addr, al) (((uint32_t)(addr)) & ~((al)-1))

/* This is the type of the MemInfo structure returned by the Uruk
 * memory probe function. */
struct UrukMemInfo {
  uint32_t  len;		/* handed to/from the bios */
  uint32_t  BaseAddrLow;	/* low 32 bits of base address */
  uint32_t  BaseAddrHigh;	/* high 32 bits of base address */
  uint32_t  LengthLow;		/* low 32 bits of length */
  uint32_t  LengthHigh;		/* high 32 bits of length */
  uint32_t  Type;		/* address type of this range */
} ;
typedef struct UrukMemInfo UrukMemInfo;

/* type codes as returned by the BIOS logic. */
#define AddrRangeMemory   1
#define AddrRangeReserved 2

extern int get_mem_map(UrukMemInfo *umi, uint32_t continuation);

/* The ProbeMemory() routine determines what physical memory is
 * attached to the machine and does the initial setup so that
 * BootMalloc can later allocate memory that will be reserved to the
 * boot area.
 *
 * Note that we no longer attempt to support machines that do not
 * implement the BIOS Address Map logic -- such machines may still
 * (hypothetically) exist, but they aren't worth the bother until
 * somebody complains.  I haven't actually seen one used for anything
 * besides a boat anchor in quite some time.
 *
 * Hmm. No sooner did I add that then somebody discovered a boat
 * anchor that was alive and well, and I had to re-introduce the
 * memory forgery thing.
 */

static MemInfo *mallocSourceRegion;
static MemInfo *mallocRegion;

BootInfo *
ProbeMemory()
{
  uint32_t nextContinuation;
  uint64_t totalMemory = 0;
  uint32_t nRegion = 0;

  uint32_t mallocRegionBase = 0;
  uint32_t mallocRegionBound = 0;
  MemInfo *mi;
  DivisionInfo *di;
  BootInfo *bi;
  unsigned haveSMAP = 1;

  kpa_t memInfoRgn;

  UrukMemInfo bmi;

  /* We make two passes over memory. 
   * 
   * The first is to determine how many physical memory regions exist
   * on the machine so that we know how big a memory region descriptor
   * to allocate. In the process, we figure out where the bootstrap
   * will allocate memory from.
   *
   * The present implementation bets that the "top" region of physical
   * memory (wherever that may fall) will be sufficient to support all
   * of the memory allocations that the bootstrap code will make. A
   * more robust allocator would not make this assumption.
   *
   * Also, note that this bootstrap routine does not cope with
   * physical memories larger than 4Gbytes.
   */

  printf("BIOS System Address Map:\n");

  /* See if this BIOS supports the BIOS System Address Map call. There
   * should be at least one response with nonzero buffer for low
   * memory... */
  nextContinuation = 0;
  nextContinuation = get_mem_map(&bmi, nextContinuation);
  if (bmi.len == 0) 
    haveSMAP = 0;

  if (haveSMAP)  {
    /* Pass 1: simply count the regions: */
    nextContinuation = 0;
    do {
      nextContinuation = get_mem_map(&bmi, nextContinuation);

      if (bmi.len > 0) {
	const char *addrType = "Unk.";
	uint64_t base = bmi.BaseAddrHigh;
	uint64_t len = bmi.LengthHigh;
	uint64_t top;

	base <<= 32;
	base |= bmi.BaseAddrLow;
	
	len <<= 32;
	len |= bmi.LengthLow;

	top = base + len;
	
#ifdef MEM_LIMIT
	/* FIX: Ad hoc constraint until I fix the kernel memory
	   subsystem. */ 
	if (bmi.Type == AddrRangeMemory) {
	  if (base >= MEM_LIMIT)
	    continue;

	  if (top > MEM_LIMIT)
	    top = MEM_LIMIT;

	  len = top - base;
	  if (len == 0)
	    continue;
	}
#endif

	if (bmi.Type == AddrRangeMemory)
	  addrType = "Mem ";
	else if (bmi.Type == AddrRangeReserved)
	  addrType = "Rsrv";

	printf("  %s base=0x%08lx%08lx len=0x%08lx%08lx (%ld Kbytes)\n",
	       addrType, bmi.BaseAddrHigh, bmi.BaseAddrLow, 
	       bmi.LengthHigh, bmi.LengthLow,
	       (uint32_t) len/1024u);

	if (bmi.Type == AddrRangeMemory) {
	  totalMemory += len;

	  if (top < 0x100000000ull && /* must be low 4G region */
	      base > mallocRegionBase) {
	    mallocRegionBase = base;
	    mallocRegionBound = base + len;
	  }
	}

	nRegion++;
      }
    } while (nextContinuation);
  } 
  else {
    uint32_t hi = old_memsize(1);
    uint32_t lo = old_memsize(0);

    printf("Newer probe call not supported. lo=%d hi=%d Winging it...\n", lo, hi);

    totalMemory = 0x100000;	/* base of extended mem region */
    totalMemory += hi * 1024;

    mallocRegionBase = 0x100000;
    mallocRegionBound = totalMemory;
    nRegion = 7;		/* two real plus 5 forged */
  }

  printf("  Total Memory: 0x%08lx%08lx.\n"
	 "  Malloc region [0x%08lx, 0x%08lx)\n",
	 (uint32_t) (totalMemory >> 32),
	 (uint32_t) totalMemory,
	 (uint32_t) mallocRegionBase,
	 (uint32_t) mallocRegionBound);

  /* The malloc region bound should be word aligned, but better to
   * check: */
  if (mallocRegionBound != align_down(mallocRegionBound, 4)) {
    printf("Boot-time malloc region is mis-aligned!\n");
    halt();
  }

  /* Now, this is a bit tricky. We are grabbing the memory for the
   * region list from the top of what will momentarily become the boot 
   * malloc arena. We will patch up the data structures accordingly
   * below. 
   */
  memInfoRgn = mallocRegionBound;

  memInfoRgn -= MAX_MEMINFO * sizeof(MemInfo);
  memInfoRgn = align_down(memInfoRgn, sizeof(uint64_t));

  mi = PA2BOOT((uint32_t) memInfoRgn, MemInfo *);

  memset(mi, 0, MAX_MEMINFO * sizeof(MemInfo));

  memInfoRgn -= MAX_PRELOAD * sizeof(DivisionInfo);
  memInfoRgn = align_down(memInfoRgn, sizeof(uint64_t));

  di = PA2BOOT((uint32_t) memInfoRgn, DivisionInfo *);

  memset(di, 0, MAX_PRELOAD * sizeof(DivisionInfo));

  memInfoRgn -= sizeof(BootInfo);
  memInfoRgn = align_down(memInfoRgn, sizeof(uint64_t));

  bi = PA2BOOT((uint32_t) memInfoRgn, BootInfo *);
  bi->memInfo = mi;
  bi->nMemInfo = nRegion + 1;	/* +1 for boot malloc region */

  bi->divInfo = di;
  bi->nDivInfo = 0;

  mallocRegion = &mi[nRegion];

  printf("BootInfo 0x%08x, MemInfo 0x%08x mallocRegion 0x%08x\n", 
	 BOOT2PA(bi, void *),
	 BOOT2PA(mi, void *),
	 BOOT2PA(mallocRegion, void *));
  printf("bi->memInfo 0x%08x\n", BOOT2PA(bi->memInfo, void *));

  mallocRegion->bound = mallocRegionBound;
  mallocRegion->base = mallocRegionBound;
  mallocRegion->type = MI_BOOT;

  if (haveSMAP) {
    nRegion = 0;
    nextContinuation = 0;

    do {
      nextContinuation = get_mem_map(&bmi, nextContinuation);

      if (bmi.len > 0) {
	kpa_t base;
	kpa_t len;
	kpa_t top;

	base = bmi.BaseAddrHigh;
	len = bmi.LengthHigh;

	base <<= 32;
	len <<= 32;

	base |= bmi.BaseAddrLow;
	len |= bmi.LengthLow;

	top = base + len;
	
#ifdef MEM_LIMIT
	/* FIX: Ad hoc constraint until I fix the kernel memory
	   subsystem. */ 
	if (bmi.Type == AddrRangeMemory) {
	  if (base >= MEM_LIMIT)
	    continue;

	  if (top > MEM_LIMIT)
	    top = MEM_LIMIT;

	  len = top - base;
	  if (len == 0)
	    continue;
	}
#endif

	mi[nRegion].base = base;
	mi[nRegion].bound = top;

	switch(bmi.Type) {
	case AddrRangeMemory:
	  mi[nRegion].type = MI_MEMORY;
	  break;
	case AddrRangeReserved:
	  /* Check explicitly for the BIOS ROM, which needs to be 
	     accessable: */ 
	  if (mi[nRegion].base >= 0xc0000 && mi[nRegion].base < 0x100000) {
	    if (mi[nRegion].bound > 0x100000) {
	      printf("Bad ROM bound\n");
	      waitkbd();
	    }
	    mi[nRegion].type = MI_BOOTROM;
	  }
	  else
	    mi[nRegion].type = MI_RESERVED;
	  break;
	default:
	  mi[nRegion].type = MI_UNKNOWN;
	  break;
	}

	if (bmi.Type == AddrRangeMemory &&
	    mi[nRegion].base == mallocRegionBase &&
	    mi[nRegion].bound == mallocRegionBound) {

	  mallocSourceRegion = &mi[nRegion];
	}

	nRegion++;
      }
    } while (nextContinuation);
  }
  else {
    uint32_t low = old_memsize(0); /* mem size in KB */
    uint32_t hi = old_memsize(1);
    nRegion = 0;

    mi[nRegion].base = 0;
    mi[nRegion].bound = (low * 1024);
    mi[nRegion].type = MI_MEMORY;
    nRegion++;

    /* Forge an entry for the video region: */
    mi[nRegion].base = mi[nRegion-1].bound;
    mi[nRegion].bound = 0xa0000;
    mi[nRegion].type = MI_RESERVED;
    nRegion++;

    /* For the BIOS prom. This could be wrong, as it depends on the
     * setting of the memory window mapping code, but it should get us
     * there for now.: */
    mi[nRegion].base = 0xeec00;
    mi[nRegion].bound = 0x100000;
    mi[nRegion].type = MI_BOOTROM;
    nRegion++;

    mi[nRegion].base = 0x100000;	/* begins at 1M */
    mi[nRegion].bound = 0x100000 + (hi * 1024);
    mi[nRegion].type = MI_MEMORY;
    mallocSourceRegion = &mi[nRegion];
    nRegion++;

    /* For the duplicate BIOS prom mapping. This could be wrong, as it
     * depends on the setting of the memory window mapping code, but
     * it should get us there for now.: */
    mi[nRegion].base = 0xfec00000;
    mi[nRegion].bound = 0xfec10000;
    mi[nRegion].type = MI_RESERVED;
    nRegion++;

    /* Damned if I remember what this one is for, but I do remember a
     * reserved region here too. */
    mi[nRegion].base = 0xfee00000;
    mi[nRegion].bound = 0xfee01000;
    mi[nRegion].type = MI_RESERVED;
    nRegion++;

    /* And finally, the location of the hardware powerup boot vector: */
    mi[nRegion].base = 0xfff80000;
    mi[nRegion].bound = 0x100000000ull;
    mi[nRegion].type = MI_RESERVED;
    nRegion++;
  }

  /* Shorten the malloc source region to reflect what we have
   * already taken out, and likewise set the base of the
   * mallocRegion to reflect that.
   */

  mallocSourceRegion->bound = memInfoRgn;
  mallocRegion->base = memInfoRgn;

  /* Now we can finally call BootMalloc()! */

#if 1
  if (haveSMAP == 0) {
    ShowMemory(bi);

    printf("Press any key to load kernel\n");
    waitkbd();
  }
#endif


  return bi;
}

void
ShowMemory(BootInfo *bi)
{
  unsigned i;

  printf("In ShowMemory()... %d regions\n", bi->nMemInfo);

  for (i = 0; i < bi->nMemInfo; i++) {
    const char *addrType;
    MemInfo *pmi = &bi->memInfo[i];

    switch(pmi->type) {
    case MI_UNUSED:
      addrType = "Unused!:    ";
      break;
    case MI_MEMORY:
      addrType = "Memory:     ";
      break;
    case MI_RESERVED:
      addrType = "Reserved:   ";
      break;
    case MI_BOOT:
      addrType = "BootMalloc: ";
      break;
    case MI_RAMDISK:
      addrType = "RamDisk:    ";
      break;
    case MI_DEVICEMEM:
      addrType = "Device:     ";
      break;
    case MI_UNKNOWN:
      addrType = "Unknown:    ";
      break;
    case MI_BOOTROM:
      addrType = "BOOTROM:    ";
      break;
    case MI_PRELOAD:
      addrType = "Preload:    ";
      break;
    default:
      addrType = "???:        ";
      break;
    }

    printf("  %s [0x%08lx%08lx, 0x%08lx%08lx)\n",
	   addrType, 
	   (uint32_t) (pmi->base >> 32),
	   (uint32_t) (pmi->base),
	   (uint32_t) (pmi->bound >> 32),
	   (uint32_t) (pmi->bound));
  }
}

void *
BootAlloc(size_t sz, unsigned alignment)
{
  void *dest;
  kpsize_t avail;

  if ( align_down(mallocSourceRegion->base, alignment) != 
       mallocSourceRegion->base ) {
    printf("base is misaligned\n");
    halt();
  }

  avail = mallocSourceRegion->bound - mallocSourceRegion->base;
  if (avail < sz) {
    printf("Unable to allocate %d bytes with %d alignment\n",
	   sz, alignment);
    halt();
  }

  mallocSourceRegion->bound -= sz;
  mallocSourceRegion->bound = align_down(mallocSourceRegion->bound, alignment);

  mallocRegion->base = mallocSourceRegion->bound;

  DEBUG(bootalloc) 
    printf("BootAlloc(%d %% %d) yields 0x%08x\n",
	   sz, alignment, (uint32_t) mallocSourceRegion->bound);

  dest = (void *) PA2BOOT(mallocSourceRegion->bound, uint32_t);

  memset(dest, 0, sz);

  return dest;
}

void
BindRegion(BootInfo *bi, kpa_t base, kpsize_t len, unsigned type)
{
  MemInfo *mi = &bi->memInfo[bi->nMemInfo];

  if (bi->nMemInfo == MAX_MEMINFO) {
    printf("Total allowable count exceeded.\n");
    halt();
  }

  bi->nMemInfo++;

  mi->base = base;
  mi->bound = base + len;
  mi->type = type;
}
