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

/* W1Mult
   This persistent program receives a capability to a 1-Wire bus master
   and serves caps to individual devices on that bus.
 */

#include <stdint.h>
#include <string.h>
#include <eros/fls.h>
#include <eros/target.h>
#include <eros/machine/cap-instr.h>

#include <idl/capros/key.h>
#include <idl/capros/Node.h>
#include <idl/capros/Sleep.h>
#include <idl/capros/Constructor.h>
#include <idl/capros/SuperNode.h>
#include <idl/capros/W1Mult.h>

#include <domain/domdbg.h>
#include <domain/assert.h>

#include "w1mult.h"
#include "w1multConfig.h"
#include "DS18B20.h"

#define KC_SNODEC 0

/* Memory:
  0: nothing
  0x01000: code
  0x1e000: stack
  0x1f000: config file */
#define configFileAddr 0x1f000

uint32_t __rt_unkept = 1;
unsigned long __rt_stack_pointer = 0x1f000;

bool haveBusKey = false;	// no key in KR_W1BUS yet

// Stuff for programming the 1-Wire bus:
unsigned char outBuf[capros_W1Bus_maxProgramSize + 1];
unsigned char * const outBeg = &outBuf[0];
unsigned char * outCursor;
unsigned char inBuf[capros_W1Bus_maxResultsSize];

Message RunPgmMsg = {
  .snd_invKey = KR_W1BUS,
  .snd_code = 3,    // for now
  .snd_w1 = 0,
  .snd_w2 = 0,
  .snd_w3 = 0,
  // .snd_len = outlen,
  .snd_data = &outBuf[0],
  .snd_key0 = KR_VOID,
  .snd_key1 = KR_VOID,
  .snd_key2 = KR_VOID,
  .rcv_limit = sizeof(inBuf),
  .rcv_data = inBuf,
  .rcv_key0 = KR_VOID,
  .rcv_key1 = KR_VOID,
  .rcv_key2 = KR_VOID,
  .rcv_rsmkey = KR_VOID,
};

/******************* some utility procedures ************************/

uint64_t
GetCurrentTime(void)
{
  capros_Sleep_nanoseconds_t time;
  result_t result = capros_Sleep_getTimeMonotonic(KR_SLEEP, &time);
  assert(result == RC_OK);
  return time;
}

uint8_t
CalcCRC8(uint8_t * data, unsigned int len)
{
  int i, j;
  unsigned int crc = 0;

  for (i = 0; i < len; i++) {		// for each byte
    unsigned int s = data[i];
    for (j = 0; j < 8; j++, s>>=1) {	// for each bit
      if ((s ^ crc) & 0x1) {
        crc ^= 0x118; // 0b100011000
      }
      crc >>= 1;      
    }
  }
  return crc;
}

/* Run the program from outBeg to outCursor.
Returns:
  -1 if W1Bus cap is gone
  else a StatusCode
 */
int
RunProgram(void)
{
  RunPgmMsg.snd_len = outCursor - outBeg;
  result_t result = CALL(&RunPgmMsg);
  if (result == RC_capros_key_Void)
    return -1;
  assert(result == RC_OK);
#if 1		// debug
  if (RunPgmMsg.rcv_w1)
    kdprintf(KR_OSTREAM, "Got status %d", RunPgmMsg.rcv_w1);
#endif
  return RunPgmMsg.rcv_w1;
}

#define maxDevices 50

struct W1Device devices[maxDevices];

enum {
  branchUnknown = 0,
  branchMain = capros_W1Bus_stepCode_setPathMain,
  branchAux  = capros_W1Bus_stepCode_setPathAux
};

unsigned int numDevices = 0;

static uint8_t
w1dev_getFamilyCode(struct W1Device * dev)
{
  return dev->rom & 0xff;
}

static bool
w1dev_IsCoupler(struct W1Device * dev)
{
  return w1dev_getFamilyCode(dev) == famCode_DS2409;
}

#if 0 // if needed:
static bool
w1dev_IsThermom(struct W1Device * dev)
{
  return w1dev_getFamilyCode(dev) == famCode_DS18B20;
}

static bool
w1dev_IsAD(struct W1Device * dev)
{
  return w1dev_getFamilyCode(dev) == famCode_DS2450;
}

static bool
w1dev_IsBatt(struct W1Device * dev)
{
  return w1dev_getFamilyCode(dev) == famCode_DS2438;
}
#endif

void
AddressDevice(struct W1Device * dev)
{
  wp(capros_W1Bus_stepCode_resetSimple)
  // Set up path through any couplers.
  // Address parents from the bus root to the device. 
  struct W1Device * addressedSoFar = NULL;
  while (dev->parent != addressedSoFar) {
    struct W1Device * pdev;
    uint8_t pdevBranch = dev->mainOrAux;
    for (pdev = dev->parent;
         pdev->parent != addressedSoFar;
         pdev = pdev->parent) {
      pdevBranch = pdev->mainOrAux;
    }
    if (pdev->u.coupler.activeBranch != pdevBranch) {
      wp(pdevBranch)	// capros_W1Bus_stepCode_setPath*
      memcpy(outCursor, &pdev->rom, 8);
      outCursor += 8;
      addressedSoFar = pdev;
    }
  }

  wp(capros_W1Bus_stepCode_matchROM)
  memcpy(outCursor, &dev->rom, 8);
  outCursor += 8;
}

/* Returns -1 if W1Bus cap is gone, else 0. */
int
SearchPath(struct W1Device * parent, uint8_t mainOrAux)
{
  int statusCode;
  int i;
  uint64_t rom;
  uint64_t discrep;
  unsigned char * initCursor;

  outCursor = outBeg;
  *outCursor++ = capros_W1Bus_stepCode_resetSimple;
  // Set up path through any couplers.
  // Address parents from the bus root to the device. 
  struct W1Device * addressedSoFar = NULL;
  while (parent != addressedSoFar) {
    struct W1Device * pdev;
    uint8_t pdevBranch = mainOrAux;
    for (pdev = parent;
         pdev->parent != addressedSoFar;
         pdev = pdev->parent) {
      pdevBranch = pdev->mainOrAux;
    }
    *outCursor++ = pdevBranch;	// capros_W1Bus_stepCode_setPath*
    memcpy(outCursor, &pdev->rom, 8);
    outCursor += 8;
    addressedSoFar = pdev;
  }
  initCursor = outCursor;	// save repeating the above

  // Deactivate the branch lines of any couplers at this level,
  // so we will search just at this level.
  outCursor = initCursor;
  *outCursor++ = capros_W1Bus_stepCode_skipROM;
  *outCursor++ = capros_W1Bus_stepCode_writeBytes;
  *outCursor++ = 1;		// write one byte
  *outCursor++ = 0x66;		// all lines off
  *outCursor++ = capros_W1Bus_stepCode_readBytes;
  *outCursor++ = 1;		// read one byte
  // If there are any couplers, they will transmit a confirmation byte.
  // If there are none, this will read ones.
  statusCode = RunProgram();
  if (statusCode < 0)
    return statusCode;

  if (statusCode != capros_W1Bus_StatusCode_OK
      || RunPgmMsg.rcv_sent != 1
      || (inBuf[0] != 0x66 && inBuf[0] != 0xff) ) {
    if (statusCode == capros_W1Bus_StatusCode_NoDevicePresent
        && RunPgmMsg.rcv_w2 < (initCursor - outBeg) ) {
      kprintf(KR_OSTREAM, "No devices on this branch.\n");////
    } else {
      kprintf(KR_OSTREAM, "All lines off got status %d %d %d bytes=%d",
        statusCode, RunPgmMsg.rcv_w2, RunPgmMsg.rcv_w3,
        RunPgmMsg.rcv_sent);
      kprintf(KR_OSTREAM, "All lines off got byte %#x", inBuf[0]);
      // FIXME recover; repeat?
    }
  }
  else	// there are devices, and all lines off did not fail
  {
    // Find ROMs on this branch.
    rom = 0;		// next ROM to find
    while (1) {
      outCursor = initCursor;
      *outCursor++ = capros_W1Bus_stepCode_searchROM;
      memcpy(outCursor, &rom, 8);
      outCursor += 8;
      statusCode = RunProgram();
      if (statusCode < 0)
        return statusCode;

      if (statusCode == capros_W1Bus_StatusCode_NoDevicePresent)
        break;	// no device
      assert(statusCode == capros_W1Bus_StatusCode_OK);
      if (RunPgmMsg.rcv_sent != 16)
        kdprintf(KR_OSTREAM, "SearchROM got %d bytes", RunPgmMsg.rcv_sent);

      memcpy(&rom, inBuf, 8);	// little-endian
      memcpy(&discrep, inBuf+8, 8);
      DEBUG(search) kprintf(KR_OSTREAM, "ROM %#.16llx\n", rom);

      // Is this device in our configuration?
      /* This search could be more efficient, but I don't expect
      large numbers of devices. */
      for (i = 0; i < numDevices; i++) {
        if (devices[i].rom == rom
            && devices[i].parent == parent) {
          // If there is a parent, mainOrAux must match:
          if (! parent
              || devices[i].mainOrAux == mainOrAux) {	// found it
            devices[i].found = true;
            break;
          }
        }
      }
      if (i == numDevices) {	// did not find it
        if (parent) 
          kprintf(KR_OSTREAM, "From parent %#.16llx %#x ",
                  parent->rom, mainOrAux);
        kprintf(KR_OSTREAM, "ROM %#.16llx found but not configured.\n", rom);
      }
      else {
        struct W1Device * dev = &devices[i];

        if (w1dev_IsCoupler(dev)) {	// recurse
          statusCode = SearchPath(dev, capros_W1Bus_stepCode_setPathMain);
          if (statusCode < 0)
            return statusCode;
          statusCode = SearchPath(dev, capros_W1Bus_stepCode_setPathAux);
          if (statusCode < 0)
            return statusCode;
        }
      }

      // Calculate next ROM address to look for.
      // This is tricky because the search begins with the LSB.
      DEBUG(search) kprintf(KR_OSTREAM, "discrep %#.16llx ", discrep);
      uint64_t mask;
      while (discrep) {
        unsigned int bit = fls64(discrep) - 1;
        mask = 1ULL << bit;
        if (! (rom & mask))
          break;
        /* For this discrepancy, we already chose the 1,
        so we are done with that bit: */
        discrep &= ~mask;
      }
      if (! discrep) break;	// no more devices
      rom = mask | (rom & (mask - 1));
      DEBUG(search) kprintf(KR_OSTREAM, "nextrom %#.16llx\n", rom);
    }
  }
  return 0;
}

/* We just got a new W1Bus cap.
 */
int	// returns -1 if W1Bus cap went away.
ScanBus(void)
{
  result_t result;
  int statusCode;
  int i;

#define ckres \
  if (result == RC_capros_key_Void) \
    return -1; \
  assert(result == RC_OK);

  /* Apparently, rebooting the CPU doesn't reset the device,
  so do it here: */
  result = capros_W1Bus_resetDevice(KR_W1BUS);
  ckres

  // Set bus parameters:
  result = capros_W1Bus_setSpeed(KR_W1BUS, capros_W1Bus_W1Speed_flexible);
  ckres

  result = capros_W1Bus_setPDSR(KR_W1BUS, capros_W1Bus_PDSR_PDSR137);
        // 1.37 V/us
  ckres

  result = capros_W1Bus_setW1LT(KR_W1BUS, capros_W1Bus_W1LT_W1LT11);    // 11 us
  ckres

  result = capros_W1Bus_setDSO(KR_W1BUS, capros_W1Bus_DSO_DSO10);  // 10 us
  ckres

  // Search for all ROMs.
  for (i = 0; i < numDevices; i++) {
    struct W1Device * dev = &devices[i];
    dev->found = false;
    // We may have lost the state of the devices:
    if (w1dev_IsCoupler(dev))
        dev->u.coupler.activeBranch = branchUnknown;
  }

  statusCode = SearchPath(NULL, 0);
  if (statusCode < 0)
    return statusCode;

  for (i = 0; i < numDevices; i++) {
    if (! devices[i].found) {
      kprintf(KR_OSTREAM, "ROM %#.16llx configured but not found.\n",
              devices[i].rom);
    }
  }

  haveBusKey = true;
  return 0;
}

int
main(void)
{
  Message Msg;
  result_t result;
  
  Msg.snd_invKey = KR_VOID;
  Msg.snd_key0 = KR_VOID;
  Msg.snd_key1 = KR_VOID;
  Msg.snd_key2 = KR_VOID;
  Msg.snd_rsmkey = KR_VOID;
  Msg.snd_len = 0;
  Msg.snd_code = 0;
  Msg.snd_w1 = 0;
  Msg.snd_w2 = 0;
  Msg.snd_w3 = 0;

  Msg.rcv_key0 = KR_ARG(0);
  Msg.rcv_key1 = KR_VOID;
  Msg.rcv_key2 = KR_VOID;
  Msg.rcv_rsmkey = KR_RETURN;
  Msg.rcv_limit = 0;

  // Read the configuration file.
  struct W1DevConfig * cfg;
  for (cfg = (void *)configFileAddr;
       cfg->thisIndex >= 0;
       cfg++) {
#if 0
    kprintf(KR_OSTREAM, "cfg this %d parent %d m/a %d rom %#.16llx numDev %d\n",
      cfg->thisIndex, cfg->parentIndex, cfg->mainOrAux, cfg->rom, numDevices);
#endif
    assert(cfg->thisIndex == numDevices);	// thisIndex is a sanity check
    assert(numDevices < maxDevices);	// else device array is too small
    assert(cfg->parentIndex < (int)numDevices);  // parent must already be defined
    struct W1Device * dev = &devices[numDevices];
    if (cfg->parentIndex < 0) {		// no parent coupler
      dev->parent = NULL;
      dev->mainOrAux = 0;	// for cleanliness
    } else {
      assert(cfg->mainOrAux == branch_main || cfg->mainOrAux == branch_aux);
      dev->parent = &devices[cfg->parentIndex];
      dev->mainOrAux = cfg->mainOrAux;
    }
    dev->rom = cfg->rom;
    link_Init(&dev->workQueueLink);
    dev->callerWaiting = false;
    // Do device-specific initialization:
    switch (w1dev_getFamilyCode(dev)) {
    case famCode_DS2409:
      dev->u.coupler.activeBranch = branchUnknown;
      dev->u.coupler.mainNeedsWork = false;
      dev->u.coupler.auxNeedsWork = false;
      break;
    case famCode_DS18B20:
      DS18B20_InitDev(dev);
      break;
    default: break;
    }

    numDevices++;
  }

  // Create supernode.
  result = capros_Node_getSlotExtended(KR_CONSTIT, KC_SNODEC, KR_TEMP0);
  assert(result == RC_OK);
  result = capros_Constructor_request(KR_TEMP0,
             KR_BANK, KR_SCHED, KR_VOID, KR_SNODE);
  assert(result == RC_OK);	// FIXME
  // Slot i is for device i.
  result = capros_SuperNode_allocateRange(KR_SNODE, 0, numDevices - 1);
  assert(result == RC_OK);	// FIXME
  
  // kdprintf(KR_OSTREAM, "w1mult: accepting requests\n");

  for(;;) {
    RETURN(&Msg);

    // Defaults for reply:
    Msg.snd_invKey = KR_RETURN;
    Msg.snd_code = RC_OK;
    Msg.snd_w1 = 0;
    Msg.snd_w2 = 0;
    Msg.snd_w3 = 0;

    switch (Msg.rcv_keyInfo) {
    case 0xffff:	// nplink has this key
      switch (Msg.rcv_code) {
      default:
        Msg.snd_code = RC_capros_key_UnknownRequest;
        break;

      case OC_capros_W1Mult_RegisterBus:
        // We don't use the subtype in Msg.rcv_w1.
        COPY_KEYREG(KR_ARG(0), KR_W1BUS);
        ScanBus();
        break;
      }
      break;

    case 0xfffe:	// timer thread has this key
      break;

    default:		// a device key, keyInfo == device number
    {
      assert(Msg.rcv_keyInfo < numDevices);
      struct W1Device * dev = &devices[Msg.rcv_keyInfo];

      switch (w1dev_getFamilyCode(dev)) {
      case famCode_DS18B20:
        DS18B20_ProcessRequest(dev, &Msg);
        break;

      case famCode_DS2409:

        switch (Msg.rcv_code) {
        default:
          Msg.snd_code = RC_capros_key_UnknownRequest;
          break;

        case OC_capros_key_getType:
#if 0 // incomplete
          Msg.snd_w1 = IKT_capros_;
#endif
          break;

        }
        break;
      }		// end of switch on family code
    }
    }
  }
}
