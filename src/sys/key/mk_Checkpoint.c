/*
 * Copyright (C) 1998, 1999, Jonathan S. Shapiro.
 * Copyright (C) 2007, 2008, Strawberry Development Group.
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

#include <kerninc/kernel.h>
#include <kerninc/Invocation.h>
#include <kerninc/SysTimer.h>
#include <kerninc/Ckpt.h>
#include <kerninc/IORQ.h>

#include <idl/capros/key.h>
#include <idl/capros/Checkpoint.h>

void
CheckpointKey(Invocation * inv)
{
  inv_GetReturnee(inv);

  switch (inv->entry.code) {
  case OC_capros_Checkpoint_ensureCheckpoint:
  {
    uint64_t timeToSave = (((uint64_t) inv->entry.w2) << 32)
                          | ((uint64_t) inv->entry.w1);
    uint64_t timeNow = sysT_NowPersistent();

    if (timeToSave > timeNow) {
      COMMIT_POINT();

      inv->exit.code = RC_capros_Checkpoint_FutureTime;
      break;
    }

    inv->exit.code = RC_OK;

    if (monotonicTimeOfLastDemarc >= timeToSave) {
      // There is a recent-enough checkpoint.
      COMMIT_POINT();

      break;
    }

    if (ckptIsActive()) {
      SleepOnPFHQueue(&WaitForCkptInactive);
    }

    COMMIT_POINT();

    DeclareDemarcationEvent();

    break;
  }

  case OC_capros_key_getType:
    inv->exit.code = RC_OK;
    inv->exit.w1 = IKT_capros_Checkpoint;
    COMMIT_POINT();
  
    break;

  default:
    inv->exit.code = RC_capros_key_UnknownRequest;
    COMMIT_POINT();
  
    break;
  }

  ReturnMessage(inv);
}
