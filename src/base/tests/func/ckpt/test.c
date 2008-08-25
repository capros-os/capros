/*
 * Copyright (C) 2008, Strawberry Development Group.
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
Research Projects Agency under Contract No. W31P4Q-07-C-0070.
Approved for public release, distribution unlimited. */

/* Test checkpointing.
*/

#include <stdint.h>
#include <string.h>
#include <eros/target.h>
#include <eros/Invoke.h>
#include <idl/capros/SpaceBank.h>
#include <idl/capros/Node.h>
#include <idl/capros/Checkpoint.h>
#include <idl/capros/Sleep.h>

#include <domain/Runtime.h>
#include <domain/domdbg.h>
#include <domain/assert.h>

#define KR_OSTREAM	KR_APP(0)
#define KR_CHECKPOINT	KR_APP(1)
#define KR_SLEEP        KR_APP(2)

#define KR_NODE         KR_APP(6)

const uint32_t __rt_stack_pointer = 0x20000;
const uint32_t __rt_unkept = 1;

#define ckOK \
  if (result != RC_OK) { \
    kdprintf(KR_OSTREAM, "Line %d result is 0x%08x!\n", __LINE__, result); \
  }

int
main(void)
{
  result_t result;
  int i;
  capros_Sleep_nanoseconds_t timeToSave;

  kprintf(KR_OSTREAM, "About to test checkpointing future time.\n");

  timeToSave = 1ULL << 62;	// a very large time
  result = capros_Checkpoint_ensureCheckpoint(KR_CHECKPOINT, timeToSave);
  assert(result == RC_capros_Checkpoint_FutureTime);

  // Allocate a node for later use.
  result = capros_SpaceBank_alloc1(KR_BANK, capros_Range_otNode, KR_NODE);
  ckOK

  for (i = 0; i < 3; i++) {
    result = capros_Sleep_sleep(KR_SLEEP, 5*1000);	// sleep 5 seconds
    ckOK

    result = capros_Sleep_getPersistentMonotonicTime(KR_SLEEP, &timeToSave);
    ckOK

    kprintf(KR_OSTREAM, "About to checkpoint, time=%llu ms\n",
            timeToSave/1000000);

    result = capros_Checkpoint_ensureCheckpoint(KR_CHECKPOINT, timeToSave);
    ckOK

    kprintf(KR_OSTREAM, "%d checkpoints done.\n", i+1);

    // Dirty the node.
    result = capros_Node_swapSlot(KR_NODE, 0, KR_BANK, KR_VOID);
    ckOK
  }

  kprintf(KR_OSTREAM, "Done.\n");

  return 0;
}

