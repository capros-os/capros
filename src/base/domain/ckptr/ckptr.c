/*
 * Copyright (C) 2008, Strawberry Development Group.
 *
 * This file is part of the CapROS Operating System.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */
/* This material is based upon work supported by the US Defense Advanced
Research Projects Agency under Contract No. W31P4Q-07-C-0070.
Approved for public release, distribution unlimited. */

/* Simple program to take a checkpoint periodically. */

#include <string.h>
#include <eros/Invoke.h>
#include <idl/capros/Sleep.h>
#include <idl/capros/Checkpoint.h>
#include <domain/Runtime.h>
#include <domain/assert.h>

/* Bypass all the usual initialization. */
unsigned long __rt_runtime_hook = 0;

// The interval between checkpoints in seconds:
#define CheckpointInterval (4*60)

#define dbg_server 0x1

/* Following should be an OR of some of the above */
#define dbg_flags   ( 0x0 )

#define DEBUG(x) if (dbg_##x & dbg_flags)

#define KR_OSTREAM    KR_APP(0)
#define KR_SLEEP      KR_APP(1)
#define KR_Checkpoint KR_APP(2)

NORETURN int
main(void)
{
  result_t result;
  capros_Sleep_nanoseconds_t timeToSave;

  DEBUG(server) kprintf(KR_OSTREAM, "Taking first checkpoint.\n"
    "(NOTE: it takes TWO checkpoints to overwrite any old ckpt hdrs on the disk.)\n");

  result = capros_Sleep_getPersistentMonotonicTime(KR_SLEEP, &timeToSave);
  assert(result == RC_OK);

  result = capros_Checkpoint_ensureCheckpoint(KR_Checkpoint, timeToSave);
  assert(result == RC_OK);

  for (;;) {
    DEBUG(server) kprintf(KR_OSTREAM, "Taking checkpoint.\n");

    result = capros_Sleep_getPersistentMonotonicTime(KR_SLEEP, &timeToSave);
    assert(result == RC_OK);

    result = capros_Checkpoint_ensureCheckpoint(KR_Checkpoint, timeToSave);
    assert(result == RC_OK);

    DEBUG(server) kprintf(KR_OSTREAM, "Checkpoint taken.\n");

    result = capros_Sleep_sleep(KR_SLEEP, CheckpointInterval*1000);
    assert(result == RC_OK);
  }
}
