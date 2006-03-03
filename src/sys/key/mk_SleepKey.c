/*
 * Copyright (C) 1998, 1999, Jonathan S. Shapiro.
 * Copyright (C) 2006, Strawberry Development Group.
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
#include <kerninc/Process.h>
#include <arch-kerninc/Process-inline.h>
#include <kerninc/Invocation.h>
#include <kerninc/Activity.h>
#include <kerninc/Machine.h>
#include <kerninc/IRQ.h>
#include <eros/Invoke.h>
#include <eros/StdKeyType.h>

#include <idl/eros/key.h>
#include <idl/eros/Sleep.h>

void
SleepKey(Invocation* inv /*@ not null @*/)
{
  COMMIT_POINT();
      
  switch (inv->entry.code) {
  case OC_eros_Sleep_wakeup:
    {
      /* This is NOT a no-op.  The wakeup logic hacks wakeup by
	 resetting the order code to this one */
      break;
    }
  case OC_eros_Sleep_sleep:
  case OC_eros_Sleep_sleepTill:
    {
      uint64_t ms = 00l;

      ms = (((uint64_t) inv->entry.w2) << 32) | ((uint64_t) inv->entry.w1);

      /* FIX: call DeliverResult() here to update result regs! */
      
      irq_DISABLE();
      if (inv->entry.code == OC_eros_Sleep_sleep) {
	act_WakeUpIn(act_Current(), ms);
      }
      else {
        act_WakeUpAtTick(act_Current(), mach_MillisecondsToTicks(ms));
      }

      /* Invokee is resuming from either waiting or available state, so
	 advance their PC past the trap instruction.

	 If this was a kernel key invocation in the fast path, we never
	 bothered to actually set them waiting, but they were logically
	 in the waiting state nonetheless. */
      proc_SetPC(act_CurContext(), act_CurContext()->nextPC);

      /* The following is an ugly lie.  We have committed the
       * invocation, and therefore advanced the PC.  The sleep call,
       * however, does not return in the usual way, but rather yields
       * here.  To make sure that this does not trip up the various
       * consistency checks, we set InvocationCommitted to FALSE here.
       */

#ifndef NDEBUG
      InvocationCommitted = false;
#endif
      
      act_SleepOn(act_Current(), &DeepSleepQ);
      irq_ENABLE();

      /* Thread::Current() changed to act_Current() */
      act_Yield(act_Current());
      return;
    }
    
  case OC_eros_key_getType:
    inv->exit.code = RC_OK;
    inv->exit.w1 = AKT_Sleep;
    return;
  default:
    inv->exit.code = RC_eros_key_UnknownRequest;
    return;
  }
}
