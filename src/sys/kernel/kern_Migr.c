/*
 * Copyright (C) 2008, 2009, Strawberry Development Group.
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
#include <disk/DiskNode.h>
#include <kerninc/ObjectHeader.h>
#include <kerninc/ObjectCache.h>
#include <kerninc/ObjectSource.h>
#include <kerninc/Activity.h>
#include <kerninc/IORQ.h>
#include <kerninc/Ckpt.h>
#include <kerninc/LogDirectory.h>
#include <eros/Invoke.h>
#include <idl/capros/MigratorTool.h>
#include <idl/capros/SchedC.h>

#define dbg_migr	0x1

/* Following should be an OR of some of the above */
#define dbg_flags   ( 0u | dbg_migr )

#define DEBUG(x) if (dbg_##x & dbg_flags)

Activity * migratorActivity;
Activity * checkpointActivity;

unsigned int migrPhase;

// temporary stuff:
DEFQUEUE(migratorQueue);
void
DoMigrationStep(void)
{
  DEBUG(migr) printf("DoMigrStep %d\n", migrPhase);
  act_SleepOn(&migratorQueue);	// sleep forever (until implemented)
  act_Yield();	// act_Yield does not return.
}

#define StackSize 256

/* Most of the work of restart and migration is done in the kernel.
 * This thread just drives the execution.
 * This thread is started when both restart areas are mounted.
 */
/* Note, beware of accessing a page both from this process and from
 * the kernel. On some architectures (ARM), that can result in
 * an incoherent cache. */
void
MigratorStart(void)
{
  DEBUG(migr) printf("Start Migrator thread, act=%#x\n", migratorActivity);

  // FIXME: allocate all the space we will need up front.

  // The migrator thread begins by performing a restart.

  Message Msg = {
    .snd_invKey = KR_MigrTool,
    .snd_code = OC_capros_MigratorTool_restartStep,
    .snd_key0 = KR_VOID,
    .snd_key1 = KR_VOID,
    .snd_key2 = KR_VOID,
    .snd_rsmkey = KR_VOID,
    .rcv_limit = 0,
    .rcv_key0 = KR_VOID,
    .rcv_key1 = KR_VOID,
    .rcv_key2 = KR_VOID,
    .rcv_rsmkey = KR_VOID
  };
  CALL(&Msg);

  DEBUG(migr) printf("Finished restart.\n");

  // Start migration
  for (;;) {
    Msg.snd_code = OC_capros_MigratorTool_migrationStep;
    CALL(&Msg);
  }
}

static void
CreateKernelThread(const char * name, capros_SchedC_Priority prio,
  void (*func)(void), Activity * * ppAct)
{
  fixreg_t * stack = MALLOC(fixreg_t, StackSize);

  Activity * act = kact_InitKernActivity(name,
    dispatchQueues[prio], func,
    stack, &stack[StackSize]);

  // Endow it with the migrator tool.
  Key * k = & act_GetProcess(act)->keyReg[KR_MigrTool];
  keyBits_InitType(k, KKT_MigratorTool);

  *ppAct = act;
}

void
CreateMigratorActivity(void)
{
  CreateKernelThread("Migr", capros_SchedC_Priority_Normal, &MigratorStart,
                     &migratorActivity);

  printf("Created migrator process at %#x\n", act_GetProcess(migratorActivity));

  void CheckpointThread(void);
  CreateKernelThread("Ckpt", capros_SchedC_Priority_Max, &CheckpointThread,
                     &checkpointActivity);

  printf("Created checkpoint process at %#x\n", act_GetProcess(checkpointActivity));
}

