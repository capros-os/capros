/*
 * Copyright (C) 2008, 2009, Strawberry Development Group.
 *
 * This file is part of the CapROS Operating System.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */
/* This material is based upon work supported by the US Defense Advanced
Research Projects Agency under Contract No. W31P4Q-07-C-0070.
Approved for public release, distribution unlimited. */

#include <string.h>
#include <eros/Invoke.h>
#include <eros/machine/cap-instr.h>
#include <idl/capros/Node.h>
#include <idl/capros/Process.h>
#include <idl/capros/SpaceBank.h>
#include <idl/capros/Forwarder.h>
#include <idl/capros/Sleep.h>
#include <idl/capros/NPLink.h>
#include <idl/capros/SerialPort.h>
#include <idl/capros/W1Mult.h>
#include <idl/capros/W1Bus.h>
#include <idl/capros/NPLinkee.h>
#include <idl/capros/Errno.h>
#include <domain/assert.h>

uint32_t __rt_unkept = 1;
unsigned long __rt_stack_pointer = 0x20000;

/* This is an adapter that takes a SerialPort connected to a
 * Dallas Semiconductor DS2480B and supplies a W1Bus. */

#define dbg_prog   0x1
#define dbg_errors 0x2
#define dbg_init   0x4

/* Following should be an OR of some of the above */
#define dbg_flags   ( 0u | dbg_errors )////

#define DEBUG(x) if (dbg_##x & dbg_flags)

#define keyInfo_nplinkee 0
#define keyInfo_W1Bus    1

#define KR_OSTREAM KR_APP(0)
#define KR_W1MULT  KR_APP(1)
#define KR_SLEEP   KR_APP(2)
#define KR_Serial  KR_APP(3)
#define KR_Forwarder KR_APP(4) // non-opaque Forwarder for W1Bus cap

// DS2480B codes

const unsigned int commandWrite0         = 0x81;
const unsigned int commandWrite1         = 0x91;
const unsigned int commandSearchAccelOff = 0xA1;
const unsigned int commandSearchAccelOn  = 0xB1;
const unsigned int commandReset          = 0xC1;
const unsigned int commandToDataMode     = 0xE1;
const unsigned int commandToCommandMode  = 0xE3;
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
const unsigned int configDSO   = 0x50;  // data sample offset
					//  and write 0 recovery time
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

bool haveSerial = false;

// Delay len milliseconds.
// len must be < 4294, else the multiplication below must be 64 bit.
void
msDelay(int len)
{
  result_t result;
  result = capros_Sleep_sleepForNanoseconds(KR_SLEEP, len * 1000000UL);
  assert(result == RC_OK || result == RC_capros_key_Restart);
}

void
RescindAnyW1BusCap(void)
{
  capros_SpaceBank_free1(KR_BANK, KR_Forwarder);
  // Ignore any error; KR_Forwarder could be Void.
}

INLINE result_t
StatusCodeToException(unsigned int status)
{
  switch (status) {
  default:	// capros_W1Bus_StatusCode_BusError
    return RC_capros_W1Bus_BusError;
  case capros_W1Bus_StatusCode_SysRestart:
    return RC_capros_key_Restart;
  }
}

bool
CheckRestart(result_t result)
{
  if (result == RC_capros_key_Restart || result == RC_capros_key_Void) {
    haveSerial = false;
    RescindAnyW1BusCap();
    return true;
  }
  return false;
}

/* There are several buffers used in this program. 
 *
 * MsgRcvBuf receives data from processes that call start caps
 *   to this process, such as the program in a RunProgram operation.
 *
 * MsgSndBuf holds data going to other processes,
 * namely the results of a RunProgram operation.
 *
 * sendpacket holds data destined for the serial port,
 * notably the commands and data for a segment of a program.
 *
 * inBuf holds data received from the serial port.
 *
 * smallBuf in ChangeBaudRate holds data that needs to go to the serial port
 * in front of the data being assembled in sendpacket.
 */

unsigned long MsgRcvBuf[capros_W1Bus_maxProgramSize/4];

unsigned long MsgSndBuf[capros_W1Bus_maxReadSize/4];

#define sendMaxSize (capros_W1Bus_maxReadSize * 2)	// hopefully enough
unsigned int sendlen;
uint8_t sendpacket[sendMaxSize];
#define wp(c) sendpacket[sendlen++] = (c)

#define inBufEntries (capros_W1Bus_maxReadSize * 2) // hopefully enough
struct InputPair {
  uint8_t flag;
  uint8_t data;
} __attribute__ ((packed))
inBuf[inBufEntries];
unsigned int numInputPairsProcessed;
unsigned int numInputPairsInBuf;

bool	// true iff successful, false if system restart
serial_sendData(void)
{
  result_t result;
  result = capros_SerialPort_write(KR_Serial, sendlen, &sendpacket[0]);
  if (CheckRestart(result))
    return false;
  if (result != RC_OK) {
    DEBUG(errors) kprintf(KR_OSTREAM, "SerialPort_write returned %#x\n",
                          result);
    assert(false);	// bug
  }
  sendlen = 0;	// reset for next packet
  return true;
}

bool
SerialOut_Flush(void)
{
  result_t result = capros_SerialPort_waitUntilSent(KR_Serial);
  if (CheckRestart(result))
    return false;
  assert(result == RC_OK);
  return true;
}

bool
SerialIn_Flush(void)
{
  msDelay(100);	// FIXME: we need a more reliable way to know that
		// characters aren't working their way through the driver
  uint32_t pairsRcvd;
  result_t result;
  numInputPairsInBuf = 0;
  do {
    result = capros_SerialPort_readTimeout(KR_Serial,
               inBufEntries, 0 /* timeout */,
               &pairsRcvd, (uint8_t *)&inBuf[0]);
  } while (result == RC_OK && pairsRcvd == inBufEntries);
  if (CheckRestart(result))
    return false;
  return true;
}

bool
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

  result_t result = capros_SerialPort_setTermios2(KR_Serial, termio);
  if (CheckRestart(result))
    return false;
  assert(result == RC_OK);
  return true;
}

bool inDataMode;
bool inDataModeAtBeginningOfSegment;

void EnterDataMode(void)
{
  wp(commandToDataMode);
  inDataMode = true;
}

void EnterCommandMode(void)
{
  wp(commandToCommandMode);
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

uint32_t currentBaudRate;

/* baudPreference keeps track of what baud rate we ought to run at,
 * with some hysteresis to avoid excessive rate switching.
 *
 * It is set to baudPref9600 if a segment has a search command,
 * which must run at 9600.
 *
 * It is decremented by the number of bytes that can be transmitted
 * at 19,200 baud. If it goes negative, we switch to 19,200 baud.
 *
 * baudPref9600 is chosen for the tradeoff; for about that many characters,
 * it's faster to switch to the higher rate, even if we have to switch back.
 */
#define baudPref9600 30
int baudPreference;

bool	// return false iff Restart
ChangeBaudRate(uint32_t baudRate, int baudCmd)
{
  /* Beware:
  1. sendpacket has data in it, so we can't use that buffer.
  2. inDataMode reflects commands programmed into sendpacket,
  but the change baud command goes out before that! */
  uint8_t smallBuf[3];
  uint8_t * p = smallBuf;

  if (inDataModeAtBeginningOfSegment) {
    *p++ = commandToCommandMode;
    *p++ = baudCmd + 0x01;
    *p++ = commandToDataMode;	// restore the mode expected by sendpacket
  } else {
    *p++ = baudCmd + 0x01;
  }
  result_t result;
  result = capros_SerialPort_write(KR_Serial, p - smallBuf, smallBuf);
  if (CheckRestart(result))
    return false;
  if (result != RC_OK) {
    DEBUG(errors) kprintf(KR_OSTREAM, "SerialPort_write returned %#x\n",
                          result);
    assert(false);
  }
  if (! SerialOut_Flush())
    return false;
  currentBaudRate = baudRate;
  if (! SerialOut_Init(baudRate))
    return false;
  // Response byte is sent at the new rate.
  // Since that may be garbled, just flush any input.
  return SerialIn_Flush();
}

// Read at least minChars characters, more if available.
/* Returns:
 * 0 if OK.
 * capros_W1Bus_StatusCode_BusError if a timeout or data error
 * capros_W1Bus_StatusCode_SysRestart if restart */
unsigned int
RcvCharsMin(unsigned int minChars)
{
  result_t result;
  uint32_t pairsRcvd;
  const unsigned int minTotalChars = numInputPairsProcessed + minChars;
  while (numInputPairsInBuf < minTotalChars) {
    // Read characters up to the size of the buffer.
    result = capros_SerialPort_readTimeout(KR_Serial,
               inBufEntries - numInputPairsInBuf,
               100000,		// timeout: 100,000 us
               &pairsRcvd, (uint8_t *)&inBuf[numInputPairsInBuf]);
    if (CheckRestart(result))
      return capros_W1Bus_StatusCode_SysRestart;
    if (result != RC_OK) {
      if (result == RC_capros_SerialPort_TimedOut) {
        DEBUG(errors) kprintf(KR_OSTREAM,
                        "SerialPort_readTimeout timed out!\n");
      } else {
        DEBUG(errors) kprintf(KR_OSTREAM,
                        "SerialPort_readTimeout returned %#x\n", result);
        assert(false);
      }
      return capros_W1Bus_StatusCode_BusError;
    }
    // Check that the received characters are OK.
    unsigned int i;
    for (i = 0; i < pairsRcvd; i++) {
      if (inBuf[numInputPairsInBuf + i].flag
          != capros_SerialPort_Flag_NORMAL) {
        DEBUG(errors) kprintf(KR_OSTREAM,
                        "SerialPort_readTimeout inbuf[%d].flag is %d\n",
                        numInputPairsInBuf + i,
                        inBuf[numInputPairsInBuf + i].flag);
        return capros_W1Bus_StatusCode_BusError;
      }
    }
    numInputPairsInBuf += pairsRcvd;
  }
  return 0;
}

// Expecting exactly numChars characters.
unsigned int
RcvChars(unsigned int numChars)
{
  // Empty the buffer:
  numInputPairsProcessed = 0;
  numInputPairsInBuf = 0;

  int res = RcvCharsMin(numChars);
  if (res != 0)
    return res;
  if (numInputPairsInBuf > numChars) {
    DEBUG(errors) kprintf(KR_OSTREAM,
                    "RcvChars got %d extra chars\n",
                    numInputPairsInBuf - numChars);
    return capros_W1Bus_StatusCode_BusError;
  }
  return 0;
}

result_t
SendConfigCommand(uint8_t command)	// low bit of command is zero
{
  EnsureCommandMode();
  wp(command + 0x01);
  if (! serial_sendData()) {
    return RC_capros_key_Restart;
  }
  unsigned int err = RcvChars(1);
  if (err != 0) {
    return StatusCodeToException(err);
  }
  uint8_t c = inBuf[0].data;
  if (c != command) {
    DEBUG(errors) kprintf(KR_OSTREAM, "Config command sent %#.2x got %#.2x\n",
                          command, c);
    return RC_capros_W1Bus_BusError;
  }
  return RC_OK;
}

uint8_t * const startPgm = (void *)&MsgRcvBuf;
uint8_t * stepStart;	// start of the current step

void
ProgramDataByte(uint8_t c)
{
  EnsureDataMode();
  if (c == commandToCommandMode) {
    wp(c);  // commandToCommandMode must be doubled
  }
  wp(c);
}

void
ProgramDataBytes(uint8_t * p, unsigned int num)
{
  while (num--) {
    ProgramDataByte(*p++);
  }
}

void
ProgramReadBytes(unsigned int num)
{
  memset(&sendpacket[sendlen], 0xff, num);
  sendlen += num;
}

void
ProgramReset(void)
{
  EnsureCommandMode();
  wp(commandReset + gBusSpeed);
}

unsigned int duration;	// number of bit times for the bus activity

#define SPUDCode_unknown 255
unsigned int currentSPUDCode;	// 0 to 7
bool settingSPUD;

// If next step is strongPullup5, returns the SPUD parameter value (>= 0),
// else returns -1.
int
nextIsSPU5(uint8_t * pgm, uint8_t * endPgm)
{
  if (pgm + 2 > endPgm
      || *pgm != capros_W1Bus_stepCode_strongPullup5)
    return -1;
  // Next step is strongPullup5, but it might not be well-formed:
  uint8_t code = *(pgm + 1);	// duration, units of 16ms
  unsigned int param;
  // Binary search on the code:
  if (code <= 8) {
    if (code <= 1) {
      if (code == 0) {
        return -1;	// invalid code
      } else {
        param = 0;
      }
    } else {
      if (code <= 4) {
        param = 1;
      } else {
        param = 2;
      }
    }
  } else {
    if (code <= 32) {
      if (code <= 16) {
        param = 3;
      } else {
        param = 4;
      }
    } else {
      if (code <= 65) {
        param = 5;	// SPUD1048/2
      } else {
        // SPUDCode_unknown is invalid, and others are not supported.
        return -1;
      }
    }
  }
  // We must do a strong pullup with a duration defined by param.
  if (param != currentSPUDCode) {
    settingSPUD = true;
    currentSPUDCode = param;
    EnsureCommandMode();
    wp(configSPUD + 0x01 + (currentSPUDCode << 1));
  }
  // Duration of each SPUD in bit times
  static const unsigned int durations[6] = {
    16400/60,
    65500/60,
    131000/60,
    262000/60,
    524000/60,
    1048000/60
  };
  duration += durations[param];
  return param;
}

bool
MatchDataByte(uint8_t c)
{
  bool ret = c == inBuf[numInputPairsProcessed++].data;
  if (!ret)
    DEBUG(errors) kprintf(KR_OSTREAM, "Sent data byte %#.2x got %#.2x!\n",
                          c, inBuf[numInputPairsProcessed++].data);
  return ret;
}

bool
MatchDataBytes(const uint8_t * p, unsigned int num)
{
  for (; num > 0; num--) {
    bool ret = MatchDataByte(*p++);
    if (!ret)
      return ret;
  }
  return true;
}

unsigned int
CheckSPUDResponse(void)
{
  if (settingSPUD) {
    // Check the response from the SPUD command.
    unsigned int ret = RcvCharsMin(1);
    if (ret != 0)
      return ret;
    if (! MatchDataByte(configSPUD + (currentSPUDCode << 1)))
      return capros_W1Bus_StatusCode_BusError;
  }
  return 0;
}

unsigned int crc;

// crcType is 0x118 for CRC8 or 0x14002 for CRC16.
void
CalcCRCByte(uint8_t s, unsigned int crcType)
{
  int i;
  for (i = 0; i < 8; i++, s>>=1) {	// for each bit
    if ((s ^ crc) & 0x1)
      crc ^= crcType;	// calc CRC8 or CRC16
    crc >>= 1;
  }
}

void
RunProgram(Message * msg, uint32_t pgmLen)
{
  result_t result;
  unsigned long ret;

  unsigned char * endPgm = startPgm + pgmLen;
  unsigned char * pgm;	// where we are in the program
  pgm = startPgm;

  unsigned char * const resultStart = (void *)&MsgSndBuf;
  unsigned char * resultNext = resultStart;

  /* We divide the program into "segments"; at the end of each segment
  we need to check the response to make a decision
  or stop if there is an error. */

  while (pgm < endPgm) {	// process segments
    inDataModeAtBeginningOfSegment = inDataMode;
    uint16_t option;
    int param;
    unsigned char nBytes;
    uint8_t c;
    unsigned char * segStart = pgm;	// start of the current segment
    sendlen = 0;
    duration = 0;	// number of bit times for the bus activity
				// of this segment
    settingSPUD = false;

    while (pgm < endPgm) {	// process steps first pass
#define needPgm(n) if (pgm + (n) > endPgm) goto programError;
#define needEP2(n) if (sendlen + (n) > sendMaxSize) goto endSegment;
      stepStart = pgm;
      unsigned char stepCode = *pgm++;
      DEBUG(prog) kprintf(KR_OSTREAM, "stepCode %d", stepCode);
      switch (stepCode) {
      default:
        goto programError;

      case capros_W1Bus_stepCode_resetSimple:
        // Enforce restrictions for compatibility with DS2490.
        if (pgm >= endPgm)	// is this the last step in the program?
          goto sequenceError;
        switch (*pgm) {		// check next step
        case capros_W1Bus_stepCode_resetSimple:
        case capros_W1Bus_stepCode_resetNormal:
        case capros_W1Bus_stepCode_resetAny:
        case capros_W1Bus_stepCode_write0:
        case capros_W1Bus_stepCode_write1Read:
        case capros_W1Bus_stepCode_readCRC8:
        case capros_W1Bus_stepCode_readCRC16:
        case capros_W1Bus_stepCode_readUntil1:
          pgm++;		// give the error on the next command
          goto sequenceError;	// disallowed
        }
        /* Empirically, it does not work to combine this with subsequent steps;
        we must receive the reset response first. */
        // Fall into the other reset cases, which need to test the response.
      case capros_W1Bus_stepCode_resetAny:
      case capros_W1Bus_stepCode_resetNormal:
        ProgramReset();
        duration += 16;
        goto execute;

      case capros_W1Bus_stepCode_setPathMain:
      case capros_W1Bus_stepCode_setPathAux:
        needPgm(8)
	/* Do Smart-On explicitly. */
	/* Must send: match ROM, ROM, smart-on command, reset stimulus.
	Then read: reset response, confirmation byte. */
        needEP2(13)
        ProgramDataByte(0x55);	// Match ROM
        ProgramDataBytes(pgm, 8);
        pgm += 8;
        ProgramDataByte(stepCode);	// Smart-On command
        ProgramDataByte(0xff);	// reset stimulus
        ProgramReadBytes(2);	// reset response and confirmation byte
        goto execute;

      case capros_W1Bus_stepCode_skipROM:
        needEP2(1)
        ProgramDataByte(0xcc);	// skip ROM command
        duration += 8;
        break;

      case capros_W1Bus_stepCode_matchROM:
        needPgm(8)
        needEP2(9)
        ProgramDataByte(stepCode);	// Match ROM command
        ProgramDataBytes(pgm, 8);
        pgm += 8;
        duration += (1+8) * 8;
        break;

      case capros_W1Bus_stepCode_searchROM:
      case capros_W1Bus_stepCode_alarmSearchROM:
        needPgm(8)
        needEP2(8)
        ProgramDataByte(stepCode);
        EnsureCommandMode();
        wp(commandSearchAccelOn + gBusSpeed);
        int i, j;
        for (j = 0; j < 8; j++) {	// for each byte of ROM
          unsigned int s = 0;
          unsigned int c = *pgm++;
          for (i = 0; i < 8; i++) {	// for each bit
            s = (s >> 2) | ((c & 0x01) << 15);
            c >>= 1;
          }
          ProgramDataByte((uint8_t)s);
          ProgramDataByte(s >> 8);
        }
        EnsureCommandMode();
        wp(commandSearchAccelOff + gBusSpeed);
        duration += 8 + 64*3;
        // search can't go faster than 9600 baud:
        baudPreference = baudPref9600;
        goto execute;

      case capros_W1Bus_stepCode_write1Read:
        option = 0x10;
        goto writeCommon;

      case capros_W1Bus_stepCode_write0:
        option = 0;
      writeCommon:
        duration += 1;
        param = nextIsSPU5(pgm, endPgm);
        if (param >= 0) {
          pgm += 2;	// consume the strongPullup5 step
          EnsureCommandMode();
          wp(commandWrite0 + option + gBusSpeed + commandPulseArm);
          /* End this segment, to ensure we only need one pulse duration. */
          goto execute;
        } else {
          EnsureCommandMode();
          wp(commandWrite0 + option + gBusSpeed);
          break;
        }

      case capros_W1Bus_stepCode_writeBytes:
      {
        needPgm(1)
        nBytes = *pgm;
        if (nBytes == 0 || nBytes > capros_W1Bus_maxWriteSize)
          goto programError; // can never do this
        needPgm(1+nBytes)
        needEP2(nBytes)
        pgm++;
        // Put all but the last byte in the buffer.
        ProgramDataBytes(pgm, nBytes - 1);
        pgm += nBytes - 1;
        c = *pgm++;	// the last byte
        goto readWriteBytes;
      }

      case capros_W1Bus_stepCode_readBytes:
      {
        needPgm(1)
        nBytes = *pgm;
        if (nBytes == 0 || nBytes > capros_W1Bus_maxReadSize)
          goto programError; // can never do this
        needEP2(nBytes)
        pgm++;
        EnsureDataMode();
        ProgramReadBytes(nBytes - 1);
        c = 0xff;	// last byte
readWriteBytes:
        duration += nBytes * 8;
        param = nextIsSPU5(pgm, endPgm);
        if (param >= 0) {
          pgm += 2;	// consume the strongPullup5 step
          EnsureCommandMode();
          wp(commandPulse5VStrong + commandPulseArm);
          // Terminate the pulse (we only wanted to arm)
          wp(commandPulseTerminate);
          ProgramDataByte(c);
          /* End this segment, to ensure we only need one pulse duration. */
          goto execute;
        } else {
          ProgramDataByte(c);
          break;
        }
      }

      case capros_W1Bus_stepCode_readCRC8:
        option = 1;
        goto readCRC;

      case capros_W1Bus_stepCode_readCRC16:
        option = 2;	// bytes of CRC to receive
      readCRC:
        needPgm(5)
        needEP2(3)
      {
        unsigned char logPageSize = *pgm;
        if (logPageSize == 0 || logPageSize >= 7) goto programError;
        unsigned int pageSize = 1UL << logPageSize;
        unsigned char numPages = *(pgm+1);
        unsigned long nBytes = pageSize * numPages;
        if (nBytes > capros_W1Bus_maxReadSize)
          goto programError; // can never do this
        unsigned long nBytesRcvd = (pageSize + option) * numPages;
        needEP2(3 + nBytesRcvd);
        pgm += 2;
        // Send the preamble:
        ProgramDataBytes(pgm, 3);
        pgm += 3;
        duration += (3 + nBytesRcvd) * 8;
        ProgramReadBytes(nBytesRcvd);
        goto execute;
      }

      case capros_W1Bus_stepCode_readUntil1:
        needPgm(1)
        assert(!"implemented"); ////...
        break;

      case capros_W1Bus_stepCode_strongPullup5:
        // If this were valid, it would have been consumed by the previous step.
        goto sequenceError;

      }
    }
    goto execute;

// End this segment before the step we just examined.
endSegment:
    pgm--;	// undo increment to get stepCode

execute: ;
    // We have now preprocessed a segment. Execute it.
    unsigned char * segEnd = pgm;	// end of the current segment

    // Do we need or want to change the baud rate?
    if (baudPreference == baudPref9600) {
      // This segment has a search command, which must run at 9600 baud.
      if (currentBaudRate != 9600) {
        if (! ChangeBaudRate(9600, configRBR + RBR9600)) {
          assert(!"implemented");
        }
      }
    } else {
      if ((baudPreference -= sendlen) <= 0) {
        baudPreference = 0;	// prevent underflow
        if (currentBaudRate != 19200) {
          if (! ChangeBaudRate(19200, configRBR + RBR19200)) {
            assert(!"implemented");
          }
        }
      }
      // else not sending much, don't bother to change the rate now
    }

    capros_Sleep_nanoseconds_t startTime;
    result = capros_Sleep_getTimeMonotonic(KR_SLEEP, &startTime);
    assert(result == RC_OK);

    // The input buffer is empty:
    numInputPairsProcessed = 0;
    numInputPairsInBuf = 0;

    // We will go through the segment again, doing post-processing.
    pgm = segStart;

    // Send all the commands and data.
    if (! serial_sendData()) {
      pgm++;	// as expected by terminateSysRestart
      goto terminateSysRestart;
    }

    // Give it time to execute.
    // This is necessary in the case of a strong pullup.
    // In other cases it avoids polling the serial port getting partial results.
    startTime += duration * 60000;
    result = capros_Sleep_sleepTill(KR_SLEEP, startTime);
    assert(result == RC_OK || result == RC_capros_key_Restart);

    while (pgm < segEnd) {	// process steps first pass
      unsigned char stepCode = *pgm++;
      DEBUG(prog) kprintf(KR_OSTREAM, "Post-processing step %d\n", stepCode);
      switch (stepCode) {
      default:
        assert(false);

      case capros_W1Bus_stepCode_resetSimple:
      case capros_W1Bus_stepCode_resetAny:
      case capros_W1Bus_stepCode_resetNormal:
      {
        // get the response byte
        if ((ret = RcvCharsMin(1)) != 0)
          goto terminateGotRet;
        c = inBuf[numInputPairsProcessed++].data & 0xdf;
        if ((c & 0xfc) != 0xcc) {
          DEBUG(errors) kprintf(KR_OSTREAM, "Sent reset got %#.2x!\n", c);
          goto terminateBusError;
        }
        if (stepCode == capros_W1Bus_stepCode_resetSimple)
          break;
        uint8_t app;
        switch (c) {
        default:	// for the compiler; this can't happen
        case 0xcc:
          goto terminateShorted;
        case 0xcd:	// normal presence pulse
          app = 0;
          break;
        case 0xce:	// alarming presence pulse
          app = 4;
          break;
        case 0xcf:
          goto terminateNoDevice;
        }
        if (stepCode == capros_W1Bus_stepCode_resetAny) {
          // Report whether app or not.
          *resultNext++ = app;
        } else {	// resetNormal
          if (app) goto terminateAPP;
        }
        break;
      }

      case capros_W1Bus_stepCode_setPathMain:
      case capros_W1Bus_stepCode_setPathAux:
      {
        if ((ret = RcvCharsMin(12)) != 0)
          goto terminateGotRet;
        if (! MatchDataByte(0x55)	// Match ROM response
            || ! MatchDataBytes(pgm, 8)	// match ROM
            || ! MatchDataByte(stepCode)
            || ! MatchDataByte(0xff))	// reset stimulus
          goto terminateBusError;
        uint8_t resetResponse = inBuf[numInputPairsProcessed++].data;
        DEBUG(prog) kprintf(KR_OSTREAM, "ResetResponse %#x.\n", resetResponse);

        c = inBuf[numInputPairsProcessed++].data;	// confirmation byte
        if (c != stepCode) {
          if (c == (stepCode ^ 0xff))
            goto terminateBusShorted;
          else goto terminateBusError;
        }

	if (resetResponse & 0x80) {
          DEBUG(prog) kprintf(KR_OSTREAM, "No devices on %s branch.\n",
              stepCode == capros_W1Bus_stepCode_setPathMain ? "main" : "aux");
          goto terminateNoDevice;
        }
        pgm += 8;
        break;
      }

      case capros_W1Bus_stepCode_skipROM:
        if ((ret = RcvCharsMin(1)) != 0)
          goto terminateGotRet;
        if (! MatchDataByte(0xcc))	// Skip ROM response
          goto terminateBusError;
        break;

      case capros_W1Bus_stepCode_matchROM:
        if ((ret = RcvCharsMin(9)) != 0)
          goto terminateGotRet;
        if (! MatchDataByte(stepCode)	// Match ROM response
            || ! MatchDataBytes(pgm, 8))	// match ROM
          goto terminateBusError;
        pgm += 8;
        break;

      case capros_W1Bus_stepCode_searchROM:
      case capros_W1Bus_stepCode_alarmSearchROM:
        if ((ret = RcvCharsMin(17)) != 0)
          goto terminateGotRet;
        if (! MatchDataByte(stepCode))	// Search ROM response
          goto terminateBusError;
        uint8_t rom[8];
        uint8_t discrep[8];
        int i, j;
        for (j = 0; j < 8; j++) {       // for each byte of ROM
          unsigned int s = inBuf[numInputPairsProcessed++].data;
          unsigned int r = 0;
          unsigned int d = 0;
          s |= inBuf[numInputPairsProcessed++].data << 8;
          for (i = 0; i < 8; i++) {     // for each bit
            if (s & 0x2)
              r |= 0x100;
            if (s & 0x1)
              d |= 0x100;
            r >>= 1;
            d >>= 1;
            s >>= 2;
          }
          rom[j] = r;
          discrep[j] = d;
        }
        memcpy(resultNext, rom, 8);
        resultNext += 8;
        memcpy(resultNext, discrep, 8);
        resultNext += 8;

        pgm += 8;
        break;

      case capros_W1Bus_stepCode_write1Read:
        option = 0x10;
        goto writeCommonPP;

      case capros_W1Bus_stepCode_write0:
        option = 0;
      writeCommonPP:
        if (pgm + 1 < segEnd
            && *(pgm + 1) == capros_W1Bus_stepCode_strongPullup5) {
          if ((ret = CheckSPUDResponse()) != 0)
            goto terminateGotRet;
          option += commandPulseArm;
        }
        if ((ret = RcvCharsMin(1)) != 0)
          goto terminateGotRet;
        c = inBuf[numInputPairsProcessed++].data;
        option += commandWrite0 - 1 + gBusSpeed;	// expected response
        if (c != option
            && c != option + 0x3) {
          DEBUG(errors) kprintf(KR_OSTREAM, "Single bit op got %#.2x!\n", c);
          goto terminateBusError;
        }
        if (stepCode == capros_W1Bus_stepCode_write1Read) {
          *resultNext++ = c & 0x01;
        }
        break;

      case capros_W1Bus_stepCode_writeBytes:
      case capros_W1Bus_stepCode_readBytes:
        nBytes = *pgm;
        if ((ret = RcvCharsMin(nBytes)) != 0)
          goto terminateGotRet;
        uint8_t * nextStep = pgm + 1;
        if (stepCode == capros_W1Bus_stepCode_writeBytes) {
          if (! MatchDataBytes(pgm+1, nBytes-1))
            goto terminateBusError;
          nextStep += nBytes;
        } else {		// readBytes
          int i;
          for (i = 0; i < nBytes-1; i++) {
            *resultNext++ = inBuf[numInputPairsProcessed++].data;
          }
        }

        if (nextStep < segEnd
            && *nextStep == capros_W1Bus_stepCode_strongPullup5) {

          if ((ret = CheckSPUDResponse()) != 0)
            goto terminateGotRet;
          if ((ret = RcvCharsMin(3)) != 0)
            goto terminateGotRet;
          // Check response to pulse arm.
          c = inBuf[numInputPairsProcessed++].data;
          if ((c & 0xfc) != (commandPulse5VStrong & 0xfc)) {
            assert(!"implemented");
          }
          // Process the last data byte.
          if (stepCode == capros_W1Bus_stepCode_writeBytes) {
            if (! MatchDataByte(*(pgm + nBytes)))
              goto terminateBusError;
          } else {		// readBytes
            *resultNext++ = inBuf[numInputPairsProcessed++].data;
          }
          // Check the end of pulse signal.
          c = inBuf[numInputPairsProcessed++].data;
          if ((c & 0x7f) != 0x76) {
            assert(!"implemented");
          }
          if (numInputPairsProcessed < numInputPairsInBuf) {
            assert(!"implemented");	// extra characters in inBuf
          }
          // The strong pullup is still armed. Disarm it now.
          EnsureCommandMode();
          wp(commandPulse5VStrong);	// pulse and disarm
          wp(commandPulseTerminate);	// terminate the pulse, leave disarmed
          if (! serial_sendData())
            goto terminateSysRestart;
          if ((ret = RcvCharsMin(1)) != 0)
            goto terminateGotRet;
          // Check response to pulse disarm.
          c = inBuf[numInputPairsProcessed++].data;
          if ((c & 0xfc) != (commandPulse5VStrong & 0xfc)) {
            assert(!"implemented");
          }

        } else {		// no pullup

          if (stepCode == capros_W1Bus_stepCode_writeBytes) {
            if (! MatchDataByte(*(pgm + nBytes)))
              goto terminateBusError;
          } else {		// readBytes
            *resultNext++ = inBuf[numInputPairsProcessed++].data;
          }

        }
        pgm = nextStep;
        break;

      {
      case capros_W1Bus_stepCode_readCRC8:
        option = 1;
        unsigned int crcType = 0x118;
        goto readCRCPP;

      case capros_W1Bus_stepCode_readCRC16:
        option = 2;
        crcType = 0x14002;
      readCRCPP: ;
        unsigned int pageSize = (1UL << *pgm);
        unsigned char numPages = *(pgm+1);
        unsigned long nBytesRcvd = (pageSize + option) * numPages;
        if ((ret = RcvCharsMin(3 + nBytesRcvd)) != 0)
          goto terminateGotRet;
        if (! MatchDataBytes(pgm+2, 3))	// preamble
          goto terminateBusError;
        // Check CRC.
        crc = 0;
        // First CRC includes the preamble:
        CalcCRCByte(*(pgm+2), crcType);
        CalcCRCByte(*(pgm+3), crcType);
        CalcCRCByte(*(pgm+4), crcType);
        int j, k;
        for (k = 0; k < numPages; k++) {	// for each page
          unsigned int s;
          for (j = 0; j < pageSize; j++) {	// for each byte of data
            s = inBuf[numInputPairsProcessed++].data;
            *resultNext++ = s;
            CalcCRCByte(s, crcType);
          }
          // Get the CRC
          s = inBuf[numInputPairsProcessed++].data;
          if (option > 1) {	// CRC16
            s |= inBuf[numInputPairsProcessed++].data << 8;
            s = s ^ 0xffff;	// CRC16 is returned complemented
          }
          if (crc != s) {
            DEBUG(errors) kprintf(KR_OSTREAM, "CRC%d calc %#x read %#x!\n",
                            option * 8, crc, s);
            goto terminateCRCError;
          }
          crc = 0;	// set up for next page
        }
        pgm += 5;
        break;
      }

      case capros_W1Bus_stepCode_readUntil1:
        assert(!"implemented");
        break;

      case capros_W1Bus_stepCode_strongPullup5:
        pgm++;
        break;
      }
    }
    // Done postprocessing this segment.
    if (numInputPairsProcessed < numInputPairsInBuf) {
      assert(!"implemented");	// extra characters in inBuf
    }

    continue;	// process any other segments

sequenceError:
    ret = capros_W1Bus_StatusCode_SequenceError;
    goto preprocessError;

programError:
    ret = capros_W1Bus_StatusCode_ProgramError;
preprocessError:
    // None of the current segment was executed.
    msg->snd_w1 = ret;
    // Segments before this one were executed successfully.
    msg->snd_w2 = segStart - startPgm;
    msg->snd_w3 = stepStart - startPgm;
    return;
  }
  // Processed all segments.
  ret = capros_W1Bus_StatusCode_OK;
  msg->snd_w2 = pgm - startPgm;
  goto returnOK;

terminateShorted:
  ret = capros_W1Bus_StatusCode_BusShorted;
  goto returnLength;

terminateNoDevice:
  ret = capros_W1Bus_StatusCode_NoDevicePresent;
  goto returnLength;

terminateAPP:
  ret = capros_W1Bus_StatusCode_AlarmingPresencePulse;
  goto returnLength;

terminateBusShorted:
  ret = capros_W1Bus_StatusCode_BusShorted;
  goto returnLengthGotEP3;

terminateBusError:
  ret = capros_W1Bus_StatusCode_BusError;
  goto returnLengthGotEP3;

terminateCRCError:
  ret = capros_W1Bus_StatusCode_CRCError;
  goto returnLength;

returnLength:
  if (! SerialIn_Flush())
    goto terminateSysRestart;
  goto returnLengthGotEP3;

terminateSysRestart:
  ret = capros_W1Bus_StatusCode_SysRestart;
  goto returnLengthGotEP3;

terminateGotRet:
returnLengthGotEP3:
  msg->snd_w2 = pgm - 1 - startPgm;	// pgm successfully executed
returnOK:
  msg->snd_w1 = ret;
  msg->snd_w3 = pgm - startPgm;
  msg->snd_data = resultStart;
  msg->snd_len = resultNext - resultStart;
  // FIXME: need to check if the results are too long for our buffer.
  return;
}

/* Returns one of:
RC_OK
RC_capros_key_Restart
RC_capros_W1Bus_BusError
*/
result_t
DS2480B_Init(void)
{
  result_t result;
  haveSerial = true;
  inDataMode = false;
  sendlen = 0;
  numInputPairsProcessed = 0;
  numInputPairsInBuf = 0;
  currentSPUDCode = SPUDCode_unknown;
  currentBaudRate = 9600;
  baudPreference = baudPref9600;	// because likely to do a search soon
  gBusSpeed = BusSpeedFlexible;

  // Initialize the DS2480B:
  DEBUG(init) kprintf(KR_OSTREAM, "DS2480B opening serial port...");
  uint32_t openErr;
  result = capros_SerialPort_open(KR_Serial, &openErr);
  if (CheckRestart(result))
    goto restarted;
  assert(result == RC_OK);
  DEBUG(init) kprintf(KR_OSTREAM, " done.\n");

  int triesLeft = 3;
  while (triesLeft--) {
    if (! SerialOut_Init(4800))
      goto restarted;
    // A NUL at 4800 baud is seen as a break at 9600 baud.
    wp(0);
    if (! serial_sendData())
      goto restarted;
    if (! SerialOut_Flush())
      goto restarted;
    msDelay(5);
    if (! SerialOut_Init(9600))
      goto restarted;

    // Send initial Reset command after master reset for timing
    wp(commandReset + gBusSpeed);
    if (! serial_sendData())
      goto restarted;
    if (! SerialOut_Flush())
      goto restarted;
    if (! SerialIn_Flush())
      goto restarted;

    // Read the baud rate (to test the command block)
    wp((configRBR >> 3) + 0x01);
    // Do 1 bit operation (to test 1-Wire block)
    wp(commandWrite1 + gBusSpeed);

    if (! serial_sendData())
      goto restarted;
    unsigned int err = RcvChars(2);
    switch (err) {
    default:	// capros_W1Bus_StatusCode_BusError
      DEBUG(errors) kprintf(KR_OSTREAM, "RcvChars got %d! triesLeft=%d\n",
                            err, triesLeft);
      continue;		// try again

    case capros_W1Bus_StatusCode_SysRestart:
      goto restarted;

    case 0: break;
    }
    if (inBuf[0].data != RBR9600) {
      DEBUG(errors) kprintf(KR_OSTREAM, "DS2480B read baud rate got %#.2x! inbuf %#x\n",
                            inBuf[0].data, inBuf);
      continue;		// try again
    }
    if ((inBuf[1].data & 0xfc) != ((commandWrite1 + gBusSpeed) & 0xfc)) {
      assert(!"implemented");
    }
    return RC_OK;	// success
  }
  DEBUG(errors) kdprintf(KR_OSTREAM, "DS2480B_Init failed!\n");
  return RC_capros_W1Bus_BusError;

restarted:
  return RC_capros_key_Restart;
}

int
main(void)
{
  result_t result;
  Message Msg = {
    .snd_key0 = KR_VOID,
    .snd_key1 = KR_VOID,
    .snd_key2 = KR_VOID,
    .snd_rsmkey = KR_VOID,
    .snd_len = 0
  };
  DEBUG(init) kdprintf(KR_OSTREAM, "DS2480B started.\n");

  Msg.snd_invKey = KR_VOID;

  for (;;) {
    Msg.rcv_key0 = KR_ARG(0);
    Msg.rcv_key1 = KR_VOID;
    Msg.rcv_key2 = KR_VOID;
    Msg.rcv_rsmkey = KR_RETURN;
    Msg.rcv_data = &MsgRcvBuf;
    Msg.rcv_limit = sizeof(MsgRcvBuf);

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

    switch (Msg.rcv_keyInfo) {
    case keyInfo_nplinkee:
      DEBUG(init) kprintf(KR_OSTREAM, "DS2480B got serial port.\n");
      assert(Msg.rcv_code == OC_capros_NPLinkee_registerNPCap);

      COPY_KEYREG(KR_ARG(0), KR_Serial);

      // Return to nplink before using the serial port.
      SEND(&Msg);

      if (DS2480B_Init() != RC_OK) {
        // Serial port is broken, or gone already.
        haveSerial = false;
        break;
      }

      // Notify W1Mult:
      RescindAnyW1BusCap();
      result = capros_SpaceBank_alloc1(KR_BANK,
                 capros_Range_otForwarder, KR_Forwarder);
      assert(result == RC_OK);	// FIXME handle allocation failure
      result = capros_Process_makeStartKey(KR_SELF, keyInfo_W1Bus, KR_TEMP0);
      assert(result == RC_OK);
      result = capros_Forwarder_swapTarget(KR_Forwarder, KR_TEMP0, KR_VOID);
      assert(result == RC_OK);
      result = capros_Forwarder_getOpaqueForwarder(KR_Forwarder, 0, KR_TEMP0);
      assert(result == RC_OK);
      result = capros_W1Mult_registerBus(KR_W1MULT, KR_TEMP0, 0);
      assert(result == RC_OK);
      break;

    case keyInfo_W1Bus:
      switch (Msg.rcv_code) {
      default:
        Msg.snd_code = RC_capros_key_UnknownRequest;
        break;

      case OC_capros_key_getType:
        Msg.snd_w1 = IKT_capros_W1Bus;
        break;
  
      case 3:	// runProgram
        if (Msg.rcv_sent > Msg.rcv_limit) {	// he sent too much
          Msg.snd_code = RC_capros_W1Bus_ProgramTooLong;
          break;
        }
        RunProgram(&Msg, Msg.rcv_sent);
        break;
  
      case OC_capros_W1Bus_waitForDisconnect:
        assert(!"implemented");	//// FIXME
        break;
  
      case OC_capros_W1Bus_resetDevice:
        Msg.snd_code = DS2480B_Init();
        break;
  
      case OC_capros_W1Bus_setSpeed:
        if (Msg.rcv_w1 > capros_W1Bus_W1Speed_overdrive) {
          Msg.snd_code = RC_capros_key_RequestError;
          break;
        }
        gBusSpeed = Msg.rcv_w1 << 2;
        // The speed will be set on the next communication command, e.g. reset.
        break;
  
      case OC_capros_W1Bus_setPDSR:
        if (Msg.rcv_w1 > capros_W1Bus_PDSR_PDSR055) {
          Msg.snd_code = RC_capros_key_RequestError;
          break;
        }
        Msg.snd_code =  SendConfigCommand(configPDSRC + (Msg.rcv_w1 << 1));
        break;
  
      case OC_capros_W1Bus_setW1LT:
        if (Msg.rcv_w1 > capros_W1Bus_W1LT_W1LT15) {
          Msg.snd_code = RC_capros_key_RequestError;
          break;
        }
        Msg.snd_code = SendConfigCommand(configW1LT + (Msg.rcv_w1 << 1));
        break;
  
      case OC_capros_W1Bus_setDSO:
        if (Msg.rcv_w1 > capros_W1Bus_DSO_DSO10) {
          Msg.snd_code = RC_capros_key_RequestError;
          break;
        }
        Msg.snd_code = SendConfigCommand(configDSO + (Msg.rcv_w1 << 1));
        break;
  
      }
      break;
    }
  }
}
