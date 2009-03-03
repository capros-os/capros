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

/* Test for DS2480B and DS2438.
*/

#include <stdint.h>
#include <string.h>
#include <eros/target.h>
#include <eros/Invoke.h>
#include <eros/fls.h>
#include <idl/capros/SpaceBank.h>
#include <idl/capros/GPT.h>
#include <idl/capros/SuperNode.h>
#include <idl/capros/Sleep.h>
#include <idl/capros/DevPrivs.h>
#include <idl/capros/W1Bus.h>
#include <idl/capros/DS18B20.h>
#include <idl/capros/DS2450.h>
#include <idl/capros/DS2438.h>

#include <domain/Runtime.h>
#include <domain/domdbg.h>
#include <domain/assert.h>

#define KR_OSTREAM  KR_APP(1)
#define KR_SLEEP    KR_APP(2)
#define KR_DEVPRIVS KR_APP(3)
#define KR_DEVNODE  KR_APP(4)

const uint32_t __rt_stack_pointer = 0x20000;
const uint32_t __rt_unkept = 1;

#define ckOK \
  if (result != RC_OK) { \
    kdprintf(KR_OSTREAM, "Line %d result is 0x%08x!\n", __LINE__, result); \
  }

void
GetDevN(int n, cap_t cap)
{
  result_t result;
  result = capros_Node_getSlotExtended(KR_DEVNODE, n, cap);
  ckOK
}

void
configureTemp(int n)
{
  result_t result;
  GetDevN(n, KR_TEMP0);
  // Sample every 2 seconds, 3 binary bits of resolution.
  result = capros_DS18B20_configure(KR_TEMP0, 1, 3);
  ckOK
}

void
configureAD(int n)
{
  result_t result;
  GetDevN(n, KR_TEMP0);
#define init_2450_port { \
    .output = 0, \
    .rangeOrOutput = 1, \
    .log2Seconds = 1,	/* 2 seconds */ \
    .bitsToConvert = 8 \
  }
  capros_DS2450_portsConfiguration config = {
    .port = {
      [0] = init_2450_port,
      [1] = init_2450_port,
      [2] = init_2450_port,
      [3] = init_2450_port
    }
  };
  result = capros_DS2450_configurePorts(KR_TEMP0, config);
  ckOK
}

void
configureBM(int n, bool vdd)
{
  result_t result;
  GetDevN(n, KR_TEMP0);
  result = capros_DS2438_configureTemperature(KR_TEMP0, 3);	// every 8 sec
  ckOK
  result = capros_DS2438_configureVoltage(KR_TEMP0, vdd, 0);	// every 1 sec
  ckOK
  result = capros_DS2438_configureCurrent(KR_TEMP0,
             capros_DS2438_CurrentConfig_AccumNoEE);
  ckOK
}

void
PrintTempDevN(int n)
{
  result_t result;
  short temperature;
  capros_RTC_time_t time;

  GetDevN(n, KR_TEMP0);
  result = capros_DS18B20_getTemperature(KR_TEMP0,
               &temperature, &time);
  ckOK
  if (time) {
    kprintf(KR_OSTREAM, "Dev %d temperature is %d.%d Celsius at %#lx\n",
            n, temperature/16, (temperature%16) >> 1, time);
  }
}

void
PrintAD(int n)
{
  result_t result;
  capros_DS2450_portsData data;
  capros_RTC_time_t time;

  GetDevN(n, KR_TEMP0);
  result = capros_DS2450_getData(KR_TEMP0, &data, &time);
  ckOK
  if (time) {
    kprintf(KR_OSTREAM, "Dev %d data is %#.4x %#.4x %#.4x %#.4x at %#lx\n",
            n, data.data[0], data.data[1], data.data[2], data.data[3], time);
  }
}

void
PrintBM(int n)
{
  result_t result;
  capros_RTC_time_t time;
  uint16_t datau16;

  GetDevN(n, KR_TEMP0);

  result = capros_DS2438_getVoltage(KR_TEMP0, &datau16, &time);
  ckOK
  if (time) {
    unsigned int v = datau16 * 674 / 1000;	// adj v, units 0.1V
    kprintf(KR_OSTREAM, "Dev %d is %u mV or %u.%u V at %#lu sec\n",
            n, datau16 * 10, v/10, v%10, time);
  }

#if 0
  int16_t data16;
  result = capros_DS2438_getTemperature(KR_TEMP0, &data16, &time);
  ckOK
  if (time) {
    kprintf(KR_OSTREAM, "Dev %d temp is %d.%d at %#lu sec\n",
            n, data16 >> 8, (data16 & 0xff)/26, time);
  }

  result = capros_DS2438_getCurrent(KR_TEMP0, &data16);
  if (result != RC_capros_DS2438_Offline) {
    ckOK
    kprintf(KR_OSTREAM, "current is %d\n",
            data16);
  }
#endif
}

int
main(void)
{
  result_t result;

  kprintf(KR_OSTREAM, "Starting.\n");

  configureTemp(1);
  configureBM(2, true);
#define all
#ifdef all
  configureBM(5, true);
#endif

  // Give it a chance to get started.
  result = capros_Sleep_sleep(KR_SLEEP, 5000);	// sleep 5 seconds
  assert(result == RC_OK || result == RC_capros_key_Restart);

  for (;;) {
#ifdef all
    configureBM(3, true);
    configureBM(4, true);
#endif
//    PrintTempDevN(1);
    PrintBM(2);
#ifdef all
    PrintBM(3);
    PrintBM(4);
    PrintBM(5);
#endif

    result = capros_Sleep_sleep(KR_SLEEP, 3000);	// sleep 3 seconds
    assert(result == RC_OK || result == RC_capros_key_Restart);
  }

  kprintf(KR_OSTREAM, "Done.\n");

  return 0;
}

