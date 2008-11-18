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

/* Test the Real Time Clock.
*/

#include <time.h>
#include <eros/target.h>
#include <eros/Invoke.h>
#include <idl/capros/RTC.h>
#include <idl/capros/RTCSet.h>
#include <idl/capros/Sleep.h>

#include <domain/Runtime.h>
#include <domain/domdbg.h>
#include <domain/assert.h>

#define KR_OSTREAM	KR_APP(0)
#define KR_SLEEP        KR_APP(1)
#define KR_RTC		KR_APP(2)
#define KR_RTCSET	KR_APP(3)

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
  capros_RTC_time_t time;

  result = capros_RTC_getTime(KR_RTC, &time);
  ckOK
  pd(time);

  result = capros_Sleep_sleep(KR_SLEEP, 2000);
  ckOK

  result = capros_RTC_getTime(KR_RTC, &time);
  ckOK
  pd(time);

  time = 1234;
  result = capros_RTCSet_setTime(KR_RTCSET, time);
  assert(result == RC_capros_key_RequestError);

  result = capros_RTC_getTime(KR_RTC, &time);
  ckOK
  pd(time);

  struct tm t = {
    .tm_year = 2008 - 1900,
    .tm_mon = 11 - 1,
    .tm_mday = 17,
    .tm_hour = 15 + 8,
    .tm_min = 10,
    .tm_sec = 0,
    .tm_isdst = 0,
  };
  time = mktime(&t);
  unsigned long jd = time / (24*60*60);
  kdprintf(KR_OSTREAM, "Setting time to %u jday=%d (c to continue):", time, jd);
  result = capros_RTCSet_setTime(KR_RTCSET, time);
  ckOK

  result = capros_RTC_getTime(KR_RTC, &time);
  ckOK
  pd(time);

  result = capros_Sleep_sleep(KR_SLEEP, 2000);
  ckOK

  result = capros_RTC_getTime(KR_RTC, &time);
  ckOK
  pd(time);

  kprintf(KR_OSTREAM, "Done.\n");

  return 0;
}
