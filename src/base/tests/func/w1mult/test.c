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

/* W1Mult test.
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
#include <idl/capros/Constructor.h>
#include <idl/capros/W1Mult.h>
#include <idl/capros/DS18B20.h>
#include <idl/capros/DS2450.h>
#include <idl/capros/Logfile.h>

#include <domain/Runtime.h>
#include <domain/domdbg.h>
#include <domain/assert.h>

#define KC_SNODEC 0

#define KR_OSTREAM  KR_APP(1)
#define KR_SLEEP    KR_APP(2)
#define KR_DEVNODE  KR_APP(4)

// Slots in KR_DEVNODE:
#define dev_pool_DS18B20 4
#define dev_pool_DS2450  5
#define dev_attic_DS18B20 6
#define dev_HVAC1_DS2450 17
#define dev_HVAC2_DS2450 18
#define dev_poolReturn_DS18B20 19
#define dev_poolSolar_DS18B20 20

const uint32_t __rt_stack_pointer = 0x20000;

#define maxDevs 31

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
GetLogN(int n, cap_t kr)
{
  result_t result;
  uint32_t keyType;

  result = capros_Node_getSlotExtended(KR_KEYSTORE, n, kr);
  ckOK
  result = capros_key_getType(kr, &keyType);
  ckOK
  assert(keyType == IKT_capros_Logfile);
}

void
SaveLog(int n, cap_t kr)
{
  result_t result;
  uint32_t keyType;

  result = capros_key_getType(kr, &keyType);
  ckOK
  assert(keyType == IKT_capros_Logfile);
  result = capros_Node_swapSlotExtended(KR_KEYSTORE, n, kr, KR_VOID);
  ckOK
}

void
configureTemp(int n)
{
  result_t result;

  GetDevN(n, KR_TEMP0);
  // Sample every 2 seconds, 3 binary bits of resolution.
  result = capros_DS18B20_configure(KR_TEMP0, 1, 3,
             4 /* hysteresis */,
             KR_TEMP1);
  ckOK
  SaveLog(n, KR_TEMP1);
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
    .bitsToConvert = 8, \
    .hysteresis = 512 \
  }
  capros_DS2450_portsConfiguration config = {
    .port = {
      [0] = init_2450_port,
      [1] = init_2450_port,
      [2] = init_2450_port,
      [3] = init_2450_port
    }
  };
  result = capros_DS2450_configurePorts(KR_TEMP0, config,
             KR_TEMP0, KR_TEMP1, KR_TEMP2, KR_TEMP3);
  ckOK
  SaveLog(n, KR_TEMP0);
  SaveLog(maxDevs + n, KR_TEMP1);
  SaveLog(maxDevs*2 + n, KR_TEMP2);
  SaveLog(maxDevs*3 + n, KR_TEMP3);
}

void
PrintTempDevN(int n)
{
  result_t result;
  capros_W1Mult_LogRecord16 rec16;
  uint32_t len;

  GetLogN(n, KR_TEMP0);
  result = capros_Logfile_getPreviousRecord(KR_TEMP0,
             capros_Logfile_nullRecordID,
             sizeof(rec16),
             (uint8_t *)&rec16,
             &len );
  if (result != RC_capros_Logfile_NoRecord) {
    ckOK
    assert(len == sizeof(rec16));
    short temperature = rec16.value;
    kprintf(KR_OSTREAM, "Dev %d temperature is %d.%d Celsius at %#lu sec\n",
            n, temperature/16, (temperature%16) >> 1, rec16.header.rtc);
  } else {
    kprintf(KR_OSTREAM, "Dev %d not sampled\n", n);
  }
}

void
PrintADDevN(int n)
{
  int i;
  result_t result;
  capros_W1Mult_LogRecord16 rec16;
  uint32_t len;

  kprintf(KR_OSTREAM, "Dev %d data is ", n);
  for (i = 0; i < 4; i++) {
    GetLogN(maxDevs * i + n, KR_TEMP0);
    result = capros_Logfile_getPreviousRecord(KR_TEMP0,
               capros_Logfile_nullRecordID,
               sizeof(rec16),
               (uint8_t *)&rec16,
               &len );
    if (result != RC_capros_Logfile_NoRecord) {
      ckOK
      assert(len == sizeof(rec16));
#if 0
      kprintf(KR_OSTREAM, "%#.4x ", rec16.value);
#else
      kprintf(KR_OSTREAM, "%u ", rec16.value);
#endif
    } else {
      kprintf(KR_OSTREAM, ". ");
    }
  }
  // Print only the last rtc value.
  kprintf(KR_OSTREAM, "at %#lu sec\n", rec16.header.rtc);
}

int
main(void)
{
  result_t result;

  // Create SuperNode for holding Logfiles.
  result = capros_Node_getSlotExtended(KR_CONSTIT, KC_SNODEC, KR_TEMP0);
  assert(result == RC_OK);
  result = capros_Constructor_request(KR_TEMP0,
             KR_BANK, KR_SCHED, KR_VOID, KR_KEYSTORE);
  assert(result == RC_OK);
  result = capros_SuperNode_allocateRange(KR_KEYSTORE, 0, maxDevs*4);
  assert(result == RC_OK);

  kprintf(KR_OSTREAM, "Starting.\n");

//  configureTemp(3);
  configureTemp(dev_pool_DS18B20);
  configureAD(dev_pool_DS2450);
  configureTemp(dev_attic_DS18B20);
 // configureTemp(13);
  configureAD(dev_HVAC1_DS2450);
  configureAD(dev_HVAC2_DS2450);
  configureTemp(dev_poolReturn_DS18B20);
  configureTemp(dev_poolSolar_DS18B20);

  // Give it a chance to get started:
  result = capros_Sleep_sleep(KR_SLEEP, 3000);	// sleep 3 seconds
  assert(result == RC_OK || result == RC_capros_key_Restart);

  kprintf(KR_OSTREAM, "Looping.\n");

  for (;;) {
    //PrintTempDevN(3);
    PrintTempDevN(dev_pool_DS18B20);
    PrintADDevN(dev_pool_DS2450);
    PrintTempDevN(dev_attic_DS18B20);
    //PrintTempDevN(13);
//    PrintADDevN(dev_HVAC1_DS2450);
//    PrintADDevN(dev_HVAC2_DS2450);
    PrintTempDevN(dev_poolReturn_DS18B20);
    PrintTempDevN(dev_poolSolar_DS18B20);

    result = capros_Sleep_sleep(KR_SLEEP, 2000);	// sleep 2 seconds
    assert(result == RC_OK || result == RC_capros_key_Restart);
  }

  kprintf(KR_OSTREAM, "Done.\n");

  return 0;
}

