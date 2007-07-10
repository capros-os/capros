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

#include <kerninc/kernel.h>
#include <kerninc/Key.h>
#include <kerninc/Activity.h>
#include <kerninc/Invocation.h>
#include <eros/Invoke.h>
#include <eros/StdKeyType.h>

#include <idl/capros/key.h>
#include <idl/capros/Checkpoint.h>

/* There is a curious consequence of the way EROS handles checkpoints
 * that I don't see how to elegantly avoid.  The call to
 * TakeCheckpoint() must be able to yield (which it will do if
 * migration has not completed), so it must be done here.
 * Unfortunately, the effect of this is that the PC snapshot for the
 * migration process itself is the PC of the call to takecheckpoint.
 * The consequences of system failure are a bit bewildering:
 * 
 * If migration succeeds before failing, this will result in a
 *   redundant call to TakeCheckpoint() on restart, since the migrator
 *   PC still points to the TakeCheckpoint() call.  At restart time, a
 *   marginal checkpoint isn't exactly that expensive, so who really
 *   cares.
 * 
 * If migration is interrupted by failure, what happens on restart
 *   depends on whether the migration manages to complete before the
 *   system restarts this thread.  If this thread starts up before
 *   migration completion, then the marginal call to TakeCheckpoint()
 *   will be ignored.  If it starts up after migration completes, we
 *   will take an extra checkpoint.
 * 
 * There are three possible ways to address this:
 * 
 *   1. Ignore it -- it's hardly critical.
 *   
 *   2. Ensure that migration has completed, hand-advance the PC
 *      before calling TakeCheckpoint(), and then hand-rollback the
 *      PC.  This is frought with opportunities for race conditions,
 *      and has the effect that TakeCheckpoint appears to never
 *      return.
 * 
 *   3. Have the TakeCheckpoint call accept as an argument the index
 *      of the checkpoint to be taken, so that it can ignore the call
 *      if the last checkpoint count is <= that index.  This is
 *      probably the cleanest solution, and what I will eventually do.
 * 
 */

void
CheckpointKey(Invocation* inv /*@ not null @*/)
{
  switch (inv->entry.code) {
  case OC_capros_Checkpoint_migrate:
    {
#ifdef OPTION_PERSISTENT
#error "Implement ProcessMigration"
      if (Checkpoint::ProcessMigration())
	inv.exit.w1 = true;
      else
	inv.exit.w1 = false;

      inv.exit.code = RC_OK;
#else
      inv->exit.code = RC_capros_key_NotPersistent;
#endif

      break;
    }
  case OC_capros_Checkpoint_snapshot:
    {
#ifdef OPTION_PERSISTENT
#error "Implement TakeCheckpoint"
      /* While it is not possible for this operation to fail, it may
       * well yield several times pursuing migration before it
       * succeeds:
       */
      
      if (Checkpoint::IsStable())
	Checkpoint::TakeCheckpoint();

      inv.exit.code = RC_OK;
#else
      inv->exit.code = RC_capros_key_NotPersistent;
#endif
      break;
    }
  case OC_capros_key_getType:
    inv->exit.code = RC_OK;
    inv->exit.w1 = AKT_Checkpoint;

    return;
  default:
    inv->exit.code = RC_capros_key_UnknownRequest;
    break;
  }

  COMMIT_POINT();
  
  return;
}
