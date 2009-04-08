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

#include <kerninc/kernel.h>
#include <kerninc/util.h>
#include <kerninc/Invocation.h>
#include <kerninc/KernStats.h>
#include <idl/capros/SysTrace.h>

//#define RESPONSE_TEST
#ifdef RESPONSE_TEST

#include <kerninc/SysTimer.h>
#include <kerninc/IRQ.h>

#define numSamples 20
Process * waitProc;
uint64_t startTime;
uint64_t durations[numSamples];
int durationsIndex = 0;

void
ClockWakeup(Process * proc)
{
  if (proc == waitProc) {
    // Mark start of duration.
    irqFlags_t flags = local_irq_save();
    startTime = mach_TicksToNanoseconds(sysT_latestTime);
    local_irq_restore(flags);
  }
}

#endif // RESPONSE_TEST

void
SysTraceCommon(Invocation * inv, Process * proc)
{
  COMMIT_POINT();

  switch(inv->entry.code) {
  case OC_capros_key_getType:
    inv->exit.code = RC_OK;
    inv->exit.w1 = AKT_SysTrace;
#ifdef RESPONSE_TEST
    // Mark end of duration.
    if (durationsIndex >= numSamples)
      inv->exit.code = (uint32_t)durations;
    else {
      durations[durationsIndex++] = mach_TicksToNanoseconds(sysT_Now())
                  - startTime;
    }
#endif // RESPONSE_TEST
    break;
    
  default:
    inv->exit.code = RC_capros_key_UnknownRequest;
    break;

  case OC_capros_SysTrace_CheckConsistency:
    {
#ifdef RESPONSE_TEST
      waitProc = proc_Current();
#endif // RESPONSE_TEST
      // Performance test of check.
      #include <kerninc/Check.h>
      check_Consistency("SysTrace");

      inv->exit.code = RC_OK;
      break;
    }

  case OC_capros_SysTrace_clearKernelStats:
    {
      kzero(&KernStats, sizeof(KernStats));
      inv->exit.code = RC_OK;
      break;
    }

  case OC_capros_SysTrace_setInvocationTrace:
#ifndef NDEBUG
    traceInvs = inv->entry.w1;
#endif
    inv->exit.code = RC_OK;
    break;
  }

  ReturnMessage(inv);
}
