/*
 * Copyright (C) 1998, 1999, Jonathan S. Shapiro.
 * Copyright (C) 2005, Strawberry Development Group
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


#include <eros/target.h>
#include <eros/Invoke.h>
#include <idl/capros/Sleep.h>
#include <eros/NodeKey.h>
#include <eros/KeyConst.h>
#include <domain/domdbg.h>
#include <domain/SpaceBankKey.h>
#include <idl/capros/SysTrace.h>

/* The purpose of this benchmark is to measure the cost of
   reconstructing page table entries.  It is designed on the
   assumption that the actual data is in core. */

#define KR_VOID 0
#define KR_SELF 4

#define KR_BANK 8
#define KR_PGTREE 8
#define KR_SLEEP 9
#define KR_OSTREAM 10
#define KR_SYSTRACE 11

#define KR_WALK 15
#define KR_PAGE 16
#define KR_NODE 16
#define KR_SEG     17

const uint32_t __rt_stack_pages = 0;
const uint32_t __rt_stack_pointer = 0x20000;

#define NPASS  5
#define NNODES 2048		/* 16 Mbytes */
#define NPAGES 2048		/* 16 Mbytes */

void main()
{
  int i; int pass;
  
  eros_SysTrace_info st[NPASS];
  
  capros_Sleep_sleep(KR_SLEEP, 4000);

  if (spcbank_buy_data_pages(KR_BANK, 1, KR_PAGE, KR_VOID, KR_VOID))

  /* Run a calibration pass: */
  /* Warm  up: */
  for (i = 0; i < NPAGES; i++) {
    /* do not buy */
    node_copy(KR_PGTREE, i >> (EROS_NODE_LGSIZE*2), KR_WALK);
    node_copy(KR_WALK, (i >> EROS_NODE_LGSIZE) & EROS_NODE_SLOT_MASK, KR_WALK);
    node_swap(KR_WALK, i & EROS_NODE_SLOT_MASK, KR_PAGE, KR_VOID);
  }
  
  for (i = 0; i < NPAGES; i++) {
    node_copy(KR_PGTREE, i >> (EROS_NODE_LGSIZE*2), KR_WALK);
    node_copy(KR_WALK, (i >> EROS_NODE_LGSIZE) & EROS_NODE_SLOT_MASK, KR_WALK);
    node_copy(KR_WALK, i & EROS_NODE_SLOT_MASK, KR_WALK);
    /* ignore result */
  }

  /* Timing pass: */
  kprintf(KR_OSTREAM, "Following is cost of %d tree manipulations:\n",
	  NPAGES);

  for (pass = 0; pass < NPASS; pass++) {
    eros_SysTrace_clearKernelStats(KR_SYSTRACE);
    eros_SysTrace_startCounter(KR_SYSTRACE, eros_SysTrace_mode_cycles);

    for (i = 0; i < NPAGES; i++) {
      node_copy(KR_PGTREE, i >> (EROS_NODE_LGSIZE*2), KR_WALK);
      node_copy(KR_WALK, (i >> EROS_NODE_LGSIZE) &
		EROS_NODE_SLOT_MASK, KR_WALK); 
      node_swap(KR_WALK, i & EROS_NODE_SLOT_MASK, KR_PAGE, KR_VOID);
    }
  
    for (i = 0; i < NPAGES; i++) {
      node_copy(KR_PGTREE, i >> (EROS_NODE_LGSIZE*2), KR_WALK);
      node_copy(KR_WALK, (i >> EROS_NODE_LGSIZE) &
		EROS_NODE_SLOT_MASK, KR_WALK); 
      node_copy(KR_WALK, i & EROS_NODE_SLOT_MASK, KR_PAGE);

    }

    eros_SysTrace_reportCounter(KR_SYSTRACE, &st[pass]);
  }

  {
    uint64_t totcy = 0;
    uint64_t totcnt0 = 0;
    uint64_t totcnt1 = 0;

    for (pass = 0; pass < NPASS; pass++) {
      totcy += st[pass].cycles;
      totcnt0 += st[pass].count0;
      totcnt1 += st[pass].count1;
      st[pass].cycles = 0;
      st[pass].count0 = 0;
      st[pass].count1 = 0;
    }

    totcy /= (NPASS*NPAGES);
    totcnt0 /= (NPASS*NPAGES);
    totcnt1 /= (NPASS*NPAGES);

    kprintf(KR_OSTREAM, "cy: %10u  cnt0: %10u  cnt1:%10u\n",
	    (uint32_t) totcy,
	    (uint32_t) totcnt0,
	    (uint32_t) totcnt1);
  }
  kdprintf(KR_OSTREAM, "Done.  Last pass left valid kstats\n");

  if (spcbank_return_data_page(KR_BANK, KR_PAGE))
    kdprintf(KR_OSTREAM, "Page return failed\n");
}
