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

#include <stdint.h>
#include <eros/Link.h>
#include <eros/Invoke.h>
#include <idl/capros/Sleep.h>
#include <idl/capros/W1Bus.h>
#include <domain/Runtime.h>

#define dbg_errors 0x1
#define dbg_search 0x2

/* Following should be an OR of some of the above */
#define dbg_flags   ( 0u )

#define DEBUG(x) if (dbg_##x & dbg_flags)

#define KR_OSTREAM KR_APP(0)
#define KR_SLEEP   KR_APP(1)
#define KR_W1BUS   KR_APP(2)
#define KR_SNODE   KR_APP(3)

// The family code is the low byte of the ROM ID.
enum {
  famCode_DS18B20 = 0x28,	// temperature
  famCode_DS2409  = 0x1f,	// coupler2438
  famCode_DS2438  = 0x26,	// battery monitor
  famCode_DS2450  = 0x20,	// A-D
  famCode_DS2502  = 0x09,	// a DS9097U has one of these
  famCode_DS9490R = 0x81,	// custom DS2401 in a DS9490R
};

struct W1Device {
  uint64_t rom;
  uint64_t busyUntil;	// device is busy until this time
  struct W1Device * parent;    // parent coupler or NULL
  uint8_t mainOrAux;            // which branch of parent
  Link workQueueLink;
  struct W1Device * nextInActiveList;
  bool callerWaiting;	// whether snode slot has a resume key for this dev
  bool found;	// temporary flag for searching
  union {	// data specific to the type of device
    struct {
      uint8_t activeBranch;	// enumerated above
      bool mainNeedsWork;
      bool auxNeedsWork;
    } coupler;
    struct {
      int16_t temperature;	// in units of 1/16 degree Celsius
      uint8_t resolution;	// 1 through 4, bits after the binary point
      capros_Sleep_nanoseconds_t time;	// time at which temperature was the above
		// does this need to be in absolute real time?
    } thermom;
  } u;
};

// Stuff for programming the 1-Wire bus:
extern unsigned char outBuf[capros_W1Bus_maxProgramSize + 1];
extern unsigned char * const outBeg;
extern unsigned char * outCursor;
extern unsigned char inBuf[capros_W1Bus_maxResultsSize];
extern Message RunPgmMsg;

// Append a byte to the program.
#define wp(b) *outCursor++ = (b);

uint64_t GetCurrentTime(void);
uint8_t CalcCRC8(uint8_t * data, unsigned int len);
int RunProgram(void);
void AddressDevice(struct W1Device * dev);

static inline void
ClearProgram(void)
{
  outCursor = outBeg;
}
