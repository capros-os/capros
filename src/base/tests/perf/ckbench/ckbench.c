/*
 * Copyright (C) 1998, 1999, Jonathan S. Shapiro.
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
#include <eros/SleepKey.h>
#include <eros/ProcessKey.h>
#include <eros/NodeKey.h>
#include <eros/KeyConst.h>
#include <domain/domdbg.h>
#include <domain/SpaceBankKey.h>
#include <domain/ConstructorKey.h>
#include <eros/SysTraceKey.h>

#define KR_ZSF      1
#define KR_SELF     2
#define KR_SCHED    3
#define KR_BANK     4
#define KR_OSTREAM  5
#define KR_SYSTRACE 6
#define KR_MYSEG    7
#define KR_VOIDSEG  8
#define KR_SCRATCH  9

#define PPM         256		/* pages per megabyte */
#define MBYTES      128
#define TEST_ADDR   0x08000000

/* MUST use zero stack pages so that seg root doesn't get
   smashed by bootstrap code. */
const uint32_t __rt_stack_pages = 0;
const uint32_t __rt_stack_pointer = 0x10000;

void
setup()
{
  uint32_t result;
  
  spcbank_buy_nodes(KR_BANK, 1, KR_MYSEG, KR_VOID, KR_VOID);
  node_make_node_key(KR_MYSEG, 6, KR_MYSEG);
  
  process_copy(KR_SELF, ProcAddrSpace, KR_SCRATCH);

  node_swap(KR_MYSEG, 0x0, KR_SCRATCH, KR_VOID);

  result = constructor_request(KR_ZSF, KR_BANK, KR_SCHED, KR_VOID,
			 KR_VOIDSEG);

  node_swap(KR_MYSEG, 0x1, KR_VOIDSEG, KR_VOID);

  process_swap(KR_SELF, ProcAddrSpace, KR_MYSEG, KR_VOID);
}

void
main()
{
  int i;
  uint32_t *addr = (uint32_t *) TEST_ADDR;
  uint32_t pat_counter = 0;
  uint64_t sum = 0;
  
  struct SysTrace st;

  setup();

  kprintf(KR_OSTREAM, "Grow the segment\n");


  for (i = 0; i < MBYTES; i++) {
    int pg;
    systrace_clear_kstats(KR_SYSTRACE);
    systrace_start(KR_SYSTRACE, SysTrace_Mode_Cycles);
    for (pg = 0; pg < PPM; pg++) {
      int w;
      /* sum += *addr; */
      for (w = 0; w < (EROS_PAGE_SIZE/sizeof(uint32_t)); w++)
	*addr++ = pat_counter++;
    }
    systrace_stop(KR_SYSTRACE);

    kprintf(KR_OSTREAM, "Allocated %d mbytes\n", i+1);
  }
    
  kprintf(KR_OSTREAM, "Fully allocated; doing sum pass now\n");

  addr = (uint32_t *) TEST_ADDR;
  for (i = 0; i < MBYTES; i++) {
    int pg;
    for (pg = 0; pg < PPM; pg++) {
      int w;
      for (w = 0; w < (EROS_PAGE_SIZE/sizeof(uint32_t)); w++)
	sum = *addr++;
    }
  }
  
  systrace_report(KR_SYSTRACE, &st);
  systrace_stop(KR_SYSTRACE);

  kprintf(KR_OSTREAM, "Done -- %d MB in %U cycles sum %U\n",
	  MBYTES, st.cycles, sum);

  kprintf(KR_OSTREAM, "Starting stress tests...\n");

  for(;;) {
    addr = (uint32_t *) TEST_ADDR;
    pat_counter = 0;
    for (i = 0; i < MBYTES; i++) {
      int pg;
      systrace_start(KR_SYSTRACE, SysTrace_Mode_Cycles);
      for (pg = 0; pg < PPM; pg++) {
	int w;
	/* sum += *addr; */
	for (w = 0; w < (EROS_PAGE_SIZE/sizeof(uint32_t)); w++)
	  *addr++ = pat_counter++;
      }
      systrace_stop(KR_SYSTRACE);
      kprintf(KR_OSTREAM, "Touched %d mbytes\n", i+1);
    }
  }
}
