/*
 * Copyright (C) 2010, Strawberry Development Group.
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

/* This program obtains the time from a server using RFC 868,
   and synchronizes the local RTC. */

#include <string.h>
#include <eros/target.h>
#include <eros/Invoke.h>
#include <idl/capros/Number.h>
#include <idl/capros/IP.h>
#include <idl/capros/RTCSet.h>

#include <domain/Runtime.h>
#include <domain/domdbg.h>
#include <domain/assert.h>

#define KR_OSTREAM  KR_APP(0)
#define KR_RTCSet   KR_APP(1)
#define KR_IP       KR_APP(2)
#define KR_IPConfig KR_APP(3)
#define KR_UDPPort  KR_APP(4)

/* Bypass all the usual initialization. */
unsigned long __rt_runtime_hook = 0;

#define dbg_errors 0x1
#define dbg_set    0x2

/* Following should be an OR of some of the above */
#define dbg_flags   ( 0u | dbg_errors)

#define DEBUG(x) if (dbg_##x & dbg_flags)

capros_RTC_time_t durationToRecheck = 24*60*60;	// initially, 1 day

capros_RTC_time_t currentRTC;
void
GetRTC(void)
{
  result_t result;
  result = capros_RTC_getTime(KR_RTCSet, &currentRTC);
  assert(result == RC_OK);
}

int
main(void)
{
  result_t result;

  // Unpack configuration data.
  uint32_t ipaddr, portnum, unused;
  result = capros_Number_get(KR_IPConfig, &ipaddr, &portnum, &unused);
  assert(result == RC_OK);

#if 0
  // Work around a bug in lwip. If the first contact with a host is
  // a UDP transmission, etharp_query crashes saying
  // "no packet queues allowed!"
  result = capros_IP_TCPConnect(KR_IP, ipaddr, portnum, KR_TEMP0);
  result = capros_TCPSocket_close(KR_TEMP0);
#endif

  for (;;) {
    // Get the time from the time server.
    // Create a new local port each time, to make sure the data we read
    // isn't stale.
    result = capros_IP_createUDPPort(KR_IP, KR_UDPPort);
    if (result != RC_OK) {
      DEBUG(errors)
        kprintf(KR_OSTREAM, "timeClient couldn't create UDP port.\n");
      GetRTC();
      goto loop1;
    }

    result = capros_UDPPort_send(KR_UDPPort, ipaddr, portnum,
                                 0, NULL);
    if (result != RC_OK) {
      DEBUG(errors)
        kprintf(KR_OSTREAM, "timeClient couldn't send request.\n");
      GetRTC();
      goto loop;
    }

    GetRTC();

    uint32_t sourceIPAddr;
    uint16_t sourceIPPort;
    uint32_t lenRecvd;
    uint8_t r[4];
    result = capros_UDPPort_receive(KR_UDPPort, 4,
               &sourceIPAddr, &sourceIPPort, &lenRecvd, &r[0]);
    if (result != RC_OK) {
      DEBUG(errors)
        kprintf(KR_OSTREAM, "timeClient couldn't read response.\n");
      goto loop;
    }

    capros_RTC_time_t netTime = r[0];
    netTime = (netTime << 8) + r[1];
    netTime = (netTime << 8) + r[2];
    netTime = (netTime << 8) + r[3];
    // Convert from seconds since 1/1/1900 to seconds since 1/1/1970.
    // Note 1900 was not a leap year but 1968 was.
    netTime -= (70UL*365 + 70/4)*24*60*60;

    int32_t diff = netTime - currentRTC;
    int32_t absDiff = diff > 0 ? diff : -diff;

    if (absDiff > 24*60*60) {	// more than one day off
      kprintf(KR_OSTREAM, "RTC off by more than one day. Set it manually.\n");
      goto loop;
    }

    int change = diff;
    if (absDiff == 0) {
      durationToRecheck += durationToRecheck >> 1;	// wait longer
    } else if (absDiff == 1) {
      // no change in durationToRecheck
    } else {
      // To complete syncing in one hour, wait this long:
      int dur = 60*60 / (absDiff-1);
      // But don't change the clock speed by more than 1%:
      if (dur < 100)
        dur = 100;
      if (durationToRecheck > dur)
        durationToRecheck = dur;

      /* Always change by just one second at a time to avoid sudden large
         jumps in time. */
      change = diff > 0 ? 1 : -1;
    }
    if (change != 0) {
      /* Note, if we are in the middle of a second this may change
         less than one second. */
      result = capros_RTCSet_addTime(KR_RTCSet, change);
      DEBUG(set) kprintf(KR_OSTREAM, "%sed RTC.\n",
                   change > 0 ? "Increment" : "Decrement");
    }
    DEBUG(set)
      kprintf(KR_OSTREAM, "Local time %u, net time %u, diff %d, dtr %d\n",
              currentRTC, netTime, diff, durationToRecheck);
    
  loop: ;
    capros_key_destroy(KR_UDPPort);
  loop1: ;
    // Wait before checking again.
    result = capros_RTC_sleepTillTime(KR_RTCSet,
               currentRTC + durationToRecheck);
    assert(result == RC_OK);
  }
}
