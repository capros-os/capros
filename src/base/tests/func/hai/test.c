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

/* HAI test.
*/

#include <string.h>
#include <time.h>
#include <eros/target.h>
#include <eros/Invoke.h>
#include <idl/capros/Sleep.h>
#include <idl/capros/HAI.h>

#include <domain/Runtime.h>
#include <domain/domdbg.h>
#include <domain/assert.h>

#define KR_OSTREAM KR_APP(1)
#define KR_SLEEP   KR_APP(2)
#define KR_HAI     KR_APP(3)

const uint32_t __rt_stack_pointer = 0x20000;
const uint32_t __rt_unkept = 1;

#define ckOK \
  if (result != RC_OK) { \
    kdprintf(KR_OSTREAM, "Line %d result is 0x%08x!\n", __LINE__, result); \
  }

void
pd(capros_RTC_time_t time)
{
  unsigned long jd = time / (24*60*60);
  kprintf(KR_OSTREAM, "RTC is %u jday=%u ",
    time, jd);

  struct tm * ts = gmtime((long *)&time);
  kprintf(KR_OSTREAM, "%d/%d/%d %u:%.2u:%.2u\n",
          ts->tm_mon+1, ts->tm_mday, ts->tm_year+1900,
          ts->tm_hour, ts->tm_min, ts->tm_sec);
}

int
main(void)
{
  result_t result;
  capros_key_type theType;
  capros_RTC_time_t t;

  kprintf(KR_OSTREAM, "Starting.\n");

  result = capros_key_getType(KR_HAI, &theType);
  ckOK
  assert(theType == IKT_capros_HAI);

  capros_HAI_SystemStatus ss;
  result = capros_HAI_getSystemStatus(KR_HAI, &t, &ss);
  ckOK
  pd(t);
  kprintf(KR_OSTREAM, "%d %.2d-%d-%d %d %d:%.2d:%.2d %d %d:%.2d %d:%.2d %d\n",
          ss.timeDateValid, ss.year, ss.month, ss.day, ss.dayOfWeek,
          ss.hour, ss.minute, ss.second, ss.DST,
          ss.sunriseHour, ss.sunriseMinute,
          ss.sunsetHour, ss.sunsetMinute,
          ss.batteryReading);

  result = capros_Sleep_sleep(KR_SLEEP, 60*1000);	// wait 1 minute
  // a checkpoint will be taken here.
  assert(result == RC_OK || result == RC_capros_key_Restart);

  result = capros_HAI_setUnitStatus(KR_HAI, 261, capros_HAI_Command_UnitOffForSeconds, 8);
  ckOK

  uint8_t cond;
  uint16_t tl;
  result = capros_HAI_getUnitStatus(KR_HAI, 261, &t, &cond, &tl);
  ckOK
  pd(t);
  kprintf(KR_OSTREAM, "unit condition %d, time left %d seconds\n", cond, tl);

  kprintf(KR_OSTREAM, "\nDone.\n");

  return 0;
}

