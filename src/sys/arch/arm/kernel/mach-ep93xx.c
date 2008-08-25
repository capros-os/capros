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

/* This file is #included by board-specific RTC handlers. */

#include <kerninc/kernel.h>
#include <kerninc/Machine.h>
#include <kerninc/mach-rtc.h>
#include "ep93xx-rtc.h"

#define dbg_rtc	0x1

/* Following should be an OR of some of the above */
#define dbg_flags   ( 0u )

#define DEBUG(x) if (dbg_##x & dbg_flags)

#define RTC (RTCStruct(APB_VA + RTC_APB_OFS))

static uint32_t minTime = 0;

capros_RTC_time_t
RtcRead(void)
{
  // See RtcBootSet below for the reason behind this logic.
  uint32_t t = RTC.Data;
  DEBUG(rtc) printf("RTC.Data = %u\n", t);
  if (t < minTime)
    return minTime;
  else return t;
}

int
RtcSet(capros_RTC_time_t newTime)
{
  // Write to nonvolatile media first:
  int ret = RtcSave(newTime);
  if (ret) {
    assert(ret < 0);
    return ret;
  }

  DEBUG(rtc) printf("RTC.Load := %u\n", newTime);
  RTC.Load = newTime + 1;
  minTime = newTime + 1;

  // Caller must wait 2 seconds for the new time to appear in RTC.Data:
  return 2;
}

// Set the CPU's RTC at boot time.
void
RtcBootSet(uint32_t newTime)
{
  DEBUG(rtc) printf("Boot RTC.Load := %u\n", newTime);

  // The time will be loaded on the next 1Hz tick, so get the
  // time that we want loaded then:
  newTime++;

  uint32_t oldTime = RTC.Data;
  RTC.Load = newTime;
  /* This takes up to 2 seconds to propagate to RTC.Data.
  The boot process may not take that long. */
  // RTC.Data should never be less than newTime, because it may be invalid.
  minTime = newTime;
  if (newTime >= oldTime) {
    // This is the common case, since RTC.Data is cleared on power-on reset.
    // minTime will be observed.
  } else {
    // Bite the bullet and delay 2 seconds.
    printf("Delaying for RTC to settle.\n");
    int i;
    for (i = 80; i-- > 0; )
      SpinWaitUs(25000);
  }
}
