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
#include <eros/SysTraceKey.h>

/* The purpose of this benchmark is to measure the cost of
   reconstructing page table entries.  It is designed on the
   assumption that the actual data is in core. */

#define KR_VOID     0
#define KR_SELF     4

#define KR_SLEEP    9
#define KR_OSTREAM  10
#define KR_SYSTRACE 11

#define KR_SCRATCH  16
#define KR_SEG      17
#define KR_SUBSEG   18

const uint32_t __rt_stack_pages = 0;
const uint32_t __rt_stack_pointer = 0xC0000000;

#define NPAGES 2048		/* 16 Mbytes */
const char * map_addr = (const char *) 0x08000000;

#define ZAPALL  2
#define ZAP     1
#define NOZAP   0

uint32_t
dotrace(int zap, uint32_t mode)
{
  int i, x = 0;
  
  const char *ptr = map_addr;

  if (zap != NOZAP) {
    /* Zap the mapping entries associate with this segment. */
    process_copy(KR_SELF, ProcAddrSpace, KR_SCRATCH);
    node_copy(KR_SCRATCH, 1, KR_SEG);
    node_swap(KR_SCRATCH, 1, KR_SEG, KR_VOID);
  }
  if (zap == ZAPALL) {
#if EROS_PAGE_SIZE != 4096
#error "Check the zapall logic"
    /* This logic will only work for systems with hierarchical page
       tables in which the bottom level page table holds 1024 entries.
       Basically, we grab the slots corresponding to the page tables
       and then zap all the slots immediately *below* those to wipe
       out the contents of the page table. */
#endif
#define PTE_SIZE 4
#define NPTE_PER_PAGE (EROS_PAGE_SIZE/PTE_SIZE)
#define NPGDIR ((NPAGES + (NPTE_PER_PAGE-1))/NPTE_PER_PAGE)
    
    for (i = 0; i < NPGDIR; i++) {
      int ndSlot;
      
      node_copy(KR_SEG, i, KR_SUBSEG);
      for (ndSlot = 0; ndSlot < EROS_NODE_SIZE; ndSlot++) {
	node_copy(KR_SUBSEG, ndSlot, KR_SCRATCH);
	node_swap(KR_SUBSEG, ndSlot, KR_SCRATCH, KR_VOID);
      }
    }
  }

  systrace_start(KR_SYSTRACE, mode);

  for (i = 0; i < NPAGES; i++) {
    x += *ptr;
    ptr += EROS_PAGE_SIZE;
  }

  systrace_stop(KR_SYSTRACE);
  return x;
}

int
main()
{
  /* Pass 0: Touch every page in the map to ensure that the associated
     page is actually in core: */

  uint32_t x = dotrace(NOZAP, SysTrace_Mode_Cycles);

  /* print result to avoid loop hoisting on unused x: */
  kprintf(KR_OSTREAM, "Pass zero, x result is %d\n", x);

  sl_sleep(KR_SLEEP, 4000);

  kprintf(KR_OSTREAM, "Calibrating Touch Loop for %d pages\n",
	  NPAGES);
  
  dotrace(NOZAP, SysTrace_Mode_Cycles);
  dotrace(NOZAP, SysTrace_Mode_Cycles);
  dotrace(NOZAP, SysTrace_Mode_Cycles);
  dotrace(NOZAP, SysTrace_Mode_Cycles);

  sl_sleep(KR_SLEEP, 4000);

#if 0
  dotrace(ZAP, SysTrace_Mode_Instrs);

  dotrace(ZAP, SysTrace_Mode_Imiss);

  dotrace(ZAP, SysTrace_Mode_Dmiss);
  
  dotrace(ZAP, SysTrace_Mode_Branches);

  dotrace(ZAP, SysTrace_Mode_BrTaken);
#endif

  kprintf(KR_OSTREAM, "Beginning ZAPALL touch of %d pages\n", NPAGES);

  dotrace(ZAPALL, SysTrace_Mode_Cycles);
  dotrace(ZAPALL, SysTrace_Mode_Cycles);
  dotrace(ZAPALL, SysTrace_Mode_Cycles);
  dotrace(ZAPALL, SysTrace_Mode_Cycles);

  kprintf(KR_OSTREAM, "Beginning ZAP touch of %d pages\n", NPAGES);

  dotrace(ZAP, SysTrace_Mode_Cycles);
  dotrace(ZAP, SysTrace_Mode_Cycles);
  dotrace(ZAP, SysTrace_Mode_Cycles);
  dotrace(ZAP, SysTrace_Mode_Cycles);

  systrace_clear_kstats(KR_SYSTRACE);

  dotrace(ZAP, SysTrace_Mode_Cycles);

  kdprintf(KR_OSTREAM, "Done.  X is %d.  Last pass has valid kstats\n", x);

  return 0;
}
