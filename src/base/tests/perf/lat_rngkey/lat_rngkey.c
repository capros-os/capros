/*
 * Copyright (C) 1998, 1999, Jonathan S. Shapiro.
 * Copyright (C) 2005, 2007, 2008, Strawberry Development Group
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

#include <eros/target.h>
#include <eros/Invoke.h>
#include <disk/DiskNode.h>
#include <domain/domdbg.h>
#include <idl/capros/SysTrace.h>

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
  eros_SysTrace_info st;

  kprintf(KR_OSTREAM, "Begin warmup tracing\n");

  eros_SysTrace_startCounter(KR_SYSTRACE, eros_SysTrace_mode_cycles);

  for (i = 0; i < NPAGES; i++) {
    uint64_t oid = BASE_OID + (i * EROS_OBJECTS_PER_FRAME);

    if (range_waitobjectkey(KR_RANGE, OT_DataPage, oid, KR_TMP) != RC_OK)
      kprintf(KR_OSTREAM, "Couldn't get page key for submap\n");
  }
  
  eros_SysTrace_reportCounter(KR_SYSTRACE, &st);
  eros_SysTrace_stopCounter(KR_SYSTRACE);

  st.cycles /= NPAGES;
  kprintf(KR_OSTREAM, "Done -- %d pages, each %u cycles.\n",
	  NPAGES, (uint32_t) st.cycles);

  kprintf(KR_OSTREAM, "Begin rescind pass tracing\n");

  eros_SysTrace_startCounter(KR_SYSTRACE, eros_SysTrace_mode_cycles);

  /* Now destroy all of those objects: */
  for (i = 0; i < NPAGES; i++) {
    uint64_t oid = BASE_OID + (i * EROS_OBJECTS_PER_FRAME);

    if (range_waitobjectkey(KR_RANGE, OT_DataPage, oid, KR_TMP) != RC_OK)
      kprintf(KR_OSTREAM, "Couldn't get page key for submap\n");

    range_rescind(KR_RANGE, KR_TMP);
  }

  eros_SysTrace_reportCounter(KR_SYSTRACE, &st);
  eros_SysTrace_stopCounter(KR_SYSTRACE);

  st.cycles /= NPAGES;
  kprintf(KR_OSTREAM, "Done -- %d pages, each %u cycles.\n",
	  NPAGES, (uint32_t) st.cycles);


  kprintf(KR_OSTREAM, "Begin warm page alloc tracing\n");

  eros_SysTrace_startCounter(KR_SYSTRACE, eros_SysTrace_mode_cycles);

  for (i = 0; i < NPAGES; i++) {
    uint64_t oid = BASE_OID + (i * EROS_OBJECTS_PER_FRAME);

    if (range_waitobjectkey(KR_RANGE, OT_DataPage, oid, KR_TMP) != RC_OK)
      kprintf(KR_OSTREAM, "Couldn't get page key for submap\n");
  }
  
  eros_SysTrace_reportCounter(KR_SYSTRACE, &st);
  eros_SysTrace_stopCounter(KR_SYSTRACE);

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

  eros_SysTrace_startCounter(KR_SYSTRACE, eros_SysTrace_mode_cycles);

  for (i = 0; i < NPAGES; i++) {
    int j;
    for (j = 0; j < DISK_NODES_PER_PAGE; j++) {

      uint64_t oid = BASE_OID + (i * EROS_OBJECTS_PER_FRAME) + j;

      if (range_waitobjectkey(KR_RANGE, OT_Node, oid, KR_TMP) != RC_OK)
	kprintf(KR_OSTREAM, "Couldn't get page key for submap\n");
    }
  }
  
  eros_SysTrace_reportCounter(KR_SYSTRACE, &st);
  eros_SysTrace_stopCounter(KR_SYSTRACE);

  st.cycles /= (NPAGES*DISK_NODES_PER_PAGE);
  kprintf(KR_OSTREAM, "Done -- %d nodes (%d frames), each %u cycles.\n",
	  (NPAGES*DISK_NODES_PER_PAGE),
	  NPAGES, (uint32_t) st.cycles);


  return 0;
}
