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

#include <kerninc/kernel.h>
#include <kerninc/Machine.h>
#include <kerninc/SysTimer.h>
#include <kerninc/mach-rtc.h>
#include "../kernel/ep93xx-gpio.h"
#include "../kernel/mach-ep93xx.c"	// get RtcRead and RtcSet

/* Declarations for the DS1337 battery-backed serial real time clock
 * on the EDB9315 board. 
 * (Battery must be connected to jumper JP26.) */

#define DS1337_ADDR 0xd0
#define DS1337_READ 0x01

struct DS1337Registers {
  uint8_t seconds;
  uint8_t minutes;
  uint8_t hours;
  uint8_t day;
  uint8_t date;
  uint8_t month;
  uint8_t year;
  uint8_t A1Seconds;
  uint8_t A1Minutes;
  uint8_t A1Hour;
  uint8_t A1DayDate;
  uint8_t A2Minutes;
  uint8_t A2Hour;
  uint8_t A2DayDate;
  uint8_t control;
  uint8_t status;
};

/* Timing constants:
Maximum clock frequency is 400kHz, so minimum period is 2500 ns. */
#define tLow (1710/62)	// 1710 ns, in units of 62.5 ns
#define tHigh (790/62)	//  790 ns, in units of 62.5 ns


/****************************  GPIO stuff  ****************************/

#define GPIO (GPIOStruct(APB_VA + GPIO_APB_OFS))

// EECLK is the DS1337 clock SCL.
// EEDAT is the DS1337 data SDA.

// Bits in GPIO Port G:
#define EECLKBit 0x01
#define EEDATBit 0x02

static void
GPIOInit(void)
{
  GPIO.EEDrive = 0x0;	// Set EECLK and EEDAT to normal CMOS drivers.
  GPIO.PGDDR |= EECLKBit;	// EECLK is an output
}

static inline void
SetClkZero(void)
{
  GPIO.PGDR &= ~ EECLKBit;
}

static inline void
SetClkOne(void)
{
  GPIO.PGDR |= EECLKBit;
}

// Set EEDAT to be an output.
static inline void
SetDataOutput(void)
{
  GPIO.PGDDR |= EEDATBit;
}

// Set EEDAT to be an input.
static inline void
SetDataInput(void)
{
  GPIO.PGDDR &= ~ EEDATBit;
}

static inline void
SetDataZero(void)
{
  GPIO.PGDR &= ~ EEDATBit;
}

static inline void
SetDataOne(void)
{
  GPIO.PGDR |= EEDATBit;
}

static int	// returns 0 or 1
ReadData(void)
{
  return BoolToBit(GPIO.PGDR & EEDATBit);
}

static void
SendByte(unsigned int byte)
{
  int i;
  SetDataOutput();
  for (i = 8; i-- > 0; ) {
    // CLK has just gone low.
    if (byte & 0x80)
      SetDataOne();
    else
      SetDataZero();
    SpinWait62ns(tLow);	// this is longer than tSU:DAT
    SetClkOne();
    SpinWait62ns(tHigh);
    SetClkZero();
    
    byte <<= 1;
  }

  // Receive the acknowledge bit.
  SetDataInput();
  SpinWait62ns(tLow);
  SetClkOne();
  SpinWait62ns(tHigh);
  int ack = ReadData();
  SetClkZero();
  if (ack)
    dprintf(true, "DS1337 did not acknowledge!\n");
}

static uint8_t
ReadByte(bool lastByte)
{
  int i;
  int byte = 0;
  SetDataInput();
  for (i = 8; i-- > 0; ) {
    // CLK has just gone low.
    SpinWait62ns(tLow);
    SetClkOne();
    SpinWait62ns(tHigh);
    int bit = ReadData();
    SetClkZero();
    
    byte = (byte << 1) | bit;
  }

  // Generate the acknowledge bit.
  SetDataOutput();
  if (lastByte)
    SetDataOne();
  else
    SetDataZero();
  SpinWait62ns(tLow);
  SetClkOne();
  SpinWait62ns(tHigh);
  SetClkZero();

  return byte;
}

static void
CheckEEDATHigh(const char * where)
{
  // clk is high here.
  SetDataInput();
  if (ReadData() == 0) {
    printf("Waiting for EEDAT at %s ...", where);
    while (ReadData() == 0) {
      printf(".");
      assert(! (GPIO.PGDDR & EEDATBit));
      SetClkZero();
      SpinWait62ns(tLow);
      SetClkOne();
      SpinWait62ns(tHigh);
    }
    printf(" done\n");
  }
}

static void
DS1337Init(void)
{
  GPIOInit();
  SetClkOne();
  CheckEEDATHigh("Init");
  SetDataOutput();
  SetDataOne();		// keep EEDAT high

  // First do a write to set the register pointer.

  // Issue a START
  SetDataZero();	// while CLK is high
  SpinWait62ns(600/62);
  SetClkZero();

  // Address the device
  SendByte(DS1337_ADDR);
  // Caller will send the register address.
}

static void
SendSTOP(void)
{
  // Issue a STOP
  SpinWait62ns(900/62);	// max tHD:DAT
  SetDataOutput();
  SetDataZero();
  SpinWait62ns(tLow - 900/62);	// this is > tSU:DAT
  SetClkOne();
  SpinWait62ns(600/62);	// tSU:STO
  SetDataOne();		// while CLK is high
  SpinWait62ns(1300/62);	// tBUF
  CheckEEDATHigh("STOP");
}

/* On the EDB9315 RtcInit loads the EP9315's RTC from the EDB9315's
 * battery-backed RTC, which is a DS1337. */
/* This takes about a millisecond to execute. */
int
RtcInit(void)
{
  struct DS1337Registers regs;

  DS1337Init();
  // Send register address
  SendByte(0);

  // Issue a repeated START
  SetDataOutput();
  SetDataOne();
  SpinWait62ns(tLow);
  SetClkOne();
  SpinWait62ns(600/62);	// tSU:STA
  SetDataZero();	// while CLK is high
  SpinWait62ns(600/62);	// tHD:STA
  SetClkZero();

  // Address the device
  SendByte(DS1337_ADDR + DS1337_READ);

  // Read the time from the DS1337.
  int i;
  uint8_t * bp = &regs.seconds;
  for (i = 7; i > 0; i--) {
    *bp++ = ReadByte(!i);
  }

  SendSTOP();

  uint32_t newTime = kernMktime(BCDToBin(regs.year) + 2000,
    BCDToBin(regs.month & 0x1f),
    BCDToBin(regs.date),
    BCDToBin(regs.hours),
    BCDToBin(regs.minutes),
    BCDToBin(regs.seconds) );

#if 0
  dprintf(false, "Read RTC %02x %02x %02x %02x %02x %02x %02x calc %u\n",
    regs.seconds, regs.minutes, regs.hours, regs.day,
    regs.date, regs.month, regs.year, newTime );
#endif

  void RtcBootSet(uint32_t newTime);
  RtcBootSet(newTime);

  return 0;
}

/* On the EDB9315 RtcSave saves the specified time to the EDB9315's
 * battery-backed RTC, which is a DS1337. */
/* This takes about a millisecond to execute. */
int
RtcSave(capros_RTC_time_t newTime)
{
  int i;
  struct DS1337Registers regs;

  // Convert newTime to regs.
  struct kernTm tm;
  kernGmtime(newTime, &tm);

  regs.seconds = BinToBCD(tm.seconds);
  regs.minutes = BinToBCD(tm.minutes);
  regs.hours = BinToBCD(tm.hours);
  regs.date = BinToBCD(tm.date);
  regs.month = BinToBCD(tm.month);
  int year = tm.year - 2000;
  if (! (year >= 0 && year < 100))	// only for years 20xx
    return -1;
  regs.year = BinToBCD(year % 100);
  regs.day = 1;		// we don't care about this, but make it valid

  DS1337Init();
  // Send register address
  SendByte(0x0e);

  SendByte(0x04);	// Control Register; enable oscillator, disable sq wv
  SendByte(0);		// Clear OSF in Status Register; data are valid

  // Send the time to the DS1337.
  uint8_t * bp = &regs.seconds;
#define WORKAROUND	// Workaround for a bizarre bug somewhere
#ifdef WORKAROUND
  uint8_t save[7], *sp = &save[0];
#endif
  for (i = 7; i > 0; i--) {
#ifdef WORKAROUND
    uint8_t c = *bp;
    *sp++=c; 
#endif
    SendByte(*bp++);
  }

#if 0
  sp = &save[0];
  for (i = 7; i > 0; i--) {
    printf(" %02x",  *sp++);
  }
#endif

#if 0
  dprintf(false, "Set RTC %02x %02x %02x %02x %02x %02x %02x\n",
    regs.seconds, regs.minutes, regs.hours, regs.day,
    regs.date, regs.month, regs.year );
#endif

  SendSTOP();

  return 0;
}
