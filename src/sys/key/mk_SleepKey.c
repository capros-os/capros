/*
 * Copyright (C) 1998, 1999, Jonathan S. Shapiro.
 * Copyright (C) 2006, 2007, 2008, 2009, Strawberry Development Group.
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

#include <kerninc/kernel.h>
#include <kerninc/Key.h>
#include <kerninc/Process.h>
#include <arch-kerninc/Process-inline.h>
#include <kerninc/Invocation.h>
#include <kerninc/Activity.h>
#include <kerninc/Machine.h>
#include <kerninc/SysTimer.h>
#include <kerninc/IRQ.h>
#include <kerninc/Ckpt.h>
#include <eros/Invoke.h>
#include <eros/StdKeyType.h>

#include <idl/capros/key.h>
#include <idl/capros/Sleep.h>

void
SleepInvokee(Process * invokee, uint64_t wakeupTime)
{
  assert(link_isSingleton(& act_Current()->q_link));

  if (invokee != proc_curProcess) {
    Activity * act = allocatedActivity;
    if (act->state == act_Running) {
      // allocatedActivity came from the invoker.
      assert(act == act_Current());
      assert(act_HasProcess(act));
      proc_Deactivate(act_GetProcess(act));
    } else {
      // allocatedActivity is newly allocated.
      assert(act->state == act_Free);
    }
    act_AssignToRunnable(act, invokee);
  }

  act_SleepUntilTick(invokee->curActivity, wakeupTime);
  act_ForceResched();
}

/* May Yield. */
void
SleepKey(Invocation* inv /*@ not null @*/)
{
  uint64_t u64;
  uint64_t wakeupTime;

  inv_GetReturnee(inv);

  switch (inv->entry.code) {
  case OC_capros_Sleep_getTimeMonotonic:
    u64 = sysT_NowUniqueNS();

returnu64:
    COMMIT_POINT();
      
    inv->exit.w1 = (uint32_t) u64;	// low word
    inv->exit.w2 = u64 >> 32;
    inv->exit.code = RC_OK;
    break;
      
  case OC_capros_Sleep_getPersistentMonotonicTime:
    u64 = sysT_NowPersistent();
    goto returnu64;

  case OC_capros_Sleep_sleep:
    u64 = (((uint64_t) inv->entry.w2) << 32)
                  | ((uint64_t) inv->entry.w1);

    wakeupTime = sysT_Now() + mach_MillisecondsToTicks(u64);
    goto sleepCommon;
      
  case OC_capros_Sleep_sleepForNanoseconds:
    u64 = (((uint64_t) inv->entry.w2) << 32)
                  | ((uint64_t) inv->entry.w1);

    wakeupTime = sysT_Now() + mach_NanosecondsToTicks(u64);
    goto sleepCommon;

  case OC_capros_Sleep_sleepTillPersistentOrRestart:
    WaitForRestartDone();	// wait until monotonicTimeOfRestart is valid
    u64 = (((uint64_t) inv->entry.w2) << 32)
                  | ((uint64_t) inv->entry.w1);
    // The following test ensures that u64 - monotonicTimeOfRestart
    // won't be negative.
    if (u64 < monotonicTimeOfRestart)
      goto returnOK;

    u64 = u64 - monotonicTimeOfRestart;
	// non-persistent wakeup time in ns

    goto sleepCommonNs;

  case OC_capros_Sleep_sleepTill:
    u64 = (((uint64_t) inv->entry.w2) << 32)
                  | ((uint64_t) inv->entry.w1);

  sleepCommonNs:
    wakeupTime = mach_NanosecondsToTicks(u64) + 1;	// +1 to round up
    goto sleepCommon;

  sleepCommon:
    {
#if 0
      uint64_t nw = sysT_Now();
      printf("sleep till %#llx now=%#llx diff=%lld\n",
             wakeupTime, nw, wakeupTime - nw);
#endif

      COMMIT_POINT();

      /* We have now taken care of the invoker.
         It is the invokee who will be awakened later. */

      Process * invokee = inv->invokee;

      if (! invokee)
        break;	// no one to wake up at the end of the wait

      SleepInvokee(invokee, wakeupTime);

      return;	// don't call ReturnMessage
    }
      
  case OC_capros_Sleep_getDelayCalibration:
    COMMIT_POINT();
      
    inv->exit.code = RC_OK;
    inv->exit.w1 = loopsPer8us;
    break;
    
  case OC_capros_key_getType:
    inv->exit.w1 = AKT_Sleep;

  returnOK:
    COMMIT_POINT();
      
    inv->exit.code = RC_OK;
    break;

  default:
    COMMIT_POINT();
      
    inv->exit.code = RC_capros_key_UnknownRequest;
    break;
  }
  ReturnMessage(inv);
}
