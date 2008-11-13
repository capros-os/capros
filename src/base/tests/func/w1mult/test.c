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

/* USB test.
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
#include <idl/capros/W1Bus.h>
#include <idl/capros/DS18B20.h>
#include <idl/capros/DS2450.h>

#include <domain/Runtime.h>
#include <domain/domdbg.h>
#include <domain/assert.h>

#define KR_OSTREAM  KR_APP(1)
#define KR_SLEEP    KR_APP(2)
#define KR_DEVNODE  KR_APP(4)
#define KR_DS18B20_loose KR_APP(4)


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
configureDevN(int n)
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
PrintTempDevN(int n)
{
  result_t result;
  short temperature;
  uint64_t time;

  GetDevN(n, KR_TEMP0);
  result = capros_DS18B20_getTemperature(KR_TEMP0,
               &temperature, &time);
  ckOK
  if (time) {
    kprintf(KR_OSTREAM, "Dev %d temperature is %d.%d Celsius at %llu ms\n",
            n, temperature/16, (temperature%16) >> 1, time/1000000);
  }
}

void
PrintADDevN(int n)
{
  result_t result;
  capros_DS2450_portsData data;
  uint64_t time;

  GetDevN(n, KR_TEMP0);
  result = capros_DS2450_getData(KR_TEMP0, &data, &time);
  ckOK
  if (time) {
#if 0
    kprintf(KR_OSTREAM, "Dev %d data is %#.4x %#.4x %#.4x %#.4x at %llu ms\n",
            n, data.data[0], data.data[1], data.data[2], data.data[3],
            time/1000000);
#else
    kprintf(KR_OSTREAM, "Dev %d data is %u %u %u %u at %llu ms\n",
            n, data.data[0], data.data[1], data.data[2], data.data[3],
            time/1000000);
#endif
  }
}

int
main(void)
{
  result_t result;

  kprintf(KR_OSTREAM, "Starting.\n");

//  configureDevN(3);
  configureDevN(4);
  configureAD(5);
 // configureDevN(13);
  configureAD(17);
  configureAD(18);
//  configureDevN(19);
//  configureDevN(20);

  // Give it a chance to get started:
  result = capros_Sleep_sleep(KR_SLEEP, 3000);	// sleep 3 seconds
  assert(result == RC_OK);

  for (;;) {
    //PrintTempDevN(3);
    PrintTempDevN(4);
    PrintADDevN(5);
    //PrintTempDevN(13);
//    PrintADDevN(17);
//    PrintADDevN(18);
//    PrintTempDevN(19);
//    PrintTempDevN(20);

    result = capros_Sleep_sleep(KR_SLEEP, 2000);	// sleep 2 seconds
    assert(result == RC_OK);
  }

  kprintf(KR_OSTREAM, "Done.\n");

  return 0;
}

