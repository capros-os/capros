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
#include <eros/ProcessKey.h>
#include <eros/NodeKey.h>
#include <eros/KeyConst.h>
#include <domain/domdbg.h>
#include <domain/SpaceBankKey.h>
#include <domain/ConstructorKey.h>
#include <idl/capros/SysTrace.h>

#define KR_ZSF      1
#define KR_SELF     2
#define KR_SCHED    3
#define KR_BANK     4
#define KR_OSTREAM  5
#define KR_SYSTRACE 6
#define KR_SLEEP    7
#define KR_MYSEG    8
#define KR_VOIDSEG  9
#define KR_SCRATCH  10

#define NPAGES      256
#define TEST_ADDR   0x2000000

/* MUST use zero stack pages so that seg root doesn't get
   smashed by bootstrap code. */
const uint32_t __rt_stack_pages = 0;
const uint32_t __rt_stack_pointer = 0x10000;

void
setup()
{
  uint32_t result;
  
  spcbank_buy_nodes(KR_BANK, 1, KR_MYSEG, KR_VOID, KR_VOID);
  node_make_node_key(KR_MYSEG, 5, KR_MYSEG);
  
  process_copy(KR_SELF, ProcAddrSpace, KR_SCRATCH);

  node_swap(KR_MYSEG, 0x0, KR_SCRATCH, KR_VOID);

  result = constructor_request(KR_ZSF, KR_BANK, KR_SCHED, KR_VOID,
			 KR_VOIDSEG);

  node_swap(KR_MYSEG, 0x8, KR_VOIDSEG, KR_VOID);

  process_swap(KR_SELF, ProcAddrSpace, KR_MYSEG, KR_VOID);
}

void
trace_pass(uint32_t mode, eros_SysTrace_info *pst)
{
  uint32_t result;
  char *addr;
  int i;
  
  result = key_destroy(KR_VOIDSEG);
#if 0
  kprintf(KR_OSTREAM, "Destroy VCS: 0x%08x\n", result);
#endif

  result = constructor_request(KR_ZSF, KR_BANK, KR_SCHED, KR_VOID,
			 KR_VOIDSEG);

#if 0
  kprintf(KR_OSTREAM, "New VCS. Result: 0x%08x\n", result);
#endif

  node_swap(KR_MYSEG, 0x8, KR_VOIDSEG, KR_VOID);

#if 0
  kprintf(KR_OSTREAM, "New VCS created and in place...\n");

  kprintf(KR_OSTREAM, "Begin warm-case tracing\n");
#endif

  eros_SysTrace_clearKernelStats(KR_SYSTRACE);
  eros_SysTrace_startCounter(KR_SYSTRACE, mode);

  addr = (char *) TEST_ADDR;
  for (i = 0; i < NPAGES; i++) {
    /* sum += *addr; */
    *addr = 1;
    addr += EROS_PAGE_SIZE;
  }
    
  if (pst)
    eros_SysTrace_reportCounter(KR_SYSTRACE, pst);
  eros_SysTrace_stopCounter(KR_SYSTRACE);
}

int
main()
{
  int i;
  char *addr = (char *) TEST_ADDR;
  uint32_t sum=0;
  
  eros_SysTrace_info st;

  setup();

  kprintf(KR_OSTREAM, "Sleep a while\n");
  capros_Sleep_sleep(KR_SLEEP, 4000);
  kprintf(KR_OSTREAM, "Begin cold-case tracing\n");

  eros_SysTrace_startCounter(KR_SYSTRACE, eros_SysTrace_mode_cycles);

  for (i = 0; i < NPAGES + 32; i++) {
    /* sum += *addr; */
    *addr = 1;
    addr += EROS_PAGE_SIZE;
  }
    
  eros_SysTrace_reportCounter(KR_SYSTRACE, &st);
  eros_SysTrace_stopCounter(KR_SYSTRACE);

  addr = (char *) TEST_ADDR;

  for (i = 0; i < NPAGES + 32; i++) {
    sum += *addr;
    addr += EROS_PAGE_SIZE;
  }
    
  st.cycles /= NPAGES + 32;

  kprintf(KR_OSTREAM, "Done -- %d pages, each %u cycles sum %u.\n",
	  NPAGES + 32, (uint32_t) st.cycles, sum);

  kprintf(KR_OSTREAM, "Begin warm-case tracing\n");

  trace_pass(eros_SysTrace_mode_Imiss, 0);
  trace_pass(eros_SysTrace_mode_Dmiss, 0);
  trace_pass(eros_SysTrace_mode_ITLB, 0);
  trace_pass(eros_SysTrace_mode_DTLB, 0);

  st.cycles = 0;

  trace_pass(eros_SysTrace_mode_cycles, &st);

  st.cycles /= NPAGES;
  kprintf(KR_OSTREAM, "Done -- %d pages, each %u cycles.\n",
	  NPAGES, (uint32_t) st.cycles);
 
  addr = (char *) TEST_ADDR;
  for (i = 0; i < NPAGES; i++) {
    sum += *addr;
    addr += EROS_PAGE_SIZE;
  }

  return 0;
}
