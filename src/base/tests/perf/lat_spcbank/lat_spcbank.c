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
#include <idl/eros/Sleep.h>
#include <eros/ProcessKey.h>
#include <eros/NodeKey.h>
#include <eros/KeyConst.h>
#include <domain/domdbg.h>
#include <domain/SpaceBankKey.h>
#include <eros/SysTraceKey.h>

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

int
main()
{
  int i; int pass;
  
  struct SysTrace st[NPASS];
  
  eros_Sleep_sleep(KR_SLEEP, 4000);

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
    systrace_start(KR_SYSTRACE, SysTrace_Mode_Cycles);

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

    systrace_report(KR_SYSTRACE, &st[pass]);
  }

  if (spcbank_return_data_page(KR_BANK, KR_PAGE))
    kdprintf(KR_OSTREAM, "Page return failed\n");

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
  
  /* Warm up the space bank for page tests: */

  /* Buy needed pages: */
  for (i = 0; i < NPAGES; i++) {
    if (spcbank_buy_data_pages(KR_BANK, 1, KR_PAGE, KR_VOID, KR_VOID))
      kdprintf(KR_OSTREAM, "Page purchase failed\n");

    node_copy(KR_PGTREE, i >> (EROS_NODE_LGSIZE*2), KR_WALK);
    node_copy(KR_WALK, (i >> EROS_NODE_LGSIZE) & EROS_NODE_SLOT_MASK, KR_WALK);
    node_swap(KR_WALK, i & EROS_NODE_SLOT_MASK, KR_PAGE, KR_VOID);
  }
  
  /* Now sell them back: */
  for (i = 0; i < NPAGES; i++) {
    node_copy(KR_PGTREE, i >> (EROS_NODE_LGSIZE*2), KR_WALK);
    node_copy(KR_WALK, (i >> EROS_NODE_LGSIZE) & EROS_NODE_SLOT_MASK, KR_WALK);
    node_copy(KR_WALK, i & EROS_NODE_SLOT_MASK, KR_PAGE);

    if (spcbank_return_data_page(KR_BANK, KR_PAGE))
      kdprintf(KR_OSTREAM, "Page return failed\n");
  }
  
  kprintf(KR_OSTREAM, "Bank is warm; starting %d page measurements\n",
	  NPAGES);

  for (pass = 0; pass < NPASS; pass++) {
    systrace_start(KR_SYSTRACE, SysTrace_Mode_Cycles);

    /* Buy needed pages: */
    for (i = 0; i < NPAGES; i++) {
      spcbank_buy_data_pages(KR_BANK, 1, KR_PAGE, KR_VOID, KR_VOID);

      node_copy(KR_PGTREE, i >> (EROS_NODE_LGSIZE*2), KR_WALK);
      node_copy(KR_WALK, (i >> EROS_NODE_LGSIZE) &
		EROS_NODE_SLOT_MASK, KR_WALK); 
      node_swap(KR_WALK, i & EROS_NODE_SLOT_MASK, KR_PAGE, KR_VOID);
    }
  
    /* Now sell them back: */
    for (i = 0; i < NPAGES; i++) {
      node_copy(KR_PGTREE, i >> (EROS_NODE_LGSIZE*2), KR_WALK);
      node_copy(KR_WALK, (i >> EROS_NODE_LGSIZE) &
		EROS_NODE_SLOT_MASK, KR_WALK); 
      node_copy(KR_WALK, i & EROS_NODE_SLOT_MASK, KR_PAGE);

      spcbank_return_data_page(KR_BANK, KR_PAGE);
    }

    systrace_report(KR_SYSTRACE, &st[pass]);
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
  
  /* Warm up the space bank for node tests: */

  /* Buy needed nodes: */
  for (i = 0; i < NNODES; i++) {
    if (spcbank_buy_nodes(KR_BANK, 1, KR_NODE, KR_VOID, KR_VOID))
      kdprintf(KR_OSTREAM, "Node purchase failed\n");

    node_copy(KR_PGTREE, i >> (EROS_NODE_LGSIZE*2), KR_WALK);
    node_copy(KR_WALK, (i >> EROS_NODE_LGSIZE) & EROS_NODE_SLOT_MASK, KR_WALK);
    node_swap(KR_WALK, i & EROS_NODE_SLOT_MASK, KR_NODE, KR_VOID);
  }
  
  /* Now sell them back: */
  for (i = 0; i < NNODES; i++) {
    node_copy(KR_PGTREE, i >> (EROS_NODE_LGSIZE*2), KR_WALK);
    node_copy(KR_WALK, (i >> EROS_NODE_LGSIZE) & EROS_NODE_SLOT_MASK, KR_WALK);
    node_copy(KR_WALK, i & EROS_NODE_SLOT_MASK, KR_NODE);

    if (spcbank_return_node(KR_BANK, KR_NODE))
      kdprintf(KR_OSTREAM, "Node return failed\n");
  }
  
  kprintf(KR_OSTREAM, "Bank is warm; starting %d node measurements\n",
	  NNODES);

  for (pass = 0; pass < NPASS; pass++) {
    systrace_start(KR_SYSTRACE, SysTrace_Mode_Cycles);

    /* Buy needed nodes: */
    for (i = 0; i < NNODES; i++) {
      spcbank_buy_nodes(KR_BANK, 1, KR_NODE, KR_VOID, KR_VOID);

      node_copy(KR_PGTREE, i >> (EROS_NODE_LGSIZE*2), KR_WALK);
      node_copy(KR_WALK, (i >> EROS_NODE_LGSIZE) &
		EROS_NODE_SLOT_MASK, KR_WALK); 
      node_swap(KR_WALK, i & EROS_NODE_SLOT_MASK, KR_NODE, KR_VOID);
    }
  
    /* Now sell them back: */
    for (i = 0; i < NNODES; i++) {
      node_copy(KR_PGTREE, i >> (EROS_NODE_LGSIZE*2), KR_WALK);
      node_copy(KR_WALK, (i >> EROS_NODE_LGSIZE) &
		EROS_NODE_SLOT_MASK, KR_WALK); 
      node_copy(KR_WALK, i & EROS_NODE_SLOT_MASK, KR_NODE);

      spcbank_return_node(KR_BANK, KR_NODE);
    }

    systrace_report(KR_SYSTRACE, &st[pass]);
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

    totcy /= (NPASS*NNODES);
    totcnt0 /= (NPASS*NNODES);
    totcnt1 /= (NPASS*NNODES);

    kprintf(KR_OSTREAM, "cy: %10u  cnt0: %10u  cnt1:%10u\n",
	    (uint32_t) totcy,
	    (uint32_t) totcnt0,
	    (uint32_t) totcnt1);
  }
  
  kprintf(KR_OSTREAM, "Done.\n");

  return 0;
}
