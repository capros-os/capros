/*
 * Copyright (C) 2007, Strawberry Development Group.
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

/* Serial port test.

Unfortunately, the Linux driver does not support setting the loopback option.
This test assumes that a Dallas Semiconductor DS9097-U is connected
to the serial port.
*/

#include <eros/target.h>
#include <eros/Invoke.h>
#include <idl/capros/SerialPort.h>
#include <idl/capros/SpaceBank.h>
#include <idl/capros/SuperNode.h>
#include <idl/capros/Sleep.h>
#include <idl/capros/NPLinkee.h>

#include <idl/capros/Constructor.h>
#include <domain/Runtime.h>
#include <domain/assert.h>
#include <domain/domdbg.h>

#define KR_OSTREAM KR_APP(0)
#define KR_SLEEP   KR_APP(1)
#define KR_SER     KR_APP(2)


const uint32_t __rt_stack_pointer = 0x20000;
const uint32_t __rt_unkept = 1;

#define ckOK \
  if (result != RC_OK) { \
    kdprintf(KR_OSTREAM, "Line %d result is 0x%08x!\n", __LINE__, result); \
  }

void
SerialOut_Init(unsigned int baudRate)
{
  struct capros_SerialPort_termios2 termio = {
    .c_iflag = capros_SerialPort_BRKINT | capros_SerialPort_INPCK,
    /* 8 bits No parity 1 stop bit */
    .c_cflag = capros_SerialPort_CS8 | capros_SerialPort_CBAUD
               | capros_SerialPort_CREAD,
    .c_line = 0,
    .c_ospeed = baudRate
  };

  result_t result = capros_SerialPort_setTermios2(KR_SER, termio);
  ckOK
}

#define inBufEntries 100
struct InputPair {
  uint8_t flag;
  uint8_t data;
} __attribute__ ((packed))
inBuf[inBufEntries];

bool doInitialize = true;

bool inDataMode = false;

// DS2480B codes

const unsigned int commandWrite0         = 0x81;
const unsigned int commandWrite1         = 0x91;
const unsigned int commandSearchAccelOff = 0xA1;
const unsigned int commandSearchAccelOn  = 0xB1;
const unsigned int commandReset          = 0xC1;
const unsigned int commandPulse5VStrong  = 0xED;
const unsigned int commandPulseTerminate = 0xF1;
const unsigned int commandPulse12VProg   = 0xFD;
const unsigned int commandPulseArm       = 0x02;

const unsigned int BusSpeedRegular   = 0x00;
const unsigned int BusSpeedFlexible  = 0x04;
const unsigned int BusSpeedOverdrive = 0x08;
unsigned char gBusSpeed;

const unsigned int configPDSRC = 0x10;  // pulldown slew rate control
const unsigned int configPPD   = 0x20;
const unsigned int configSPUD  = 0x30;  // strong pullup duration
const unsigned int configW1LT  = 0x40;  // write-1 low time
const unsigned int configDSO   = 0x50;  // data sample offset and write 0 recovery time
const unsigned int configLOAD  = 0x60;
const unsigned int configRBR   = 0x70;

// selected parameter value codes
const unsigned int PDSRC137 = 0x06;     // 1.37 V/µs
const unsigned int SPUD1048 = 0x0a;     // 1048 ms
const unsigned int W1LT11 = 0x06;       // 11 µs
const unsigned int DSO10 = 0x0e;        // 10 µs
const unsigned int RBR9600 = (0<<1);    //   9,600 baud
const unsigned int RBR19200 = (1<<1);   //  19,200 baud
const unsigned int RBR57600 = (2<<1);   //  57,600 baud
const unsigned int RBR115200 = (3<<1);  // 115,200 baud

/* Delay for t/60 seconds. */
void Delay(unsigned int t)
{
  capros_Sleep_sleep(KR_SLEEP, t * (1000/60));
}

void
SerialIn_Flush(void)
{
  result_t result;
  uint32_t pairsSent;
  do {
    result = capros_SerialPort_readTimeout(KR_SER, inBufEntries, 0,
               &pairsSent, (uint8_t *)inBuf);
    if (result != RC_OK && result != RC_capros_SerialPort_TimedOut) {
      kdprintf(KR_OSTREAM, "Line %d result is 0x%08x!\n", __LINE__, result); \
    }
    if (pairsSent) {
      kprintf(KR_OSTREAM, "Flushed %d pairs, 1st=(%d, 0x%x)\n",
              pairsSent, inBuf[0].flag, inBuf[0].data);
    }
  } while (result == RC_OK);
}

void
SerialOut_Flush(void)
{
  result_t result = capros_SerialPort_waitUntilSent(KR_SER);
  ckOK
}

void SendOneChar(unsigned char c)
{
  result_t result = capros_SerialPort_write(KR_SER, 1, &c);
  ckOK
}

void EnterDataMode(void)
{
  SendOneChar(0xE1);
  inDataMode = true;
}

void EnterCommandMode(void)
{
  SendOneChar(0xE3);
  inDataMode = false;
}

void EnsureDataMode(void)
{
  if (!inDataMode) EnterDataMode();
}

void EnsureCommandMode(void)
{
  if (inDataMode) EnterCommandMode();
}

unsigned char RcvOneChar(void)
{
  uint32_t pairsSent;
  kprintf(KR_OSTREAM, "Reading ...");
  result_t result = capros_SerialPort_read(KR_SER,
                      1, &pairsSent, (uint8_t *)inBuf);
  ckOK
  kprintf(KR_OSTREAM, ", got %d 0x%x.\n", pairsSent, inBuf[0].data);
  switch (inBuf[0].flag) {
  case capros_SerialPort_Flag_BREAK:
    kprintf(KR_OSTREAM, "Break!\n");
    break;
  case capros_SerialPort_Flag_FRAME:
    kprintf(KR_OSTREAM, "Frame!\n");
    break;
  case capros_SerialPort_Flag_PARITY:
    kprintf(KR_OSTREAM, "Parity!\n");
    break;
  case capros_SerialPort_Flag_OVERRUN:
    kprintf(KR_OSTREAM, "Overrun!\n");
    break;
  case capros_SerialPort_Flag_NORMAL:
    break;
  }
  return inBuf[0].data;
}

#define OutputString(x) kprintf(KR_OSTREAM, x)

void printHexChar(unsigned char c)
{
  kprintf(KR_OSTREAM, "0x%02x ", c);
}

void SendConfigCommand(unsigned int command)
// low bit of command is zero
{
  unsigned int c;

  EnsureCommandMode();
  SendOneChar(0x01 + command);
  c = RcvOneChar();
  if (c != command) {
    OutputString("Config command sent "); printHexChar(command);
    OutputString(" got "); printHexChar(c);
  }
}

void ChangeBaudRate(uint32_t baudRate, int baudCmd)
{
  EnsureCommandMode();
  SendOneChar(0x01 + baudCmd);
  // Response byte is sent at the new rate.
  SerialOut_Flush();
  SerialOut_Init(baudRate);
  Delay(1);     // or 5 ms
  SerialIn_Flush();     // flush the response byte and any garbage
}

int
main(void)
{
  result_t result;
  unsigned long err;
  Message Msg = {
    .snd_invKey = KR_VOID,
    .snd_code = RC_OK,
    .snd_key0 = KR_VOID,
    .snd_key1 = KR_VOID,
    .snd_key2 = KR_VOID,
    .snd_rsmkey = KR_VOID,
    .snd_len = 0,
    .rcv_key0 = KR_SER,
    .rcv_key1 = KR_VOID,
    .rcv_key2 = KR_VOID,
    .rcv_rsmkey = KR_RETURN,
    .rcv_limit = 0,
  };

  RETURN(&Msg);
  assert(Msg.rcv_code == OC_capros_NPLinkee_registerNPCap);
  // Reply to NPLink:
  Msg.snd_invKey = KR_RETURN;
  SEND(&Msg);

  result = capros_SerialPort_open(KR_SER, &err);
  ckOK
  kprintf(KR_OSTREAM, "Opened.\n");

  SerialOut_Init(4800);
  SendOneChar(0);
  SerialOut_Flush();

  SerialOut_Init(9600);

  Delay(1);
  gBusSpeed = BusSpeedFlexible;
        // Send initial Reset command after master reset
  SendOneChar(commandReset + gBusSpeed);

  // kprintf(KR_OSTREAM, "Flushing output ...\n");
  SerialOut_Flush();
  // kdprintf(KR_OSTREAM, "done\n");

  Delay(1);
  // kprintf(KR_OSTREAM, "Flushing input ...\n");
  SerialIn_Flush();
  // kdprintf(KR_OSTREAM, "done\n");

  doInitialize = true;
  ChangeBaudRate(19200, configRBR + RBR19200);

  SendConfigCommand(configSPUD + SPUD1048);     // strong pullup duration 1048 ms
  SendConfigCommand(configPDSRC + PDSRC137);    // pulldown slew rate control 1.37 V/µs
  SendConfigCommand(configW1LT + W1LT11);       // write-1 low time 11 µs
  SendConfigCommand(configDSO + DSO10); // data sample offset/recovery time 10 µs


  kprintf(KR_OSTREAM, "Closing\n");

  result = capros_SerialPort_close(KR_SER);
  ckOK

  kprintf(KR_OSTREAM, "Done.\n");

  return 0;
}

