/*
 * Copyright (C) 1998, 1999, Jonathan S. Shapiro.
 * Copyright (C) 2006, 2007, 2008, Strawberry Development Group.
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
#include <kerninc/Key.h>
#include <kerninc/Process.h>
#include <arch-kerninc/Process-inline.h>
#include <kerninc/Invocation.h>
#include <kerninc/Activity.h>
#include <kerninc/Machine.h>
#include <kerninc/SysTimer.h>
#include <kerninc/IRQ.h>
#include <eros/Invoke.h>
#include <eros/StdKeyType.h>

#include <idl/capros/key.h>
#include <idl/capros/Sleep.h>

/* May Yield. */
void
SleepKey(Invocation* inv /*@ not null @*/)
{
  inv_GetReturnee(inv);

  switch (inv->entry.code) {
  case OC_capros_Sleep_getTimeMonotonic:
    COMMIT_POINT();
      
    {
      uint64_t nsec = mach_TicksToNanoseconds(sysT_Now());
      inv->exit.w1 = (uint32_t) nsec;	// low word
      inv->exit.w2 = nsec >> 32;
      inv->exit.code = RC_OK;
      return;
    }
      
  case OC_capros_Sleep_sleep:
  case OC_capros_Sleep_sleepTill:
    {
      uint64_t ms = (((uint64_t) inv->entry.w2) << 32)
                    | ((uint64_t) inv->entry.w1);

      /* FIX: call DeliverResult() here to update result regs! */

      uint64_t wakeupTime;
      if (inv->entry.code == OC_capros_Sleep_sleep)
        wakeupTime = sysT_Now() + mach_MillisecondsToTicks(ms);
      else
        wakeupTime = mach_NanosecondsToTicks(ms);

#if 0
      uint64_t nw=sysT_Now();
      printf("sleep till %#llx now=%#llx diff=%lld\n",
             wakeupTime, nw, wakeupTime - nw);
#endif
      
      irqFlags_t flags = local_irq_save();

      act_WakeUpAtTick(act_Current(), wakeupTime);

      /* Invokee is resuming from either waiting or available state, so
	 advance their PC past the trap instruction.

	 If this was a kernel key invocation in the fast path, we never
	 bothered to actually set them waiting, but they were logically
	 in the waiting state nonetheless. */
	// FIXME: all this code is completely wrong if invoker != invokee
	// and needs to be redone.
      proc_AdvancePostInvocationPC(act_CurContext());

      act_SleepOn(&DeepSleepQ);

      local_irq_restore(flags);

      act_Yield();
      return;
    }
      
  case OC_capros_Sleep_getDelayCalibration:
    COMMIT_POINT();
      
    inv->exit.code = RC_OK;
    inv->exit.w1 = loopsPer8us;
    return;
    
  case OC_capros_key_getType:
    COMMIT_POINT();
      
    inv->exit.code = RC_OK;
    inv->exit.w1 = AKT_Sleep;
    return;

  default:
    COMMIT_POINT();
      
    inv->exit.code = RC_capros_key_UnknownRequest;
    return;
  }
}
