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

#include <string.h>
#include <kerninc/kernel.h>
#include <kerninc/Activity.h>
#include <kerninc/Invocation.h>
#include <kerninc/Ckpt.h>

#include <idl/capros/key.h>
#include <idl/capros/MigratorTool.h>

#define dbg_migr	0x1

/* Following should be an OR of some of the above */
#define dbg_flags   ( 0u | dbg_migr )

#define DEBUG(x) if (dbg_##x & dbg_flags)

/* May Yield. */
void
MigratorToolKey(Invocation* inv)
{
  inv_GetReturnee(inv);

  inv->exit.code = RC_OK;	// Until proven otherwise

  switch (inv->entry.code) {
  case OC_capros_key_getType:
    COMMIT_POINT();

    inv->exit.code = RC_OK;
    inv->exit.w1 = IKT_capros_MigratorTool;
    break;

  default:
    COMMIT_POINT();

    inv->exit.code = RC_capros_key_UnknownRequest;
    break;

  case OC_capros_MigratorTool_restartStep: ;
    void DoRestartStep(void);
    DoRestartStep();
    COMMIT_POINT();
    break;

  case OC_capros_MigratorTool_checkpointStep:
    DoCheckpointStep();
    COMMIT_POINT();
    break;

  case OC_capros_MigratorTool_migrationStep: ;
    void DoMigrationStep(void);
    DoMigrationStep();
    COMMIT_POINT();
    break;

  case OC_capros_MigratorTool_waitForRestart:
    if (! restartIsDone()) {
      DEBUG(migr)
        printf("MigrTool_waitForRestart waiting for restart to complete.\n");
      act_SleepOn(&RestartQueue);
      act_Yield();
    }
    COMMIT_POINT();
    break;
  }
  ReturnMessage(inv);
}
