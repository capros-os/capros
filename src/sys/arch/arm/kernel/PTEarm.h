#ifndef __PTEARM_H__
#define __PTEARM_H__
/*
 * Copyright (C) 1998, 1999, Jonathan S. Shapiro.
 * Copyright (C) 2006, 2007, 2008, Strawberry Development Group.
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
Research Projects Agency under Contract Nos. W31P4Q-06-C-0040 and
W31P4Q-07-C-0070.  Approved for public release, distribution unlimited. */

#include <eros/machine/target-asm.h>

#define PID_SHIFT 25
#define PID_MASK 0xfe000000

/* PID_IN_PROGRESS is stored in Process.md.pid while we are traversing
the memory tree.
If the traversal Yields, PID_IN_PROGRESS can remain in Process.md.pid
and the process can be dispatched with this pid. 

Process.md.pid must be < UserEndVA. Otherwise, a user process 
that is privileged (running in System mode) that attempts to access memory 
below 0x02000000 (thus using the pid, which may be PID_IN_PROGRESS)
could inadvertently access kernel memory instead. */
#define PID_IN_PROGRESS 0x02000000

/* For Level 1 descriptors: */
#define L1D_ADDR_SHIFT 20
#define L1D_VALIDBITS 0x03
#define L1D_COARSE_PT 0x11 /* the 0x10 bit is required by ARM920 */
#define L1D_SECTION   0x12 /* the 0x10 bit is required by ARM920 */
#define L1D_DOMAIN_SHIFT 5
#define L1D_DOMAIN_MASK 0x1e0
#define L1D_COARSE_PT_ADDR 0xfffffc00

/*
  This table shows all the various types of access control used
  in level 1 descriptors.

[1:0] [3:2] domain [31:10] meaning
  00    00     0      0    PTE_ZAPPED Nothing mapped there (note D)
  00    01     0      0    PTE_IN_PROGRESS (notes A,D)
  00    00     0     !=0   domain stolen (note B)
  00    00    !=0    !=0   tracking LRU (note C)
  01    00     0     !=0   coarse page table descriptor (kernel only)
  01    00    !=0    !=0   coarse page table descriptor (user space)
  10    CB     0     any   section descriptor (kernel only)
  (we never use fine page tables)

Note A:
  PTE_IN_PROGRESS is used during mapping construction to detect dependency
  zaps on the descriptor under construction. When building a descriptor,
  if the existing descriptor is invalid, first set it to this value, then do
  the necessary translation, then check that the descriptor is not
  PTE_ZAPPED before overwriting. Note that this value represents an
  INVALID descriptor, but one that can be readily distinguished from the
  result of a call to l1d_Invalidate().
  There are no cache entries dependent on this descriptor.

Note B:
  This is a coarse page table descriptor that is temporarily invalid
  because it is for a small space (PID) that does not have a domain assigned
  (the domain was stolen after the descriptor was built).
  (This descriptor must be in the FLPT_FCSE.)
  This provides a mechanism to efficiently restore the access. 
  There may be cache entries dependent on this descriptor.

Note C:
  This is a coarse page table descriptor that is temporarily invalid
  because we are tracking whether the page table is recently used.
  This provides a mechanism to efficiently restore the access. 
  There may be cache entries dependent on this descriptor.

Note D:
   There are no cache entries dependent on this descriptor
   (or there is a pending cache flush).
*/

/* CPT: Coarse Page Table */
#define CPT_ADDR_MASK 0x000ff000
#define CPT_SPAN 0x100000	/* virtual memory defined by a CPT */
#define CPT_ENTRIES 0x100	/* number of entries in a CPT */
#define CPT_SIZE 0x400		/* size of a coarse page table in bytes */
#define CPT_LGSIZE 10		/* log2 size of a coarse page table in bytes */

/* For Level 2 descriptors (PTEs): */
#define PTE_CACHEABLE  0x8
#define PTE_BUFFERABLE 0x4
#define PTE_VALIDBITS 0x3
#define PTE_SMALLPAGE 0x2 /* "small" is 4KB, the size we use */
/* We never use large or tiny pages. */
#define PTE_FRAMEBITS 0xfffff000

/*
 * This table shows all the various types of access control used
 * in PTEs.
 * Note that in the Control Register (CP15 register 1), S=1 and R=0.

Domain    Descriptor     Access   Meaning
          [1:0] C B  AP Priv User

 n/a       00   0 0  00 none none PTE_ZAPPED Nothing mapped there. (note 4)
 n/a       00   0 1  00 none none PTE_IN_PROGRESS (notes 1,4)
 n/a       00   1 x  xx none none tracking LRU (note 2)
no access  10   x x  xx none none something mapped but not for this process
 manager   10   x x  xx  rw  n/a  for kernel copy across 2 small spaces?
 client    10   1 w  11  rw   rw  normal user R/W area (note 5)
 client    10   0 0  11  rw   rw  noncacheable user R/W area
 client    10   1 1  10  rw   ro  normal user RO area
 client    10   0 0  10  rw   ro  noncacheable user RO area
 client    10   1 0  10  rw   ro  tracking dirty (note 3)
0, client  10   x x  01  rw  none kernel-only R/W data
0, client  10   x x  00  ro  none kernel RO data, e.g. code

Note 1:
 * PTE_IN_PROGRESS is used during mapping construction to detect
 * dependency zaps on the PTE under construction. When building a PTE,
 * if the existing PTE is invalid, first set it to this value, then do
 * the necessary translation, then check that the PTE is not
 * PTE_ZAPPED before overwriting. Note that this value represents an
 * INVALID PTE, but one that can be readily distinguished from the
 * result of a call to pte_Invalidate().

Note 2:
   This PTE should be valid (as though the low two bits were PTE_SMALLPAGE),
   but is temporarily invalid because we are tracking whether the page is
   recently referenced.
   This provides a mechanism to efficiently restore the access. 
   There may be cache entries dependent on this PTE.

Note 3:
   This PTE should grant write access (as though AP=0b11),
   but is temporarily read-only because we are tracking whether the page
   is dirty, or because of some other write hazard.
   This provides a mechanism to efficiently restore the access. 

Note 4:
   There are no cache entries dependent on this PTE
   (or there is a pending cache flush).

Note 5:
   The B bit is 1 if the WRITEBACK option is used, otherwise 0.
 */
#define PTE_ZAPPED       0x0	/* designates nothing */
#define PTE_IN_PROGRESS  PTE_BUFFERABLE

/* Values passed to PageFault(): */
#define prefetchAbort 0
#define dataAbort     1
#define CSwapLoad     2
#define CSwapStore    3

/* Bits for mapWork are defined here.
 * These bits are gathered into a single word so they can be quickly tested
 * in UpdateTLB().
 * These bits help the kernel avoid invalidating and cleaning the TLB
 * and cache more often than necessary.
 *
 * MapWork_UserTLBWrong means that some user-accessible TLB entries
 * may be incorrect.
 *
 * MapWork_UserCacheWrong means that some user-accessible cache entries
 * may be incorrect.
 *
 * MapWork_UserDirtyWrong means that there may be user-accessible
 * dirty cache entries for pages not marked CACHEADDR_WRITEABLE.
 *
 * MapWork_KernClearedTLB means that the TLB has been invalidated
 * since the last time user-accessible TLB entries may have been created,
 * therefore there are no user-accessible TLB entries.
 *
 * MapWork_KernInvalidatedCache means that the cache has been invalidated
 * since the last time user-accessible cache entries may have been created,
 * therefore there are no user-accessible cache entries.
 *
 * MapWork_KernCleanedCache means that the cache has been cleaned
 * since the last time user-accessible dirty cache entries
 * may have been created,
 * therefore there are no user-accessible dirty cache entries.
 */
#define MapWork_UserTLBWrong         0x1
#define MapWork_UserCacheWrong       0x2
#define MapWork_UserDirtyWrong       0x4
#define MapWork_KernClearedTLB       0x100
#define MapWork_KernInvalidatedCache 0x200
#define MapWork_KernCleanedCache     0x400

#ifndef __ASSEMBLER__

#include <kerninc/ObjectHeader.h>

/* FLPT_FCSEPA is the physical address of the First Level Page Table that is
   used for processes using the Fast Context Switch Extension. */
extern kpa_t FLPT_FCSEPA;
extern uint32_t * FLPT_FCSEVA;	/* Virtual address of the above */

extern uint32_t * HighCPTVA;

/* FLPT_NullPA is the physical address of the Null First Level Page Table. */
extern kpa_t FLPT_NullPA;
extern uint32_t * FLPT_NullVA;	/* Virtual address of the above */

// MapWork_* are defined above.
extern unsigned int mapWork;

//#define TRACK_MAPWORK

INLINE void
SetMapWork_TLB(void)
{
#ifdef TRACK_MAPWORK
  printf("SetMapWork %#x -> %#x\n", mapWork, mapWork | MapWork_UserTLBWrong);
#endif
  mapWork |= MapWork_UserTLBWrong;
}

INLINE void
SetMapWork_CleanCache(void)
{
#ifdef TRACK_MAPWORK
  printf("SetMapWork %#x -> %#x\n", mapWork, mapWork | MapWork_UserDirtyWrong);
#endif
  mapWork |= MapWork_UserDirtyWrong;
}

INLINE void
SetMapWork_InvalidateCache(void)
{
#ifdef TRACK_MAPWORK
  printf("SetMapWork %#x -> %#x\n", mapWork,
         mapWork | MapWork_UserDirtyWrong | MapWork_UserCacheWrong);
#endif
  mapWork |= MapWork_UserDirtyWrong | MapWork_UserCacheWrong;
}

INLINE void
SetMapWork_TLBCache(void)
{
#ifdef TRACK_MAPWORK
  dprintf(true, "SetMapWork %#x -> %#x\n", mapWork,
         MapWork_UserTLBWrong | MapWork_UserDirtyWrong | MapWork_UserCacheWrong);
#endif
  mapWork = MapWork_UserTLBWrong | MapWork_UserDirtyWrong | MapWork_UserCacheWrong;
}


struct PTE {
  uint32_t w_value;
};

INLINE void PTE_Set(struct PTE * pte, uint32_t val)
{
  pte->w_value = val;
}

#ifdef KVA_PTEBUF
/* Defined in IPC-vars.c: */
extern struct PTE * pte_kern_ptebuf;
#endif

// mth cannot be for a small space.
// (Large space not implemented yet, so that leaves CPT.)
INLINE PageHeader *
MapTab_ToPageH(MapTabHeader * mth)
{
  assert(mth->tableSize == 0);
  PageHeader * pageH = (PageHeader *)
    ((char *)mth - offsetof(PageHeader, kt_u.mp.hdrs[mth->ndxInPage]));
  assert(pageH_GetObType(pageH) == ot_PtMappingPage2);
  return pageH;
}

// mth cannot be for a small space.
INLINE void *
MapTabHeaderToKVA(MapTabHeader * mth)
{
  PageHeader * pageH = MapTab_ToPageH(mth);
  kva_t va = pageH_GetPageVAddr(pageH);
  return (void *) (va + (mth->ndxInPage)*CPT_SIZE);
}

void mach_DoMapWork(unsigned int work);
void mach_DoCacheWork(unsigned int work);
void mach_FlushBothTLBs(void);
void mach_DrainWriteBuffer(void);
void mach_FlushTLBsCaches(void);
kpa_t mach_ReadTTBR(void);
void mach_LoadTTBR(kpa_t ttbr);
void mach_LoadPID(uint32_t pid);
void mach_LoadDACR(uint32_t dacr);
bool LoadWordFromUserSpace(uva_t userAddr, uint32_t * resultP);
void LoadWordFromUserVirtualSpace(uva_t userAddr, uint32_t * resultP);
bool StoreByteToUserSpace(uva_t userAddr, uint32_t byte);
bool SafeLoadByte(uint8_t * addr, uint8_t * resultP);
bool SafeStoreByte(uint8_t * addr, uint8_t value);
MapTabHeader * AllocateCPT(void);

bool proc_DoPageFault(Process * p, uva_t va, bool isWrite, bool prompt);
void proc_ResetMappingTable(Process * p);
MapTabHeader * SmallSpace_GetMth(unsigned int ss);

#endif /* __ASSEMBLER__  */
#endif /* __PTEARM_H__ */
