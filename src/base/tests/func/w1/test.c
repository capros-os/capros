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

//#include <linux/usb/ch9.h>

#include <domain/Runtime.h>
#include <domain/domdbg.h>
#include <domain/assert.h>

#define KR_OSTREAM KR_APP(1)
#define KR_SLEEP   KR_APP(2)
#define KR_DEVPRIVS KR_APP(3)
#define KR_SEG     KR_APP(4)

#define KR_W1BUS   KR_APP(5)


const uint32_t __rt_stack_pointer = 0x20000;
const uint32_t __rt_unkept = 1;

#define ckOK \
  if (result != RC_OK) { \
    kdprintf(KR_OSTREAM, "Line %d result is 0x%08x!\n", __LINE__, result); \
  }

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

#define maxDevices 50
struct W1Device {
  uint64_t rom;
  struct W1Device * parent;	// parent coupler or NULL
  uint8_t mainOrAux;		// which branch of parent
} devices[maxDevices],
  * nextDev = &devices[0];

result_t
DoRunProgram(void)
{
  RunPgmMsg.snd_len = outCursor - outBeg;
  return CALL(&RunPgmMsg);
}

#define RunGoodProgram \
  result_t result = DoRunProgram(); \
  ckOK \
  if (RunPgmMsg.rcv_w1 != capros_W1Bus_StatusCode_OK) \
    kdprintf(KR_OSTREAM, "Got status %d", RunPgmMsg.rcv_w1);

void
SearchPath(struct W1Device * parent, uint8_t mainOrAux)
{
  result_t result;
  uint64_t rom;
  struct W1Device * pdev;
  uint64_t discrep;
  unsigned char * initCursor;

  // Set up parent path.
  initCursor = outBeg;
  *initCursor++ = capros_W1Bus_stepCode_resetSimple;
  // Set up parent path.
  for (pdev = parent; pdev; pdev = pdev->parent) {
    *initCursor++ = mainOrAux;
    memcpy(initCursor, &pdev->rom, 8);
    initCursor += 8;
  }
#if 1
  // Deactivate the branch lines of any couplers at this level,
  // so we will search just at this level.
  outCursor = initCursor;
  *outCursor++ = capros_W1Bus_stepCode_skipROM;
  *outCursor++ = capros_W1Bus_stepCode_writeBytes;
  *outCursor++ = 1;		// write one byte
  *outCursor++ = 0x66;		// all lines off
  // If there are any couplers, they will transmit a confirmation byte.
  // If there are none, this will read ones.
  *outCursor++ = capros_W1Bus_stepCode_readBytes;
  *outCursor++ = 1;		// read one byte
  result = DoRunProgram();
  ckOK
  if (RunPgmMsg.rcv_w1 != capros_W1Bus_StatusCode_OK)
    kprintf(KR_OSTREAM, "All lines off got status %d", RunPgmMsg.rcv_w1);
  if (RunPgmMsg.rcv_sent != 1)
    kprintf(KR_OSTREAM, "All lines off got %d bytes", RunPgmMsg.rcv_sent);
////  if (inBuf[0] != 0x66 && inBuf[0] != 0xff)
    kprintf(KR_OSTREAM, "All lines off got byte %#x", inBuf[0]);
#endif

  // Find ROMs.
  rom = 0;		// next ROM to find
  while (1) {
    outCursor = initCursor;
    *outCursor++ = capros_W1Bus_stepCode_searchROM;
    memcpy(outCursor, &rom, 8);
    outCursor += 8;
    result = DoRunProgram();
    ckOK

    if (RunPgmMsg.rcv_w1 == capros_W1Bus_StatusCode_NoDevicePresent)
      break;	// no device
    if (RunPgmMsg.rcv_sent != 16)
      kdprintf(KR_OSTREAM, "SearchROM got %d bytes", RunPgmMsg.rcv_sent);
    memcpy(&rom, inBuf, 8);	// little-endian
    memcpy(&discrep, inBuf+8, 8);
    kprintf(KR_OSTREAM, "ROM %#.16llx\n", rom);

    // Make a new W1Device.
    struct W1Device * dev = nextDev++;
    dev->rom = rom;
    dev->parent = parent;
    dev->mainOrAux = mainOrAux;

    if ((rom & 0xff) == 0x1f) {	// a coupler, recurse
#if 0	// special scope test
  initCursor = outBeg;
  *initCursor++ = capros_W1Bus_stepCode_resetSimple;
  // Set up parent path.
    *initCursor++ = capros_W1Bus_stepCode_setPathMain;
    memcpy(initCursor, &dev->rom, 8);
    initCursor += 8;
  for (;;) {
    initCursor = outBeg+10;
    result = DoRunProgram();
    ckOK
  }

#endif
      SearchPath(dev, capros_W1Bus_stepCode_setPathMain);
      SearchPath(dev, capros_W1Bus_stepCode_setPathAux);
    }

    // Calculate next ROM address to look for.
    // This is tricky because the search begins with the LSB.
    if (! discrep) break;	// no more devices
    unsigned int bit = fls64(discrep) - 1;
    uint64_t mask = 1ULL << bit;
    assert(! (rom & mask));
    rom = mask | (rom & (mask - 1));
    kprintf(KR_OSTREAM, "discrep %#.16llx nextrom %#.16llx\n", discrep, rom);
  }
}

void
TestW1Bus(void)
{
  result_t result;
  int i;
  unsigned long theType;

  result = capros_key_getType(KR_W1BUS, &theType);
  ckOK
  if (theType != IKT_capros_W1Bus)
    kdprintf(KR_OSTREAM, "Line %d type is %#x!\n", __LINE__, theType);

  /* Apparently, rebooting the CPU doesn't reset the device,
  so do it here: */
  result = capros_W1Bus_resetDevice(KR_W1BUS);
  ckOK

  result = capros_W1Bus_setSpeed(KR_W1BUS, capros_W1Bus_W1Speed_flexible);
  ckOK

  result = capros_W1Bus_setPDSR(KR_W1BUS, capros_W1Bus_PDSR_PDSR137);
	// 1.37 V/us
  ckOK

  result = capros_W1Bus_setW1LT(KR_W1BUS, capros_W1Bus_W1LT_W1LT11);	// 11 us
  ckOK

  result = capros_W1Bus_setDSO(KR_W1BUS, capros_W1Bus_DSO_DSO10);  // 10 us
  ckOK

  outCursor = outBeg + capros_W1Bus_maxProgramSize + 1;
  result = DoRunProgram();
  assert(result == RC_capros_W1Bus_ProgramTooLong);

  outCursor = outBeg;
  *outCursor++ = capros_W1Bus_stepCode_resetNormal;

  for (i = 0; i < 0000; i++) {
    RunGoodProgram
  }

  // Search for all ROMs.
  SearchPath(NULL, 0);
#if 0
  uint64_t rom;
  rom = 0;
  uint64_t discrep;
  do {
    outCursor = outBeg;
    *outCursor++ = capros_W1Bus_stepCode_resetSimple;
    *outCursor++ = capros_W1Bus_stepCode_searchROM;
    memcpy(outCursor, &rom, 8);
    outCursor += 8;
    result = DoRunProgram();
    ckOK
    if (RunPgmMsg.rcv_w1 == capros_W1Bus_StatusCode_NoDevicePresent)
      break;	// no device
    if (RunPgmMsg.rcv_sent != 16)
      kdprintf(KR_OSTREAM, "SearchROM got %d bytes", RunPgmMsg.rcv_sent);
    memcpy(&rom, inBuf, 8);	// little-endian
    memcpy(&discrep, inBuf+8, 8);
    kprintf(KR_OSTREAM, "ROM %#.16llx\n", rom);
    // Calculate next ROM address to look for.
    // This is tricky because the search begins with the LSB.
    if (! discrep) break;	// no more devices
    unsigned int bit = fls64(discrep) - 1;
    uint64_t mask = 1ULL << bit;
    assert(! (rom & mask));
    rom = mask | (rom & (mask - 1));
    //kprintf(KR_OSTREAM, "discrep %#.16llx nextrom %#.16llx\n", discrep, rom);
  } while (true);
#endif

  kprintf(KR_OSTREAM, "\nDone.\n");
}

int
main(void)
{
  //result_t result;

  kprintf(KR_OSTREAM, "Starting.\n");

  // We will receive any new W1Bus cap.
  Message Msg = {
    .snd_invKey = KR_VOID,
    .snd_code = RC_OK,
    .snd_w1 = 0,
    .snd_w2 = 0,
    .snd_w3 = 0,
    .snd_key0 = KR_VOID,
    .snd_key1 = KR_VOID,
    .snd_key2 = KR_VOID,
    .snd_rsmkey = KR_VOID,
    .snd_len = 0
  };

  for (;;) {
    Msg.rcv_key0 = KR_W1BUS;
    Msg.rcv_key1 = KR_VOID;
    Msg.rcv_key2 = KR_VOID;
    Msg.rcv_rsmkey = KR_RETURN;
    Msg.rcv_limit = 0;

    RETURN(&Msg);

    // Set up defaults for return:
    Msg.snd_invKey = KR_RETURN;
    Msg.snd_code = RC_OK;
    Msg.snd_w1 = 0;
    Msg.snd_w2 = 0;
    Msg.snd_w3 = 0;
    Msg.snd_key0 = KR_VOID;
    Msg.snd_key1 = KR_VOID;
    Msg.snd_key2 = KR_VOID;
    Msg.snd_rsmkey = KR_VOID;
    Msg.snd_len = 0;

    kprintf(KR_OSTREAM, "Test called, OC=%#x\n", Msg.rcv_code);

    switch (Msg.rcv_code) {
    default:
      Msg.snd_code = RC_capros_key_UnknownRequest;
      break;

    case OC_capros_key_getType:
      Msg.snd_w1 = 0xbadbad;
      break;

    case RC_OK:
      kprintf(KR_OSTREAM, "\nGot W1Bus cap.\n");
      TestW1Bus();
    }
  }

  return 0;
}

