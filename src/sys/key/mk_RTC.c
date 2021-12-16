/*
 * Copyright (C) 2008-2010, Strawberry Development Group.
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
#include <kerninc/Invocation.h>
#include <kerninc/mach-rtc.h>
#include <kerninc/SysTimer.h>
#include <kerninc/Ckpt.h>
#include <eros/Invoke.h>
#include <idl/capros/key.h>
#include <idl/capros/RTC.h>
#include <idl/capros/RTCSet.h>

#define dbg_get  0x1
#define dbg_set  0x2

/* Following should be an OR of some of the above */
#define dbg_flags   ( 0u )

#define DEBUG(x) if (dbg_##x & dbg_flags)

void
RTCKey(Invocation * inv)
{
  inv_GetReturnee(inv);

  inv->exit.code = RC_OK;	// default

  switch (inv->entry.code) {
  case OC_capros_RTC_getTime:

    COMMIT_POINT();

    inv->exit.w1 = RtcRead();

    DEBUG(get) printf("RTC_getTime %u\n", inv->exit.w1);
    break;

  case OC_capros_RTC_sleepTillTimeOrRestart:
  {
    uint32_t wakeupTimeRTC = inv->entry.w1;
    uint32_t nowRTC = RtcRead();
    // Compare, rather than subtract, to avoid overflow.
    if (wakeupTimeRTC <= nowRTC) {
      COMMIT_POINT();
      break;
    }
    uint32_t duration = wakeupTimeRTC - nowRTC;
    uint64_t wakeupTime = sysT_Now()
                   + mach_NanosecondsToTicks(duration * 1000000000ULL);
    // Compare with sleepCommon in mk_SleepKey.c.

    COMMIT_POINT();
    Process * invokee = inv->invokee;
    if (! invokee)
      break;	// no one to wake up at the end of the wait
    /* FIXME: If the RTC is adjusted, we should wake up this guy. */
    SleepInvokee(invokee, wakeupTime);
    return;	// don't call ReturnMessage
  }

  case OC_capros_RTC_getRestartTimes:

    COMMIT_POINT();

    inv->exit.w1 = RTCOfRestartDemarc;
    inv->exit.w2 = RTCOfRestart;

    break;

  case OC_capros_RTCSet_addTime:
    DEBUG(set) printf("RTCSet_addTime %u\n", inv->entry.w1);

    COMMIT_POINT();

    uint32_t newTime = RtcRead() + inv->entry.w1;
    goto addSet;

  case OC_capros_RTCSet_setTime:
    DEBUG(set) printf("RTCSet_setTime %u\n", inv->entry.w1);

    COMMIT_POINT();

    newTime = inv->entry.w1;

  addSet: ;
    if (inv->key->u.nk.value[0] == 0) {
      inv->exit.code = RC_capros_key_UnknownRequest;
    }

    /* FIXME: If the RTC is adjusted, we should wake up procs
       waiting on the RTC. */
    int ret = RtcSet(newTime);
    if (ret < 0) {
      inv->exit.code = RC_capros_key_RequestError;
    }

    if (ret > 0) {
      // RtcSet needs time to take effect.
      // Delay the response until it has taken effect.

      Process * invokee = inv->invokee;
      if (! invokee)
        break;	// no one to wake up at the end of the wait

      SleepInvokee(invokee, sysT_Now() + mach_MillisecondsToTicks(ret * 1000));

      return;	// don't call ReturnMessage
    }
    break;

  case OC_capros_key_getType:

    COMMIT_POINT();

    if (inv->key->u.nk.value[0] == 0) {
      inv->exit.w1 = IKT_capros_RTC;
    } else {
      inv->exit.w1 = IKT_capros_RTCSet;
    }
    break;

  default:

    COMMIT_POINT();

    inv->exit.code = RC_capros_key_UnknownRequest;
    break;
  }
  ReturnMessage(inv);
}
