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

#define KR_SELF     2
#define KR_SCHED    3
#define KR_OSTREAM  5
#define KR_SYSTRACE 6
#define KR_RANGE    7

#define KR_TMP      9

#define NPAGES    256
#define BASE_OID    0

/* MUST use zero stack pages so that seg root doesn't get
   smashed by bootstrap code. */
const uint32_t __rt_stack_pages = 0;
const uint32_t __rt_stack_pointer = 0x10000;

int
main()
{
  int i;
  struct SysTrace st;

  kprintf(KR_OSTREAM, "Begin warmup tracing\n");

  systrace_start(KR_SYSTRACE, SysTrace_Mode_Cycles);

  for (i = 0; i < NPAGES; i++) {
    uint64_t oid = BASE_OID + (i * EROS_OBJECTS_PER_FRAME);

    if (range_waitobjectkey(KR_RANGE, OT_DataPage, oid, KR_TMP) != RC_OK)
      kprintf(KR_OSTREAM, "Couldn't get page key for submap\n");
  }
  
  systrace_report(KR_SYSTRACE, &st);
  systrace_stop(KR_SYSTRACE);

  st.cycles /= NPAGES;
  kprintf(KR_OSTREAM, "Done -- %d pages, each %u cycles.\n",
	  NPAGES, (uint32_t) st.cycles);

  kprintf(KR_OSTREAM, "Begin rescind pass tracing\n");

  systrace_start(KR_SYSTRACE, SysTrace_Mode_Cycles);

  /* Now destroy all of those objects: */
  for (i = 0; i < NPAGES; i++) {
    uint64_t oid = BASE_OID + (i * EROS_OBJECTS_PER_FRAME);

    if (range_waitobjectkey(KR_RANGE, OT_DataPage, oid, KR_TMP) != RC_OK)
      kprintf(KR_OSTREAM, "Couldn't get page key for submap\n");

    range_rescind(KR_RANGE, KR_TMP);
  }

  systrace_report(KR_SYSTRACE, &st);
  systrace_stop(KR_SYSTRACE);

  st.cycles /= NPAGES;
  kprintf(KR_OSTREAM, "Done -- %d pages, each %u cycles.\n",
	  NPAGES, (uint32_t) st.cycles);


  kprintf(KR_OSTREAM, "Begin warm page alloc tracing\n");

  systrace_start(KR_SYSTRACE, SysTrace_Mode_Cycles);

  for (i = 0; i < NPAGES; i++) {
    uint64_t oid = BASE_OID + (i * EROS_OBJECTS_PER_FRAME);

    if (range_waitobjectkey(KR_RANGE, OT_DataPage, oid, KR_TMP) != RC_OK)
      kprintf(KR_OSTREAM, "Couldn't get page key for submap\n");
  }
  
  systrace_report(KR_SYSTRACE, &st);
  systrace_stop(KR_SYSTRACE);

  st.cycles /= NPAGES;
  kprintf(KR_OSTREAM, "Done -- %d pages, each %u cycles.\n",
	  NPAGES, (uint32_t) st.cycles);

  /* Now destroy all of those objects: */
  for (i = 0; i < NPAGES; i++) {
    uint64_t oid = BASE_OID + (i * EROS_OBJECTS_PER_FRAME);

    if (range_waitobjectkey(KR_RANGE, OT_DataPage, oid, KR_TMP) != RC_OK)
      kprintf(KR_OSTREAM, "Couldn't get page key for submap\n");

    range_rescind(KR_RANGE, KR_TMP);
  }

  /* The following case is deliberately done COLD, to capture the cost
     of converting page frames to node frames: */
  kprintf(KR_OSTREAM, "Begin warm node alloc tracing\n");

  systrace_start(KR_SYSTRACE, SysTrace_Mode_Cycles);

  for (i = 0; i < NPAGES; i++) {
    int j;
    for (j = 0; j < EROS_NODES_PER_FRAME; j++) {

      uint64_t oid = BASE_OID + (i * EROS_OBJECTS_PER_FRAME) + j;

      if (range_waitobjectkey(KR_RANGE, OT_Node, oid, KR_TMP) != RC_OK)
	kprintf(KR_OSTREAM, "Couldn't get page key for submap\n");
    }
  }
  
  systrace_report(KR_SYSTRACE, &st);
  systrace_stop(KR_SYSTRACE);

  st.cycles /= (NPAGES*EROS_NODES_PER_FRAME);
  kprintf(KR_OSTREAM, "Done -- %d nodes (%d frames), each %u cycles.\n",
	  (NPAGES*EROS_NODES_PER_FRAME),
	  NPAGES, (uint32_t) st.cycles);


  return 0;
}
