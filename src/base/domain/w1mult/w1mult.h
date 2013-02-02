/*
 * Copyright (C) 2008-2011, 2013, Strawberry Development Group.
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
#include <stddef.h>
#include <string.h>
#include <eros/Link.h>
#include <eros/Invoke.h>
#include <idl/capros/Node.h>
#include <idl/capros/Sleep.h>
#include <idl/capros/W1Bus.h>
#include <idl/capros/RTC.h>
#include <idl/capros/Logfile.h>
#include <idl/capros/DS2450.h>
#include <domain/Runtime.h>

#define dbg_errors 0x1
#define dbg_search 0x2
#define dbg_doall  0x4
#define dbg_server 0x8
#define dbg_timer 0x10
#define dbg_thermom 0x20
#define dbg_ad    0x40
#define dbg_bm    0x80
#define dbg_gpio8 0x100

/* Following should be an OR of some of the above */
#define dbg_flags   ( 0u | dbg_errors )

#define DEBUG(x) if (dbg_##x & dbg_flags)

#define KC_SNODEC 0
#define KC_LOGFILEC 1

#define KR_OSTREAM KR_APP(0)
#define KR_SLEEP   KR_APP(1)
#define KR_RTC     KR_APP(2)
#define KR_TIMPROC KR_APP(3)
#define KR_W1BUS   KR_APP(4)
#define KR_TIMRESUME KR_APP(5)
#define KR_NEXTW1BUS KR_APP(6)

// The family code is the low byte of the ROM ID.
enum {
  famCode_DS18B20 = 0x28,	// temperature
  famCode_DS2408  = 0x29,	// 8-channel programmable I/O
  famCode_DS2409  = 0x1f,	// coupler
  famCode_DS2438  = 0x26,	// battery monitor
  famCode_DS2450  = 0x20,	// A-D
  famCode_DS2502  = 0x09,	// a DS9097U has one of these
  famCode_DS9490R = 0x81,	// custom DS2401 in a DS9490R
};

#define NOLOG (-1)
struct HystLog {
  int hysteresis;
  int hysteresisLow;
  int32_t logSlot;
};

struct W1Device;

enum {
  branchRoot = 0,
  branchUnknown = 1,
  branchMain = capros_W1Bus_stepCode_setPathMain,
  branchAux  = capros_W1Bus_stepCode_setPathAux
};

struct Branch {
  struct W1Device * childCouplers;
  struct W1Device * childDevices;	// other than couplers
  capros_W1Bus_stepCode whichBranch;	// One of:
			// branchMain, branchAux, or branchRoot
  bool shorted;
  bool needsWork;
};

struct W1Device {
  uint64_t rom;
  struct Branch * parentBranch;	// branch this device is connected to
  struct W1Device * nextChild;
  struct W1Device * nextInSamplingList;
  struct W1Device * nextInWorkList;
  bool configured;
  bool onWorkList;
  bool sampling;
  bool callerWaiting;	// whether snode slot has a resume key for this dev
  bool found;		// device has been found on the network
  union {	// data specific to the type of device
    struct {
      capros_W1Bus_stepCode activeBranch;	// One of:
			// branchMain, branchAux, or branchUnknown
      struct Branch mainBranch;
      struct Branch auxBranch;
    } coupler;
    struct {
      Link samplingQueueLink;
      int32_t logSlot;	// slot in KR_KEYSTORE with Logfile
      uint8_t resolution;	// 1 through 4, bits after the binary point
			// 255 means the resolution hasn't been specified yet
      uint8_t spadConfig;	// config register in SPAD
      uint8_t eepromConfig;	// config register in EEPROM
      uint16_t hysteresis;
      int hysteresisLow;
    } thermom;
    struct {		// DS2408 8-channel programmable I/O
      uint8_t outputState;
    } pio8;
    struct {
      Link samplingQueueLink;
      /* requestedCfg consists of four pairs of bytes; in each pair,
         the first byte is cfglo and the second byte is cfghi. 
         Likewise for devCfg. */
      uint8_t requestedCfg[8];
      uint8_t devCfg[8];
      struct {
        struct HystLog HL;
        capros_DS2450_portConfiguration lastConfig;
      } port[4];
    } ad;
    struct {		// DS2438 battery monitor
      Link tSamplingQueueLink;
      bool tSampled;		// was sampled this heartbeat
      capros_Sleep_nanoseconds_t tSampledTime;
      capros_RTC_time_t tSampledRTC;
      int16_t tempResolutionMask;
      struct HystLog tempHL;

      Link vSamplingQueueLink;
      bool vSampled;		// was sampled this heartbeat
      uint16_t voltSelect;	// 0 for Vad, 1 for Vdd
      int16_t voltResolutionMask;
      struct HystLog voltHL;

      Link cSamplingQueueLink;
      bool cSampled;		// was sampled this heartbeat
      int16_t currentResolutionMask;
      struct HystLog currentHL;

      uint8_t configReg;
      uint8_t threshReg;
    } bm;
  } u;
};

struct w1Timer {
  capros_Sleep_nanoseconds_t expiration;
  Link link;	// link in timerHead
  // On expiration, we call function(arg)
  void * arg;
  void (*function)(void * arg);	
};

extern bool busNeedsReinit;

#define NO_SNODE_SLOT ((int)-1)
result_t CreateLog(int32_t * pSlot);
void GetLogfile(unsigned int slot);
result_t EnsureLog(int32_t * pSlot);
bool AddLogRecord16(unsigned int slot,
  capros_RTC_time_t sampledRTC,
  capros_Sleep_nanoseconds_t sampledTime,
  int16_t value, int16_t param);
void HystLog16_log(struct HystLog * hl, int value,
  capros_RTC_time_t rtc, capros_Sleep_nanoseconds_t timens, int param);

struct W1Device * BranchToCoupler(struct Branch * br);

// Bits in heartbeatDisable (reasons not to initiate a heartbeat):
#define hbBit_bus     0x01	// we have no bus key and no next bus key
#define hbBit_timer   0x02	// heartbeatTimer is running
#define hbBit_DS18B20 0x04
#define hbBit_DS2450  0x08
void DisableHeartbeat(uint32_t bit);
void EnableHeartbeat(uint32_t bit);

// Stuff for programming the 1-Wire bus:
extern unsigned char outBuf[capros_W1Bus_maxProgramSize + 1];
extern unsigned char * const outBeg;
extern unsigned char * outCursor;
extern unsigned char inBuf[capros_W1Bus_maxReadSize];
extern struct Branch * resetBranch;
extern Message RunPgmMsg;

// Append a byte to the program.
#define wp(b) *outCursor++ = (b);

extern unsigned int crc;
uint8_t CalcCRC8(uint8_t * data, unsigned int len);
void ProgramByteCRC16(uint8_t c);
bool CheckCRC16(void);

void ClearProgram(void);
void ProgramReset(void);
void AddressDevice(struct W1Device * dev);
void ProgramMatchROM(struct W1Device * dev);
void WriteOneByte(uint8_t b);
int RunProgram(void);
void AllDevsNotFound(void);

static inline void
SetBusNeedsReinit(void)
{
  busNeedsReinit = true;
  // AllDevsNotFound();
}

static inline bool
ProgramIsClear(void)
{
  return outCursor == outBeg;
}

static inline void
CopyToProgram(void * data, unsigned int len)
{
  memcpy(outCursor, data, len);
  outCursor += len;
}

// Indicate that the program does not end with a reset.
static inline void
NotReset(void)
{
  resetBranch = NULL;
}

extern capros_Sleep_nanoseconds_t currentTime;
extern capros_RTC_time_t currentRTC;
void RecordCurrentTime(void);
void RecordCurrentRTC(void);

void InsertTimer(struct w1Timer * timer);

/* latestConvertTTime is the time of the most recent Convert T command
 * that was broadcast (that is, send with Skip ROM). */
extern capros_Sleep_nanoseconds_t latestConvertTTime;

/* We will sample at least every 2**7 seconds (about 2 minutes). */
#define maxLog2Seconds 7

extern struct Branch root;

void ReMarkForSampling(uint32_t hbCount, Link * samplingQueue,
  struct W1Device * * samplingListHead,
  size_t devLinkOffset);
void MarkForSampling(uint32_t hbCount, Link * samplingQueue,
  struct W1Device * * samplingListHead,
  size_t devLinkOffset);
void MarkSamplingList(struct W1Device * dev);
void UnmarkSamplingList(struct W1Device * dev);
extern void (*DoAllWorkFunction)(struct Branch * br);
int DoAll(struct Branch * br);
extern void (*DoEachWorkFunction)(struct W1Device * dev);
void DoEach(struct Branch * br);
void EnsureBranchSmartReset(struct Branch * br);
