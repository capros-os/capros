#ifndef __PTEARM_H__
#define __PTEARM_H__
/*
 * Copyright (C) 1998, 1999, Jonathan S. Shapiro.
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

#include <eros/machine/target-asm.h>

#define PID_SHIFT 25
#define PID_MASK 0xfe000000

/* For Level 1 descriptors: */
#define L1D_ADDR_SHIFT 20
#define L1D_VALIDBITS 0x03
#define L1D_COARSE_PT 0x11 /* the 0x10 bit is required by ARM920 */
#define L1D_SECTION   0x12 /* the 0x10 bit is required by ARM920 */
#define L1D_DOMAIN_SHIFT 5
#define L1D_COARSE_PT_ADDR 0xfffffc00

/* CPT: Coarse Page Table */
#define CPT_ADDR_MASK 0x000ff000
#define CPT_ENTRIES 0x100	/* number of entries in a CPT */
#define CPT_SIZE 0x400		/* size of a coarse page table */
#define CPT_SPAN 0x100000	/* virtual memory defined by a CPT */
#define CPT_VALIDBITS  0x3
#define CPT_SMALL_PAGE 0x2
#define CPT_PAGE_MASK 0xfffff000

/* If a level 1 descriptor is all zero, there is nothing mapped there. 
   If L1D_VALIDBITS are zero but there are other bits nonzero,
   then it should be a coarse page table descriptor (as though
   L1D_VALIDBITS were 0b01), but it is temporarily invalid because
   we are tracking whether the page table is recently used. 
   (There are always nonzero bits in this case because the page table
   address is never zero.) */
/* We never use fine page tables. */

/* For Level 2 descriptors (PTEs): */
#if 0
#define PTE_ACC  0x020	/* accessed */
#define PTE_DRTY 0x040	/* dirty */
#endif
#define PTE_CACHEABLE  0x8
#define PTE_BUFFERABLE 0x4
#define PTE_VALIDBITS 0x003
#define PTE_SMALLPAGE 0x002 /* "small" is 4KB, the size we use */
/* We never use large or tiny pages. */
#define PTE_FRAMEBITS 0xfffff000

/*
 * This table shows all the various types of access control used
 * in PTEs.
 * Note that in the Control Register (CP15 register 1), S=1 and R=0.

Domain    Descriptor     Access   Meaning
          [1:0] C B  AP Priv User

 n/a       00   0 0  00 none none PTE_ZAPPED Nothing mapped there.
 n/a       00   0 1  00 none none PTE_IN_PROGRESS (note 1)
 n/a       00   1 x  xx none none tracking LRU (note 2)
no access  10   x x  xx none none something mapped but not for this process
 manager   10   x x  xx  rw  n/a  for kernel copy across 2 small spaces?
 client    10   x x  11  rw   rw  normal user R/W area
 client    10   1 1  10  rw   ro  normal user RO area
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

Note 3:
   This PTE should grant write access (as though AP=0b11 and B=1),
   but is temporarily read-only because we are tracking whether the page
   is dirty, or because of some other write hazard.
   This provides a mechanism to efficiently restore the access. 
 */
#define PTE_ZAPPED       0x0	/* designates nothing */
#define PTE_IN_PROGRESS  PTE_BUFFERED

#ifndef __ASSEMBLER__

extern bool PteZapped;

/* FLPT_FCSEPA is the physical address of the First Level Page Table that is
   used for the kernel
   and for processes using the Fast Context Switch Extension. */
extern kpa_t FLPT_FCSEPA;
extern uint32_t * FLPT_FCSEVA;	/* Virtual address of the above */

struct PTE {
  uint32_t w_value;
};

INLINE void PTE_Set(PTE * pte, uint32_t val)
{
  pte->w_value = val;
}

#ifdef KVA_PTEBUF
/* Defined in IPC-vars.c: */
extern PTE* pte_kern_ptebuf;
#endif

struct PageHeader;
struct MapTabHeader;
INLINE struct PageHeader *
MapTab_ToPageH(struct MapTabHeader * mth)
{
  return (struct PageHeader *)mth;
}

void mach_FlushBothTLBs(void);
bool LoadWordFromUserSpace(uva_t userAddr, uint32_t * resultP);

#endif /* __ASSEMBLER__  */
#endif /* __PTEARM_H__ */
