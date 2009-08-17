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

/* SWCA
   This persistent program receives a SerialPort capability for a serial port
   connected to a Trace/Xantrex SW Communications Adapter (SWCA)
   attached to one or more Trace/Xantrex SW series inverter/chargers.
   This program monitors and controls the inverters
   using the undocumented protocol used by the swcps program.
 */

#include <stdint.h>
#include <string.h>
#include <ctype.h>
#include <limits.h>
#include <domain/cmte.h>
#include <domain/CMTESemaphore.h>
#include <domain/CMTETimer.h>
#include <domain/CMTEThread.h>
#include <eros/target.h>
#include <eros/machine/cap-instr.h>
#include <eros/Invoke.h>
#include <idl/capros/key.h>
#include <idl/capros/Node.h>
#include <idl/capros/Process.h>
#include <idl/capros/Sleep.h>
#define infiniteTime 18446744073709000000ULL
#include <idl/capros/Constructor.h>
#include <idl/capros/SuperNode.h>
#include <idl/capros/SerialPort.h>
#include <idl/capros/NPLinkee.h>
#include <idl/capros/RTC.h>
#include <idl/capros/Logfile.h>
#include <idl/capros/SWCA.h>
#include <idl/capros/SWCANotify.h>

#include <domain/Runtime.h>
#include <domain/domdbg.h>
#include <domain/assert.h>

#define dbg_server 0x1
#define dbg_input  0x2
#define dbg_errors 0x4
#define dbg_inputData 0x8
#define dbg_sched  0x10
#define dbg_time   0x20
#define dbg_leds   0x40

/* Following should be an OR of some of the above */
#define dbg_flags   ( 0u | dbg_errors )
#define DEBUG(x) if (dbg_##x & dbg_flags)

#define numAdapters 3	// up to 8

#define KC_RTC      0
#define KC_LOGFILEC 1

#define KR_RTC     KR_CMTE(0)	// Input process only
#define KR_SERIAL  KR_CMTE(1)
#define KR_NOTIFY  KR_CMTE(2)	// Input process only

#define LKSN_SERIAL LKSN_CMTE	// holds the serial port key if we have one
#define LKSN_NOTIFY (LKSN_SERIAL+1)
#define LKSN_WAITER (LKSN_NOTIFY+1)
#define LKSN_LEDLOGS (LKSN_WAITER+1)	// one for each adapter
#define LKSN_InvChgLogs (LKSN_LEDLOGS + numAdapters)
#define LKSN_LoadLogs (LKSN_InvChgLogs + numAdapters)
#define LKSN_End (LKSN_LoadLogs + numAdapters)

#define keyInfo_nplinkee 0xffff	// nplink has this key
#define keyInfo_swca   0	// client has this key
#define keyInfo_notify 1	// input thread has this key

bool haveWaiter = false;	// LKSN_WAITER has a resume key
uint32_t waiterCode;		// parameters from waiter
unsigned int waiterAdapterNum;
unsigned int waiterMenuNum;
unsigned int waiterMenuItemNum;
uint32_t waiterW2;

typedef capros_RTC_time_t RTC_time;		// real time, seconds
typedef capros_Sleep_nanoseconds_t mono_time;	// monotonic time in nanoseconds

#define inBufEntries 100	// hopefully enough
struct InputPair {
  uint8_t flag;
  uint8_t data;
} __attribute__ ((packed))
inBuf[inBufEntries];

typedef struct AdapterState {
  int num;		// 0 through numAdapters-1, fixed for this adapter
			// -1 for dummyAdapter
  char lcd[2][16];	// the 2-line LCD display
  char * cursor;
  char * underscore;	// NULL if none
  uint8_t LEDs;
  uint8_t LEDsBlink;
  uint8_t LEDsLogged;
  mono_time LEDTimeChanged[8];	// time the LED last changed
  int menuNum;		// -1 if unknown
  int menuItemNum;

  int invChg;	// inverter/charger in amps AC
  int load;	// load in amps AC
} AdapterState;
AdapterState adapterStates[numAdapters];
int requestData;

// When we don't know which adapter is transmitting, this serves as a sink:
AdapterState dummyAdapter;

bool ConvertYN(AdapterState * as, bool * value);
bool ConvertInt(AdapterState * as, int * value);
bool Convert2p1(AdapterState * as, int * value);

bool haveSerialKey = false;

mono_time selectTime = 0;	// time at which we gave the last select cmd
mono_time commandTime = 0;	// time at which we gave the last command
				// FIXME should be atomic

// The adapter we want to select:
unsigned int wantedAdapterNum = 0;
// The menu we want to select (or -1 if we want LEDs):
int wantedMenuNum = -1;
// The menu item we want to select (meaningless if wantedMenuNum == -1):
int wantedMenuItemNum = 0;
bool gotLEDs;
bool gettingLEDs;
unsigned int selectRetries;
unsigned int menuRetries;

// The following variables must be accessed under the lock.
/* transmittingAdapterNum is meaningful if haveSerialKey and contains:
 * 0-7 if that adapter number is transmitting, or one of the following: */
enum {
  ta_NeedFirstSelect = -5,
  ta_NeedFirstScreen,
  ta_InFirstScreen,
  ta_NeedSecondSelect,
  ta_NeedSecondScreen
};
int transmittingAdapterNum;

/* curMenuMonoTime is valid if transmittingAdapterNum >= 0
and adapterStates[transmittingAdapterNum].menuNum != -1.
It is the time at which the menu item value was received,
or zero if the value hasn't been received yet. */
mono_time curMenuMonoTime;

// How to treat the next input character:
enum {
  nextIsControl,
  nextIsLEDs,
  nextIsUnknown
} inputState;

/* SerialCapSem is "up"ed when we receive a SerialPort capability,
 * and "down"ed when the input process accepts one. */
CMTESemaphore_DECLARE(SerialCapSem, 0);
CMTEMutex_DECLARE_Unlocked(lock);

static void
printControlPanel(AdapterState * as)
{
  kprintf(KR_OSTREAM, "\n%.16s\n%.16s\n", &as->lcd[0][0], &as->lcd[1][0]);
}

/******************************  Time stuff  *****************************/

static void
timerProcedure(unsigned long data)
{
  result_t result;
  result = capros_Node_getSlotExtended(KR_KEYSTORE, LKSN_NOTIFY, KR_NOTIFY);
  assert(result == RC_OK);
  DEBUG(time) kprintf(KR_OSTREAM, "Time notifying\n");
  result = capros_SWCANotify_notify(KR_NOTIFY);
  assert(result == RC_OK);
}

CMTETimer_Define(tmr, &timerProcedure, 0);

RTC_time
GetRTCTime(void)
{
  result_t result;
  RTC_time time;
  result = capros_RTC_getTime(KR_RTC, &time);
  assert(result == RC_OK);
  return time;
}

mono_time monoNow;
void
GetMonoTime(void)
{
  result_t result;
  result = capros_Sleep_getPersistentMonotonicTime(KR_SLEEP, &monoNow);
  assert(result == RC_OK);
  DEBUG(time) kprintf(KR_OSTREAM, "monoNow set %llu\n", monoNow);
}

/****************************  Task management  ***************************/

/* Terminology:
 * A "task" is a periodically-scheduled job.
 * A "request" is a one-time job in response to a client request.
 * A request has higher priority than tasks. */

// Sampling intervals:
#define twoSec  2000000000LL

/* The LED blink period is about one second.
 * Sample every 1/3 second to make sure we catch transitions. */
#define oneThirdSec 333333333LL

/* We loop through the task list, going to each specified menu item,
 * to sample data. */
struct Task {
  mono_time period;	// how often, in ns
  unsigned int adapterNum;
  unsigned int menuNum;	// -1 means get LEDs
  unsigned int menuItemNum;
  mono_time nextSampleTime;	// monotonic time in ns
} taskList[] = {
  {twoSec,      0, -1},	// don't need to know if this inverter's LEDs are blinkg
  {twoSec,      0, 3, 1},
  {twoSec,      0, 3, 3},
  {oneThirdSec, 1, -1},	// check for blinking
			/* Note, by checking only one inverter for blinking,
			we avoid selecting inverters every 1/3 second. */
  {twoSec,      1, 3, 1},
  {twoSec,      1, 3, 3},
  {twoSec,      2, -1},	// don't need to know if this inverter's LEDs are blinkg
  {twoSec,      2, 3, 1},
  {twoSec,      2, 3, 3},
};
#define numTasks (sizeof(taskList) / sizeof(struct Task))
struct Task * currentTask = &taskList[numTasks - 1];

enum {
  js_none,	// doing nothing; there could be work to be done
  js_task,	// executing a task
  js_request,	// executing a request
  js_waiting	// nothing to do until nextTaskTime
} jobState = js_none;
/* nextTaskTime is the time at which the chronologically next task
 * is to be done. Valid if taskStats == js_waiting. */
mono_time nextTaskTime;

void
StartTaskOrRequest(unsigned int adapterNum,
  unsigned int menuNum, unsigned int menuItemNum)
{
  wantedAdapterNum = adapterNum;
  wantedMenuNum = menuNum;
  wantedMenuItemNum = menuItemNum;
  if (transmittingAdapterNum != wantedAdapterNum)
    // Select the new adapter:
    transmittingAdapterNum = ta_NeedFirstSelect;
}

void
StartRequest(void)
{
  DEBUG(sched) kprintf(KR_OSTREAM, "Starting request (%d,%d,%d,%#x).\n",
                       waiterAdapterNum, waiterMenuNum,
                       waiterMenuItemNum, waiterCode);
  jobState = js_request;
  StartTaskOrRequest(waiterAdapterNum, waiterMenuNum, waiterMenuItemNum);
}

// Look for a task.
void
NextTask(void)
{
  nextTaskTime = infiniteTime;
  struct Task * loopStart = currentTask;
  do {
    if (++currentTask >= &taskList[numTasks])
      currentTask = &taskList[0];	// wrap
    // Do we need to do this task now?
    mono_time timeToDo = currentTask->nextSampleTime;
    if (timeToDo <= monoNow) {		// do it now
      DEBUG(sched) kprintf(KR_OSTREAM, "Starting task (%d,%d,%d).\n",
                     currentTask->adapterNum, currentTask->menuNum,
                     currentTask->menuItemNum);
      gotLEDs = false;	// in case wantedMenuNum == -1
      gettingLEDs = false;	// in case wantedMenuNum == -1
      jobState = js_task;
      StartTaskOrRequest(currentTask->adapterNum, currentTask->menuNum,
                         currentTask->menuItemNum);
      return;
    } else {		// do it later
      if (nextTaskTime > timeToDo )	// take min
        nextTaskTime = timeToDo;
    }
  } while (currentTask != loopStart);
  // No task needs to be done now. Return with nextTaskTime > monoNow.
  DEBUG(sched) kprintf(KR_OSTREAM, "No task till %lld.\n", nextTaskTime);
  jobState = js_waiting;
}

// We have lost synchronization with the input stream.
void
ResetInputState(void)
{
  transmittingAdapterNum = ta_NeedFirstSelect;
  /* Set inputState to ignore the next character received,
  because it might be an LEDs value, which could be any value. */
  inputState = nextIsUnknown;
}

static void
MenuIsUnknown(AdapterState * as)
{
  as->menuNum = -1;
  // Need to get the whole menu:
  memset(as->lcd, 0, sizeof(as->lcd));
}

void
ResetAdapterState(AdapterState * as)
{
  MenuIsUnknown(as);
  /* We don't really know where the cursor is, but it has to point somewhere:*/
  as->cursor = &as->lcd[0][0];
  as->underscore = NULL;
  int i;
  for (i = 0; i < 8; i++) {
    as->LEDTimeChanged[i] = 0;
  }
}

/********************** Menu item procedures *****************************/
/* These are called by the Input process only. */

bool
AppendLogRecord(unsigned int slot, uint8_t * rec)
{
  result_t result;

  result = capros_Node_getSlotExtended(KR_KEYSTORE, slot, KR_TEMP0);
  assert(result == RC_OK);
  assert(   sizeof(capros_Logfile_LogRecord16)
         == sizeof(capros_SWCA_LEDLogRecord));	// else need more code
  result = capros_Logfile_appendRecord(KR_TEMP0,
             sizeof(capros_Logfile_LogRecord16), rec);
  switch (result) {
  default:
    kprintf(KR_OSTREAM, "%#x\n", result);
    assert(false);
  case RC_capros_Logfile_OutOfSequence:	// this should not happen
    kprintf(KR_OSTREAM, "OOS  %#llx\n",
            ((capros_Logfile_recordHeader *)rec)->id);
    // Get newest record
    uint32_t lenGotten;
    capros_Logfile_LogRecord16 rec16;
    result = capros_Logfile_getPreviousRecord(KR_TEMP0,
               capros_Logfile_nullRecordID, sizeof(rec16), (uint8_t *)&rec16,
               &lenGotten);
    assert(result == RC_OK);
    kprintf(KR_OSTREAM, "prev=%#llx\n", rec16.header.id);
    assert(false);
  case RC_capros_Logfile_Full:
    DEBUG(errors) kprintf(KR_OSTREAM, "SWCA log %u full!\n", slot);
    break;	// Not much we can do but drop the record.

  case RC_capros_SpaceBank_LimitReached:
    DEBUG(errors) kprintf(KR_OSTREAM, "SWCA log %u out of space!\n", slot);
    break;	// Not much we can do but drop the record.

  case RC_OK:
    return true;
    break;
  }
  return false;
}

// The following all return true iff input is invalid.

bool
procInt(AdapterState * as)
{
  int value;
  if (ConvertInt(as, &value))
    return true;
  // Value not currently stored.
  return false;
}

bool
proc2p1(AdapterState * as)
{
  int value;
  if (Convert2p1(as, &value))
    return true;
  // Value not currently stored.
  return false;
}

bool
procYN(AdapterState * as)
{
  bool value;
  if (ConvertYN(as, &value))
    return true;
  // Value not currently stored.
  return false;
}

bool
procLogIntValue(AdapterState * as, int * pv, capros_Node_extAddr_t ks_logs)
{
  int value;
  if (ConvertInt(as, &value))
    return true;
  if (value != *pv) {
    // The value changed. Log the new value.
    capros_Logfile_LogRecord16 rec16;
    rec16.header.length = rec16.trailer = sizeof(rec16);
    rec16.header.rtc = GetRTCTime();
    rec16.header.id = monoNow;
    rec16.value = value;
    rec16.param = 0;

    if (AppendLogRecord(ks_logs + as->num, (uint8_t *)&rec16)) {
      *pv = value;
    }
  } else {
    // kprintf(KR_OSTREAM, "Same data, not logged\n");
  }
  return false;
}

bool
procIntRequest(AdapterState * as)
{
  int value;
  if (ConvertInt(as, &value))
    return true;
  requestData = value;
  return false;
}

bool
proc2p1Request(AdapterState * as)
{
  int value;
  if (Convert2p1(as, &value))
    return true;
  requestData = value;
  return false;
}

/* Convert a time of the form hh:mm to a number of minutes
 * in units of ten minutes.
 * If input is not valid, return true,
 * else return false and store value in requestData. */
bool
procHMRequest(AdapterState * as)
{
  const char * p = &as->lcd[1][11];
  char c0 = *p;
  char c1 = *++p;
  char c2 = *++p;
  char c3 = *++p;
  char c4 = *++p;
  if (! isdigit(c0) || ! isdigit(c1) || c2 != ':'
      || ! isdigit(c3) || c3 > '5' || c4 != '0' ) {
    DEBUG(errors) kprintf(KR_OSTREAM, "ConvertHM of %c%c%c%c%c failed!\n",
                          c0, c1, c2, c3, c4);
    return true;
  }
  int value = ((c0 - '0')*10 + (c1 - '0'))*6 + (c3 - '0');
  requestData = value;
  return false;		// OK
}

bool
procInvChg(AdapterState * as)
{
  return procLogIntValue(as, &as->invChg, LKSN_InvChgLogs);
}

bool
procInputAmps(AdapterState * as)
{
  int value;
  if (ConvertInt(as, &value))
    return true;
  // Value not currently stored.
  return false;
}

bool
procLoad(AdapterState * as)
{
  return procLogIntValue(as, &as->load, LKSN_LoadLogs);
}

bool
procBattTC(AdapterState * as)
{
  int value;
  if (Convert2p1(as, &value))
    return true;
  // Value not currently stored.
  return false;
}

bool
procInvVolts(AdapterState * as)
{
  int value;
  if (ConvertInt(as, &value))
    return true;
  // Value not currently stored.
  return false;
}

bool
procGridVolts(AdapterState * as)
{
  int value;
  if (ConvertInt(as, &value))
    return true;
  // Value not currently stored.
  return false;
}

bool
procGenVolts(AdapterState * as)
{
  int value;
  if (ConvertInt(as, &value))
    return true;
  // Value not currently stored.
  return false;
}

bool
procReadFreq(AdapterState * as)
{
  int value;
  if (ConvertInt(as, &value))
    return true;
  // Value not currently stored.
  return false;
}

// Returns -1 if underscore position is not valid.
static int
UnderscoreToGenMode(AdapterState * as)
{
  assert(as->underscore);
  long underscorePosition = as->underscore - &as->lcd[0][0];
  switch (underscorePosition) {
  default:
    DEBUG(errors) {
      printControlPanel(as);
      kprintf(KR_OSTREAM, "underscore position %d\n", underscorePosition);
    }
    return -1;
  case 16:
    return capros_SWCA_GenMode_Off;
  case 20:
    return capros_SWCA_GenMode_Auto;
  case 25:
    return capros_SWCA_GenMode_On;
  case 29:
    return capros_SWCA_GenMode_Eq;
  }
}

bool
procGenMode(AdapterState * as)
{
  int m = UnderscoreToGenMode(as);
  if (m < 0)
    return true;	// invalid underscore position
  requestData = m;
  return false;
}

#define numMenus 20
#define setupMenuNum 8	// zero origin
struct MenuItem {
  char lcd[2][16];
  bool (*valueProc)(AdapterState * as);
  /* The value field if any ends valueFieldEnd characters before the end
  of the second line of the LCD. */
  int valueFieldEnd;

  // The number of characters beyond the first 16 to compare for uniqueness.
  // May be negative.
  int addlCompare;

  bool (*underscoreProc)(AdapterState * as);
} menuItems[numMenus][13] = {
  [0][0] = {{"Inverter Mode   ","               1"}},
  [0][1] = {{"Set Inverter    ","OFF SRCH  ON CHG"}},

  [1][0] = {{"Generator Mode  ","               2"}},
  [1][1] = {{"Set Generator   ","OFF AUTO  ON EQ "}, 0, 0, 0, procGenMode},
  [1][2] = {{"Gen under/over  ","speed           "}, procYN},
  [1][3] = {{"Generator start ","error           "}, procYN},
  [1][4] = {{"Generator sync  ","error           "}, procYN},
  [1][5] = {{"Load Amp Start  ","ready           "}, procYN},
  [1][6] = {{"Voltage Start   ","ready           "}, procYN},
  [1][7] = {{"Exercise Start  ","ready           "}, procYN},

  [2][0] = {{"Trace           ","Engineering    3"}},

  [3][0] = {{"Meters          ","               4"}},
  [3][1] = {{"Inverter/charger","Amps AC         "}, procInvChg, 2},
  [3][2] = {{"Input           ","amps AC         "}, procInputAmps, 2},
  [3][3] = {{"Load            ","amps AC         "}, procLoad, 2},
  [3][4] = {{"Battery actual  ","volts DC        "}, proc2p1Request},
  [3][5] = {{"Battery TempComp","volts DC        "}, procBattTC},
  [3][6] = {{"Inverter        ","volts AC        "}, procInvVolts},
  [3][7] = {{"Grid (AC1)      ","volts AC        "}, procGridVolts},
  [3][8] = {{"Generator (AC2) ","volts AC        "}, procGenVolts},
  [3][9] = {{"Read Frequency  ","Hertz           "}, procReadFreq},

  [4][0] = {{"Error Causes    ","               5"}},
  [4][1] = {{"Over Current    ","                "}, procYN},
  [4][2] = {{"Transformer     ","overtemp        "}, procYN},
  [4][3] = {{"Heatsink        ","overtemp        "}, procYN},
  [4][4] = {{"High Battery    ","voltage         "}, procYN},
  [4][5] = {{"Low Battery     ","voltage         "}, procYN},
  [4][6] = {{"AC source wired ","to output       "}, procYN},
  [4][7] = {{"Input Relay     ","failure         "}, procYN},
  [4][8] = {{"External error  ","(stacked)       "}, procYN},
#if 0 /* Note, do not use the following. They are duplicated under the
	generator menu and the menu traversing code will get confused. */
  [4][9] = {{"Generator start ","error           "}, procYN},
  [4][10] = {{"Generator sync  ","error           "}, procYN},
  [4][11] = {{"Gen under/over  ","speed           "}, procYN},
  // [4][12] marks end of list
#endif

  [5][0] = {{"Time of Day     ","               6"}, 00, 4},

  [6][0] = {{"Generator Timer ","               7"}},

  [7][0] = {{"END USER MENU   ","               8"}},

  [8][0] = {{"Inverter Setup  ","               9"}},
  [8][1] = {{"Set Grid Usage  ","FLT SELL SLT LBX"}},
  [8][2] = {{"Set Low battery ","cut out VDC     "}, proc2p1Request, 0, 11},
  [8][3] = {{"Set LBCO delay  ","minutes         "}, procIntRequest},
  [8][4] = {{"Set Low battery ","cut in VDC      "}, proc2p1Request, 0, 11},
  [8][5] = {{"Set High battery","cut out VDC     "}, proc2p1Request},
  [8][6] = {{"Set search      ","watts           "}, procIntRequest, 0, 7},
  [8][7] = {{"Set search      ","spacing         "}, procIntRequest, 0, 7},

  [9][0] = {{"Battery Charging","              10"}},
  [9][1] = {{"Set Bulk        ","volts DC        "}, proc2p1Request},
  [9][2] = {{"Set Absorbtion  ","time h:m        "}, procHMRequest},
  [9][3] = {{"Set Float       ","volts DC        "}, proc2p1Request},
  [9][4] = {{"Set Equalize    ","volts DC        "}, proc2p1Request, 0, 8},
  [9][5] = {{"Set Equalize    ","time h:m        "}, procHMRequest, 0, 8},
  [9][6] = {{"Set Max Charge  ","amps AC         "}, procIntRequest, 2},
  [9][7] = {{"Set Temp Comp   ","LeadAcid NiCad  "}},

  [10][0] = {{"AC Inputs       ","              11"}},
  [10][1] = {{"Set Grid (AC1)  ","amps AC         "}, procIntRequest, 2},
  [10][2] = {{"Set Gen (AC2)   ","amps AC         "}, procIntRequest, 2},
  [10][3] = {{"Set Input lower ","limit VAC       "}, procIntRequest},
  [10][4] = {{"Set Input upper ","limit VAC       "}, procIntRequest},

  [11][0] = {{"Gen Auto Start  ","setup         12"}},
  [11][1] = {{"Set Load Start  ","amps AC         "}, procIntRequest, 2, 9},
  [11][2] = {{"Set Load Start  ","delay min       "}, proc2p1Request, 0, 9},
  [11][3] = {{"Set Load Stop   ","delay min       "}, proc2p1Request},
  [11][4] = {{"Set 24 hr start ","volts DC        "}, proc2p1Request},
  [11][5] = {{"Set 2  hr start ","volts DC        "}, proc2p1Request},
  [11][6] = {{"Set 15 min start","volts DC        "}, proc2p1Request},
  [11][7] = {{"Read LBCO 30 sec","start VDC       "}, proc2p1Request},
  [11][8] = {{"Set Exercise    ","period days     "}, procIntRequest},

  [12][0] = {{"Gen starting    ","details       13"}},

  [13][0] = {{"Auxilary Relays ","R9 R10 R11    14"}},

  [14][0] = {{"Bulk Charge     ","Trigger Timer 15"}},

  [15][0] = {{"Low Battery     ","Transfer (LBX)16"}},

  [16][0] = {{"Battery Selling ","              17"}},

  [17][0] = {{"Grid Usage Timer","              18"}},

  [18][0] = {{"Information file","battery       19"}},

  [19][0] = {{"End Setup Menu  ","              20"}},
}
;
bool
ConvertYN(AdapterState * as, bool * value)
{
  const char * p = &as->lcd[1][16 - 1];
  char c9 = *p;
  char c8 = *(p-1);
  char c7 = *(p-2);
  if (c7 == ' ' && c8 == 'N' && c9 == 'O') {
    *value = false;
    return false;
  } else if (c7 == 'Y' && c8 == 'E' && c9 == 'S') {
    *value = true;
    return false;
  } else
    return true;	// not valid
}

// Convert an integer field to binary.
// Acceptable formats are:
//  dd
// -dd
// ddd
// If input is not valid, return true, else return false and value in *value.
bool
ConvertInt(AdapterState * as, int * value)
{
  struct MenuItem * mi = &menuItems[as->menuNum][as->menuItemNum];
  const char * p = &as->lcd[1][16 - mi->valueFieldEnd - 1];
  char c9 = *p;
  char c8 = *(p-1);
  char c7 = *(p-2);
  int sign = 1;
  if (c7 == '-') {
    sign = -1;
    c7 = '0';
  } else if (c7 == ' ')
    c7 = '0';

  if (! isdigit(c9) || ! isdigit(c8) || ! isdigit(c7)) {
    DEBUG(errors) kprintf(KR_OSTREAM, "ConvertInt of %c%c%c failed!\n",
                          *(p-2), c8, c9);
    return true;
  }
  int absValue = ((c7 - '0')*10 + (c8 - '0'))*10 + (c9 - '0');
  *value = sign > 0 ? absValue : -absValue;
  //kprintf(KR_OSTREAM, "ConvertInt %d\n", *value);
  return false;		// OK
}

// Convert a number of the form dd.d to the binary value of ddd (scaled by 10).
// If input is not valid, return true, else return false and value in *value.
bool
Convert2p1(AdapterState * as, int * value)
{
  const char * p = &as->lcd[1][12];
  char c1 = *p;
  char c2 = *++p;
  char c3 = *++p;
  char c4 = *++p;
  if (! isdigit(c1) || ! isdigit(c2) || c3 != '.' || ! isdigit(c4)) {
    DEBUG(errors) kprintf(KR_OSTREAM, "Convert2p1 of %c%c%c%c failed!\n",
                          c1, c2, c3, c4);
    return true;
  }
  *value = ((c1 - '0')*10 + (c2 - '0'))*10 + (c4 - '0');
  //kprintf(KR_OSTREAM, "Convert2p1 %d\n", *value);
  return false;		// OK
}

/*****************  Procedures for the Input process only  *****************/

// Must not be under lock.
void
InputNotifyServer(void)
{
  DEBUG(input) kprintf(KR_OSTREAM, "Input notifying\n");
  result_t result = capros_SWCANotify_notify(KR_NOTIFY);
  assert(result == RC_OK);
  DEBUG(input) kprintf(KR_OSTREAM, "Input done notifying\n");
}

bool
menuItemCompare(AdapterState * as, struct MenuItem * mi)
{
  return ! memcmp(&as->lcd[0][0], &mi->lcd[0][0], 16 + mi->addlCompare);
}

// Called under lock, exits not under lock.
void
CheckEndOfField(AdapterState * as)
{
  if (as->menuNum >= 0) {	// we are on a known menu
    struct MenuItem * mi = &menuItems[as->menuNum][as->menuItemNum];
    int vfeAdj = mi->valueFieldEnd;
    /* mi->valueFieldEnd is the number of characters from the end of the field
    to the end of the LCD panel.
    If mi->valueFieldEnd == 0, we are looking for a field that ends
    exactly at the end of the LCD panel.
    But in that case the cursor has already wrapped to the beginning,
    so we need to adjust: */
    if (vfeAdj == 0)
      vfeAdj = 32;
    if (as->cursor + vfeAdj == &as->lcd[1][16]) {
      // We just received the last character of a value field.
      bool (*valueProc)(AdapterState *) = mi->valueProc;
      if (valueProc) {
        GetMonoTime();
        if ((*valueProc)(as))
          ;	// invalid value: what to do here?
        else {		// successful
          curMenuMonoTime = monoNow;
          CMTEMutex_unlock(&lock);
          InputNotifyServer();
          return;
        }
      }
    }
  }
  CMTEMutex_unlock(&lock);
}

// After we receive the last character of an LCD screen, the cursor wraps
// to the beginning. Hence we test if it is at the beginning to see
// if we received the last character.
static bool
CursorAtBegOfScreen(AdapterState * as)
{
  return as->cursor == &as->lcd[0][0];
}

// Call under lock.
enum {
  pc_printable,
  pc_underscore,
  pc_other
}
ProcessCharacter(AdapterState * as, uint8_t c)
{
  if (c >= 240)
    ;	/* characters 255, 254, 253 are sometimes output when
	   an adapter stops transmitting. Ignore them. */
  else if (c >= 32 && c <= 126) {	// a printable character
    *as->cursor++ = c;
    if (as->cursor > &as->lcd[1][15]) {	// past end of screen
      as->cursor = &as->lcd[0][0]; // wrap
      DEBUG(input) printControlPanel(as);
    }
    return pc_printable;
  } else if (c >= 128 && c <= 143) {	// first line cursor position
    as->cursor = &as->lcd[0][0] + (c-128);
    as->menuNum = -1;		// could be a new menu
  } else if (c >= 192 && c <= 207) {	// second line cursor position
    as->cursor = &as->lcd[1][0] + (c-192);
  } else {
    switch (c) {
    default:
#if 0	// these errors are so common, don't report them:
      DEBUG(errors) kprintf(KR_OSTREAM, "%d UNKNOWN ", c);
#endif
      ResetInputState();
    // Don't know what the following mean, but they seem harmless,
    // so don't reset the state on their account:
    case   2:
    case  14:
    case  18:
    case 144:
    case 154:
    case 160:
    case 164:
    case 184:
    case 223:
    case 224:
    case 228:
    case 237:
    case 239:
      break;

    case 225:
      as->underscore = as->cursor;
      return pc_underscore;

    case 227:
      inputState = nextIsLEDs;
      break;
    }
  }
  return pc_other;	// not a printable character
}

// The input thread executes this procedure.
void *
InputProcedure(void * data /* unused */ )
{
  result_t result;

  result = capros_Node_getSlotExtended(KR_KEYSTORE, LKSN_NOTIFY, KR_NOTIFY);
  assert(result == RC_OK);
  result = capros_Node_getSlotExtended(KR_CONSTIT, KC_RTC, KR_RTC);
  assert(result == RC_OK);

  DEBUG(input) kprintf(KR_OSTREAM, "Input process starting\n");

  for (;;) {	// loop getting good SerialPort caps
    CMTESemaphore_down(&SerialCapSem);	// wait for a SerialPort cap
    result = capros_Node_getSlotExtended(KR_KEYSTORE, LKSN_SERIAL, KR_SERIAL);
    assert(result == RC_OK);

    for (;;) {	// loop reading data
      uint32_t pairsRcvd;
      result = capros_SerialPort_read(KR_SERIAL, 
                 inBufEntries, &pairsRcvd, (uint8_t *)&inBuf[0]);
      if (result != RC_OK) {
        DEBUG(errors) kprintf(KR_OSTREAM,
                        "SerialPort_read returned %#x\n", result);
      }
      if (result == RC_capros_key_Restart || result == RC_capros_key_Void)
        break;	// need a new SerialPort cap
      if (result != RC_OK) {	// unexpected error
        kdprintf(KR_OSTREAM, "SerialPort_read returned %#x\n", result);
        break;
      }
      unsigned int i;
      for (i = 0; i < pairsRcvd; i++) {
        switch (inBuf[i].flag) {
        case capros_SerialPort_Flag_NORMAL:
          break;
        case capros_SerialPort_Flag_BREAK:
          DEBUG(errors) kprintf(KR_OSTREAM, "BREAK ");
          assert(inBuf[i].data == 0);
          ResetInputState();
          goto nochar;
        case capros_SerialPort_Flag_FRAME:
          DEBUG(errors) kprintf(KR_OSTREAM, "FRAME ");
          ResetInputState();
          goto nochar;
        case capros_SerialPort_Flag_PARITY:
          DEBUG(errors) kprintf(KR_OSTREAM, "PARITY ");
          ResetInputState();
          goto nochar;
        case capros_SerialPort_Flag_OVERRUN:
          DEBUG(errors) kprintf(KR_OSTREAM, "OVERRUN ");
          ResetInputState();
          goto nochar;
        default:
          DEBUG(errors) kdprintf(KR_OSTREAM,
                          "SerialPort flag is %d ",
                          inBuf[i].flag);
        }
        // Process the character.
        uint8_t c = inBuf[i].data;
        DEBUG(inputData) kprintf(KR_OSTREAM, "%d ", c);
        CMTEMutex_lock(&lock);
        AdapterState * as = (transmittingAdapterNum >= 0
                       ? &adapterStates[transmittingAdapterNum] : &dummyAdapter);
        switch (inputState) {
        case nextIsLEDs:
          // c has the new LEDs state.
          {
            GetMonoTime();
            DEBUG(leds) kprintf(KR_OSTREAM, "Inv %d read LEDs=%#x getting=%d got=%d\n",
                          transmittingAdapterNum, c, gettingLEDs, gotLEDs);
            int i;
            uint8_t blink = 0;
            for (i = 0; i < 8; i++) {	// process each bit
              unsigned int mask = 1 << i;
              // Determine whether the LED is blinking.
/* The LEDs blink with a period of about 1 second.
 * We use 2 seconds here to be safe. */
#define BlinkPeriod 2000000000LL	// in ns
              bool steady = monoNow > as->LEDTimeChanged[i] + BlinkPeriod;
              if ((as->LEDs ^ c) & mask) {	// changed state
                if (! steady) 
                  blink |= mask;	// it's blinking
                // else it might be blinking; we won't know for a while.
                as->LEDTimeChanged[i] = monoNow;
              } else {			// same state as before
                if (steady)
                  blink &= ~mask;	// it's not blinking
              }
            }
            // Was there a change?
            uint8_t steadyLEDs = c & ~ blink;
            if ((steadyLEDs != as->LEDsLogged
                 || blink != as->LEDsBlink )
                && as->num >= 0 ) {
              // yes, log the new data.
              capros_SWCA_LEDLogRecord ledRec;
              ledRec.header.length = ledRec.trailer = sizeof(ledRec);
              ledRec.header.rtc = GetRTCTime();
              ledRec.header.id = monoNow;
              ledRec.LEDsSteady = steadyLEDs;
              ledRec.LEDsBlink = blink;
              ledRec.padding = 0;

              if (AppendLogRecord(LKSN_LEDLOGS + as->num, (uint8_t *)&ledRec)) {
                as->LEDsLogged = steadyLEDs;
                as->LEDsBlink = blink;
              }
            }
            as->LEDs = c;
            gotLEDs = true;
            inputState = nextIsControl;
            curMenuMonoTime = monoNow;
            CMTEMutex_unlock(&lock);
            InputNotifyServer();
            break;
          }
          // fall into the below
        default:	// nextIsUnknown, ignore the character
          inputState = nextIsControl;
          CMTEMutex_unlock(&lock);
          break;

        case nextIsControl:
          //DEBUG(sched) kprintf(KR_OSTREAM, "taNum=%d ", transmittingAdapterNum);
          switch (transmittingAdapterNum) {
          case ta_NeedFirstScreen:
            if (c == 128) {	// start of the first LCD screen
              transmittingAdapterNum = ta_InFirstScreen;
            }
          case ta_NeedFirstSelect:
          case ta_NeedSecondSelect:
          processChar:
            ProcessCharacter(as, c);	// process char w/ dummyAdapter
          unlock1:
            CMTEMutex_unlock(&lock);
            break;

          case ta_InFirstScreen:
            // process char w/ dummyAdapter
            if (ProcessCharacter(as, c) == pc_printable) {
              // it was a printable character
              if (CursorAtBegOfScreen(as)) {
                // First screen completed.
                transmittingAdapterNum = ta_NeedSecondSelect;
                goto notifyServer;
              }
            }
            goto unlock1;

          case ta_NeedSecondScreen:
            if (c == 128) {	// start of the second LCD screen
              transmittingAdapterNum = wantedAdapterNum;	// selected it
              as = &adapterStates[transmittingAdapterNum];
              DEBUG(input) kprintf(KR_OSTREAM, "Selected %d at %#x\n",
                                   as->num, as);
              ResetAdapterState(as);	// don't know the state now
              // Set the number of retries to select the next adapter:
              selectRetries = 6;
              goto haveAdapter;
            }
            goto processChar;	// process char w/ dummyAdapter

          default:
          haveAdapter:
            assert(transmittingAdapterNum >= 0);	// adapter is known
            switch (ProcessCharacter(as, c)) {
            case pc_printable:
              // it was a printable character
              if (CursorAtBegOfScreen(as)
                  && as->lcd[0][0] != 0) {
                // Completed a screen.
                if (as->menuNum >= 0) {
                  // Are we on the menu item we think we are on?
                  struct MenuItem * mi = &menuItems[as->menuNum][as->menuItemNum];
                  if (! menuItemCompare(as, mi)) {	// no
                    MenuIsUnknown(as);
                  }
                }
                if (as->menuNum < 0) {
                  // Are we on a menu item we recognize?
                  int i, j;
                  struct MenuItem * mi = NULL;
                  for (i = 0; i < numMenus; i++) {
                    j = 0;
                    mi = &menuItems[i][0];
                    while (mi->lcd[0][0]) {	// while an item exists
                      if (menuItemCompare(as, mi)) {
                        // found it
                        as->menuNum = i;
                        as->menuItemNum = j;
                        /* We are at the menu item, but the value may not
                        be valid yet. */
                        curMenuMonoTime = 0;
                        DEBUG(input) kprintf(KR_OSTREAM, "Menu %d item %d\n",
                                             as->menuNum, as->menuItemNum);
                        // Set the number of retries to find the next menu:
                        menuRetries = 6;
                        goto foundMenu;
                      }
                      j++;
                      mi++;
                    }
                  }
                  DEBUG(errors) kprintf(KR_OSTREAM, "%.16s unrecognized\n",
                                        &as->lcd[0][0]);
                  as->menuNum = -1;	// unrecognized menu
                foundMenu:
                notifyServer:
                  // Skip CheckEndOfField, because the field may not be
                  // filled in yet.
                  CMTEMutex_unlock(&lock);
                  InputNotifyServer();	// not needed if at the wanted menu item
                } else {	// Completed a previously recognized screen.
                  CheckEndOfField(as);	// also unlocks
                }
              } else {	// printable character not at end of screen
                CheckEndOfField(as);	// also unlocks
              }
              break;
            case pc_underscore:
              if (as->menuNum >= 0) {	// we are on a known menu
                struct MenuItem * mi = &menuItems[as->menuNum][as->menuItemNum];
                bool (*underscoreProc)(AdapterState *) = mi->underscoreProc;
                if (underscoreProc) {
                  // Call procedure under lock:
                  if ((*underscoreProc)(as))
                    ;	// invalid position; don't set curMenuMonoTime
                  else {		// successful
                    GetMonoTime();
                    curMenuMonoTime = monoNow;
                    CMTEMutex_unlock(&lock);
                    InputNotifyServer();
                    break;
                  }
                }
              }
              CMTEMutex_unlock(&lock);
              break;
            case pc_other:
              CMTEMutex_unlock(&lock);
            }
          }	// end of switch (transmittingAdapterNum)
        }		// end of switch (inputState)
        nochar: ;
      }
    }
    // SerialPort key no longer valid.
  }
  assert(false);// not implemented
  return 0;
}

/****************************  Server stuff  *****************************/

void
InitSerialPort(void)
{
  result_t result;
  uint32_t openErr;
  int i;

  result = capros_SerialPort_open(KR_SERIAL, &openErr);
  if (result == RC_capros_key_Restart || result == RC_capros_key_Void)
    return;	// as initialized as it is going to get
  assert(result == RC_OK);

  struct capros_SerialPort_termios2 termio = {
    .c_iflag = capros_SerialPort_BRKINT | capros_SerialPort_INPCK,
    /* 8 bits No parity 1 stop bit: */
    .c_cflag = capros_SerialPort_CS8 | capros_SerialPort_CREAD,
    .c_line = 0,
    .c_ospeed = 9600
  };
  result = capros_SerialPort_setTermios2(KR_SERIAL, termio);
  if (result == RC_capros_key_Restart || result == RC_capros_key_Void)
    return;	// as initialized as it is going to get
  assert(result == RC_OK);

  ResetInputState();
  haveSerialKey = true;

  /* After getting a new SerialPort, always log the next value
  (it may be after a restart and time may have passed). */
  for (i = 0; i < numAdapters; i++) {
    AdapterState * as = &adapterStates[i];
    as->LEDsBlink = 0xff;	// ensure first log entry for these
    as->invChg = INT_MAX;
    as->load = INT_MAX;
  }
}

// monoNow must be current.
void
SendCommand(uint8_t cmd)
{
  result_t result;

  commandTime = monoNow;
  result = capros_SerialPort_write(KR_SERIAL, 1, &cmd);
  if (result == RC_capros_key_Restart || result == RC_capros_key_Void)
    // Set haveSerialKey false here?
    return;	// too bad
  if (result != RC_OK) {
    kdprintf(KR_OSTREAM, "capros_SerialPort_write rc=%#x\n", result);
  }
}

// monoNow must be current.
#define selectTimeout 1000000000 // one second
void
SelectAdapter(unsigned int num)	// 0-7
{
  assert(num < numAdapters);
  selectTime = monoNow;
  CMTETimer_setDuration(&tmr, selectTimeout);
  DEBUG(time) kprintf(KR_OSTREAM, "SelectAdapter set timeout\n");
  SendCommand(num + 1);
}

uint32_t MsgBuf[16/4];

/*
 * If we are at the wanted adapter, menu, and menu item and have its data,
 *   returns true and exits still under lock.
 * Otherwise attempts to navigate to the wanted adapter, menu, and menu item,
 *   returns false, and exits not under lock.
 */
static bool
DoMenu(void)
{
  DEBUG(sched) kprintf(KR_OSTREAM, "taNum=%d ", transmittingAdapterNum);
  switch (transmittingAdapterNum) {
  case ta_NeedFirstScreen:
  case ta_InFirstScreen:
  case ta_NeedSecondScreen: ;
    // Have we timed out?
    int64_t timeToWait = selectTime + selectTimeout - monoNow;
    if (timeToWait <= 0) {	// timed out
#if 0	// this error is too common to note
      DEBUG(errors) kprintf(KR_OSTREAM, "Selecting %d timed out\n",
                            wantedAdapterNum);
#endif
      transmittingAdapterNum = ta_NeedFirstSelect;
      if (--selectRetries == 0) {		// too many retries
        DEBUG(errors)
          kprintf(KR_OSTREAM, "Too many retries to select adapter; selecting a different adapter\n");
        // Try selecting a different adapter:
        SelectAdapter(wantedAdapterNum == 0 ? 1 : 0);
        transmittingAdapterNum = ta_NeedFirstScreen;
        break;
      }
  case ta_NeedFirstSelect:
  case ta_NeedSecondSelect:
      SelectAdapter(wantedAdapterNum);
      DEBUG(sched) kprintf(KR_OSTREAM, "Selecting %d\n", wantedAdapterNum);
      transmittingAdapterNum++;
			// to ta_NeedFirstScreen or ta_NeedSecondScreen
      break;
    }
    DEBUG(sched) kprintf(KR_OSTREAM, "Wait to select\n");
    break;	// just wait

  default:
    assert(transmittingAdapterNum == wantedAdapterNum);
    AdapterState * as = &adapterStates[wantedAdapterNum];
    DEBUG(sched) kprintf(KR_OSTREAM, "wMenu=%d gotLEDs=%d ",
                         wantedMenuNum, gotLEDs);
    if (wantedMenuNum == -1) {
      if (! gotLEDs) {
#define ledsTimeout 500000000 // 0.5 second
        // Need to read the LEDs.
        int64_t timeToWait;
        if (! gettingLEDs) {	// first attempt to get LEDs
          gettingLEDs = true;
          menuRetries = 4;
        } else {
          // Allow time for the previous command:
          timeToWait = commandTime + ledsTimeout - monoNow;
          if (timeToWait > 0) {
            CMTEMutex_unlock(&lock);
            goto setLEDsTimer;	// Give the previous command more time to work.
          }
          DEBUG(leds) kprintf(KR_OSTREAM, "Get LEDs timed out.\n");
          if (--menuRetries == 0) {		// too many retries
            DEBUG(errors)
              kprintf(KR_OSTREAM, "Get LEDs timed out; reselecting adapter\n");
            // Try reselecting the adapter:
            SelectAdapter(wantedAdapterNum);
            transmittingAdapterNum = ta_NeedFirstScreen;
            break;
          }
          // else retry getting the LEDs
        }
        CMTEMutex_unlock(&lock);
        SendCommand(227);	// command to get LEDs
        // timeToWait = commandTime + ledsTimeout - monoNow;
        // Since we just set commandTime to monoNow:
        timeToWait = ledsTimeout;
      setLEDsTimer:
        CMTETimer_setDuration(&tmr, timeToWait);
        DEBUG(time) kprintf(KR_OSTREAM,
                            "Get LEDs set timeout in %llu\n", timeToWait);
        return false;
      } else {	// gotLEDs
        return true;	// exit under lock
      }
    } else if (as->menuNum != wantedMenuNum
               || as->menuItemNum != wantedMenuItemNum ) {
      DEBUG(sched) kprintf(KR_OSTREAM, "asMenu=%d wMenIt=%d asMenIt=%d\n",
                         as->menuNum, wantedMenuItemNum, as->menuItemNum);
#define commandTimeout 1000000000 // 1 second
      // Not on the menu item we want.
      int64_t timeToWait;
      char cmd;
      if (as->menuNum == -1) { // current menu unknown, or executing cmd
    checkMenuTime:
        // Allow time for the previous command:
        timeToWait = commandTime + commandTimeout - monoNow;
        if (timeToWait > 0) {
          CMTEMutex_unlock(&lock);
          goto setMenuTimer;	// Give the previous command some time to work.
        }
        // current menu is unknown, and previous command timed out
        if (--menuRetries == 0) {		// too many retries
          DEBUG(errors)
            kprintf(KR_OSTREAM, "Menu select timed out; reselecting adapter\n");
          // Try reselecting the adapter:
          SelectAdapter(wantedAdapterNum);
          transmittingAdapterNum = ta_NeedFirstScreen;
          break;
        }
        CMTEMutex_unlock(&lock);
        // We're lost; retry a menu command
#if 0	// this error is too common to note
        DEBUG(errors) kprintf(KR_OSTREAM, "SWCA: Unknown menu, moving up\n");
#endif
        cmd = 'U';	// menu item up
      } else {		// current menu is known
        CMTEMutex_unlock(&lock);
        /* Give a command to get from the menu we're at
        to the one we want.
        Note: we don't use the 'I' and 'G' commands to go directly
        to the Set Inverter and Set Generator menu items,
        because if we happen to be there already, those commands
        change the setting, which would be dangerous.
        In general we can't know if we are there already, because
        someone could be independently pressing buttons on the
        hardware control panel. */
        if (as->menuNum != wantedMenuNum) {
          // We need to go to a different menu.
          int diff = wantedMenuNum - as->menuNum;
          if (wantedMenuNum >= setupMenuNum	// going to a setup menu
              && as->menuNum < setupMenuNum)	// from a non-setup menu
            cmd = 19;	// control-S for Setup menu
          else if (diff > 0)
            cmd = 'R';	// menu right
          else
            cmd = 'L';	// menu left
        } else {
          // right menu, wrong item
          if (wantedMenuItemNum < as->menuItemNum)
            cmd = 'U';	// menu item up
          else cmd = 'D';	// menu item down
        }
      }
      DEBUG(server) kprintf(KR_OSTREAM,
                      "Inv %d at (%d,%d) want (%d,%d), menu cmd %c\n",
                      as->num, as->menuNum, as->menuItemNum,
                      wantedMenuNum, wantedMenuItemNum,
                      cmd == 19 /* control-S */ ? 's' : cmd );
      MenuIsUnknown(as);	// set unknown because we are changing
      SendCommand(cmd);
      // timeToWait = commandTime + commandTimeout - monoNow;
      // We just set commandTime to monoNow, so:
      timeToWait = commandTimeout;
    setMenuTimer:
      CMTETimer_setDuration(&tmr, timeToWait);
      DEBUG(time) kprintf(KR_OSTREAM,
                        "Select menu set timeout in %llu\n", timeToWait);
      return false;
    } else {
      // we have the menu item we want
      DEBUG(sched) kprintf(KR_OSTREAM, "cmmt=%llu cmdt=%llu ttw=%llu",
                           curMenuMonoTime, commandTime,
                           commandTime + commandTimeout - monoNow );
      if (curMenuMonoTime) {	// and we have the data there
        return true;	// exit under lock
      } else {
        // Just waiting for data for this menu item to come in.
        goto checkMenuTime;
      }
    }
  }	// end of switch (transmittingAdapterNum)
  CMTEMutex_unlock(&lock);
  return false;
}

void
CompleteRequest(uint32_t w1)
{
  result_t result;

  assert(haveWaiter);
  result = capros_Node_getSlotExtended(KR_KEYSTORE, LKSN_WAITER, KR_TEMP0);
  assert(result == RC_OK);

  Message Msg = {
    .snd_invKey = KR_TEMP0,
    .snd_key0 = KR_VOID,
    .snd_key1 = KR_VOID,
    .snd_key2 = KR_VOID,
    .snd_rsmkey = KR_VOID,
    .snd_len = 0,
    .snd_code = RC_OK,
    .snd_w1 = w1,
    .snd_w2 = 0,
    .snd_w3 = 0,
  };
  SEND(&Msg);
  haveWaiter = false;
}

/* If we the current request is completed,
 *   returns true and exits still under lock.
 * Otherwise takes action on the current request,
 *   returns false, and exits not under lock.
 */
bool
AdjustUpOrDown(int diff)
{
  if (diff == 0) {
    CompleteRequest(0);
    return true;
  }
  CMTEMutex_unlock(&lock);
  SendCommand(diff < 0 ? '-' : '+');
  // Wait until we get the whole menu back before trying again.
  MenuIsUnknown(&adapterStates[transmittingAdapterNum]);
  // currently, no timeout for these commands
  return false;
}

/* If we the current request is completed,
 *   returns true and exits still under lock.
 * Otherwise takes action on the current request,
 *   returns false, and exits not under lock.
 */
bool
AdjustUnderscore(uint32_t mode, int wanted, int numModes)
{
  int distance = mode - wanted;
  if (distance == 0) {
    CompleteRequest(0);
    return true;
  }
  CMTEMutex_unlock(&lock);
  int modDistance = distance < 0 ? numModes + distance : distance;
  SendCommand(modDistance == 1 ? '-' : '+');
  // Wait until we get the whole menu back before trying again.
  MenuIsUnknown(&adapterStates[transmittingAdapterNum]);
  // currently, no timeout for these commands
  return false;
}

/* If we the current request is completed,
 *   returns true and exits still under lock.
 * Otherwise takes action on the current request,
 *   returns false, and exits not under lock.
 */
bool
DoRequest(void)
{
  if (DoMenu()) {
    // We are at the wanted menu item and have data. Now what?
    switch (waiterCode) {
    default:
      assert(false);

    case OC_capros_SWCA_getBatteryVolts:
    case OC_capros_SWCA_getLBCOVolts:
    case OC_capros_SWCA_getBulkVolts:
    case OC_capros_SWCA_getAbsorptionTime:
    case OC_capros_SWCA_getFloatVolts:
    case OC_capros_SWCA_getEqualizeVolts:
    case OC_capros_SWCA_getEqualizeTime:
    case OC_capros_SWCA_getMaxChargeAmps:
    case OC_capros_SWCA_getGenAmps:
    case OC_capros_SWCA_get24HourStartVolts:
    case OC_capros_SWCA_get2HourStartVolts:
    case OC_capros_SWCA_get15MinStartVolts:
      CompleteRequest(requestData);
      return true;

    case OC_capros_SWCA_setLBCOVolts:
    case OC_capros_SWCA_setBulkVolts:
    case OC_capros_SWCA_setAbsorptionTime:
    case OC_capros_SWCA_setFloatVolts:
    case OC_capros_SWCA_setEqualizeVolts:
    case OC_capros_SWCA_setEqualizeTime:
    case OC_capros_SWCA_setMaxChargeAmps:
    case OC_capros_SWCA_setGenAmps:
    case OC_capros_SWCA_set24HourStartVolts:
    case OC_capros_SWCA_set2HourStartVolts:
    case OC_capros_SWCA_set15MinStartVolts:
      return AdjustUpOrDown(waiterW2 - requestData);

    case OC_capros_SWCA_getGeneratorMode:
      CompleteRequest(requestData);
      return true;

    case OC_capros_SWCA_setGeneratorMode:
      return AdjustUnderscore(requestData, waiterW2, 4);
    }
  }
  return false;
}

void
WaiterRequest(Message * msg, unsigned int menuNum, unsigned int menuItemNum)
{
  result_t result;

  if (haveWaiter) {
    msg->snd_code = RC_capros_SWCA_already;
  } else {
    waiterMenuNum = menuNum;
    waiterMenuItemNum = menuItemNum;
    result = capros_Node_swapSlotExtended(KR_KEYSTORE, LKSN_WAITER,
               KR_RETURN, KR_VOID);
    assert(result == RC_OK);
    waiterCode = msg->rcv_code;
    waiterAdapterNum = msg->rcv_w1;
    waiterW2 = msg->rcv_w2;
    haveWaiter = true;
    /* At the end of the current task, the input proc will notice
    this request and execute it. */
    msg->snd_invKey = KR_VOID;
  }
}

void
VoltsRequest(Message * msg, unsigned int menuNum, unsigned int menuItemNum,
  int32_t min, int32_t max)
{
  if (msg->rcv_w2 < min		// too low
      || msg->rcv_w2 > max	// too high
      || msg->rcv_w2 & 1 )	// odd
    msg->snd_code = RC_capros_key_RequestError;
  else
    WaiterRequest(msg, menuNum, menuItemNum);
}

void
IntRequest(Message * msg, unsigned int menuNum, unsigned int menuItemNum,
  int32_t min, int32_t max)
{
  if (msg->rcv_w2 < min		// too low
      || msg->rcv_w2 > max )	// too high
    msg->snd_code = RC_capros_key_RequestError;
  else
    WaiterRequest(msg, menuNum, menuItemNum);
}

void
GetLogfile(Message * msg, capros_Node_extAddr_t ks_slot)
{
  if (msg->rcv_w1 >= numAdapters) {
    msg->snd_code = RC_capros_SWCA_noInverter;
  } else {
    capros_Node_getSlotExtended(KR_KEYSTORE, ks_slot + msg->rcv_w1,
                  KR_ARG(0));
    capros_Logfile_getReadOnlyCap(KR_ARG(0), KR_ARG(0));
    msg->snd_key0 = KR_ARG(0);
  }
}

// Create a Logfile and put the cap in KEYSTORE slot ks_slot.
// TEMP1 has the Logfile constructor.
// Uses TEMP0.
void
CreateLog(capros_Node_extAddr_t ks_slot)
{
  result_t result;
  result = capros_Constructor_request(KR_TEMP1, KR_BANK, KR_SCHED, KR_VOID,
               KR_TEMP0);
  assert(result == RC_OK);	// FIXME
  // Save data 32 days:
  capros_Logfile_setDeletionPolicyByID(KR_TEMP0, 32*24*60*60*1000000000ULL);
  result = capros_Node_swapSlotExtended(KR_KEYSTORE, ks_slot,
               KR_TEMP0, KR_VOID);
  assert(result == RC_OK);
}

int
cmte_main(void)
{
  result_t result;
  int i;

  Message Msg = {
    .snd_invKey = KR_VOID,
    .snd_key0 = KR_VOID,
    .snd_key1 = KR_VOID,
    .snd_key2 = KR_VOID,
    .snd_rsmkey = KR_VOID,
    .snd_len = 0,
    .snd_code = 0,
    .snd_w1 = 0,
    .snd_w2 = 0,
    .snd_w3 = 0,

    .rcv_key0 = KR_ARG(0),
    .rcv_key1 = KR_VOID,
    .rcv_key2 = KR_VOID,
    .rcv_rsmkey = KR_RETURN,
    .rcv_data = MsgBuf,
    .rcv_limit = sizeof(MsgBuf)
  };

  result = CMTETimer_setup();
  assert(result == RC_OK);

  // wait for things to settle:
  DEBUG(server) capros_Sleep_sleep(KR_SLEEP, 2000);

  DEBUG(server) kprintf(KR_OSTREAM, "swca started\n");

  // Allocate slots in KR_KEYSTORE:
  result = capros_SuperNode_allocateRange(KR_KEYSTORE,
                                          LKSN_SERIAL, LKSN_End - 1);
  if (result != RC_OK) {
    assert(result == RC_OK);	// FIXME
  }

  for (i = 0; i < numAdapters; i++) {
    AdapterState * as = &adapterStates[i];
    as->num = i;
  }
  ResetAdapterState(&dummyAdapter);
  dummyAdapter.num = -1;

  // Create logs.
  capros_Node_getSlotExtended(KR_CONSTIT, KC_LOGFILEC, KR_TEMP1);
  for (i = 0; i < numAdapters; i++) {
    CreateLog(LKSN_LEDLOGS + i);
    CreateLog(LKSN_InvChgLogs + i);
    CreateLog(LKSN_LoadLogs + i);
  }

  // Create key for input process to call us:
  result = capros_Process_makeStartKey(KR_SELF, keyInfo_notify, KR_TEMP0);
  assert(result == RC_OK);
  result = capros_Node_swapSlotExtended(KR_KEYSTORE, LKSN_NOTIFY, KR_TEMP0,
                                        KR_VOID);
  assert(result == RC_OK);

  unsigned int inputThreadNum;
  result = CMTEThread_create(2048, &InputProcedure, 0, &inputThreadNum);
  assert(result == RC_OK);	// FIXME

  for(;;) {
    // Before RETURNing, see if there is any work to be done.
    if (haveSerialKey) {
      CMTEMutex_lock(&lock);
      GetMonoTime();
      // Any task to do?
      if (jobState == js_none) {
  nextJob:
        // Find the next job to do.
        if (haveWaiter) {
          StartRequest();
        } else {
          NextTask();
        }
      }
      DEBUG(sched) kprintf(KR_OSTREAM, "js=%u ", jobState);
      switch (jobState) {
      default:
        assert(false);
      case js_waiting:
        if (haveWaiter) {
          // Interrupt the wait to start a request.
          StartRequest();
      case js_request:
          if (DoRequest())
            goto nextJob;
        } else {
          int64_t timeToNextTask = nextTaskTime - monoNow;
          if (timeToNextTask > 0) {
            // Nothing to do now but wait for the next task.
            // Set a timer to wake us up then.
            CMTETimer_setDuration(&tmr, timeToNextTask);
            DEBUG(time) kprintf(KR_OSTREAM,
                          "Task set timeout in %llu\n", timeToNextTask);
            CMTEMutex_unlock(&lock);
          } else {	// it is now time for the next task
            NextTask();	// select the next task to do
            assert(jobState == js_task);
      case js_task:
            if (DoMenu()) {
              // This should complete the current task.
              struct Task * ct = currentTask;
              assert(ct->adapterNum == transmittingAdapterNum);
              assert(ct->menuNum == wantedMenuNum);
              if (ct->menuNum == -1) {	// got LEDs
                DEBUG(leds) kprintf(KR_OSTREAM, "Read LEDs completed.\n");
                DEBUG(sched) kprintf(KR_OSTREAM, "Task (%d,leds) done.\n",
                                     ct->adapterNum);
              } else {
                assert(ct->menuItemNum == wantedMenuItemNum);
                DEBUG(sched) kprintf(KR_OSTREAM, "Task (%d,%d,%d) done.\n",
                               ct->adapterNum, ct->menuNum, ct->menuItemNum);
              }
              ct->nextSampleTime = curMenuMonoTime + ct->period;
              goto nextJob;
            }
          }
        }
      }
    }	// end of if haveSerialKey

    RETURN(&Msg);

    DEBUG(server) kprintf(KR_OSTREAM, "swca was called, keyInfo=%#x\n",
                          Msg.rcv_keyInfo);

    // Defaults for reply:
    Msg.snd_invKey = KR_RETURN;
    Msg.snd_code = RC_OK;
    Msg.snd_w1 = 0;
    Msg.snd_w2 = 0;
    Msg.snd_w3 = 0;
    Msg.snd_key0 = KR_VOID;

    switch (Msg.rcv_keyInfo) {
    case keyInfo_notify:
      /* The input process has called us for the sole purpose of ensuring
      that we check workToDo.
      Note, to prevent deadlock, we must never wait for the input process.
      To prevent input overrun, we must not take long on any operation. */
      break;

    case keyInfo_nplinkee:
      switch (Msg.rcv_code) {
      default:
        Msg.snd_code = RC_capros_key_UnknownRequest;
        break;

      case OC_capros_key_getType:
        Msg.snd_w1 = IKT_capros_NPLinkee;
        break;

      case OC_capros_NPLinkee_registerNPCap:
        COPY_KEYREG(KR_ARG(0), KR_SERIAL);
        // Return to the caller before invoking the serial cap,
        // to prevent deadlock.
        SEND(&Msg);

        InitSerialPort();

        result = capros_Node_swapSlotExtended(KR_KEYSTORE, LKSN_SERIAL,
                   KR_SERIAL, KR_VOID);
        assert(result == RC_OK);
        CMTESemaphore_up(&SerialCapSem);	// allow input proc to use it

        GetMonoTime();
        break;
      }
      break;

    case keyInfo_swca:
    {
      switch (Msg.rcv_code) {
      default:
        Msg.snd_code = RC_capros_key_UnknownRequest;
        break;

      case OC_capros_key_getType:
        Msg.snd_w1 = IKT_capros_SWCA;
        break;

      case OC_capros_SWCA_getLEDsLogfile:
        GetLogfile(&Msg, LKSN_LEDLOGS);
        break;

      case OC_capros_SWCA_getInvChgAmpsLogfile:
        GetLogfile(&Msg, LKSN_InvChgLogs);
        break;

      case OC_capros_SWCA_getLoadAmpsLogfile:
        GetLogfile(&Msg, LKSN_LoadLogs);
        break;

      // Requests:

      case OC_capros_SWCA_getBatteryVolts:
        WaiterRequest(&Msg, 3, 4);
        break;

      case OC_capros_SWCA_getGeneratorMode:
        WaiterRequest(&Msg, 1, 1);
        break;

      case OC_capros_SWCA_setGeneratorMode:
        if (Msg.rcv_w2 > capros_SWCA_GenMode_Eq)
          Msg.snd_code = RC_capros_key_RequestError;
        else
          WaiterRequest(&Msg, 1, 1);
        break;

      case OC_capros_SWCA_getLBCOVolts:
        WaiterRequest(&Msg, 8, 2);
        break;

      case OC_capros_SWCA_setLBCOVolts:
        VoltsRequest(&Msg, 8, 2, 320, 640);
        break;

      case OC_capros_SWCA_getBulkVolts:
        WaiterRequest(&Msg, 9, 1);
        break;

      case OC_capros_SWCA_setBulkVolts:
        VoltsRequest(&Msg, 9, 1, 400, 640);
        break;

      case OC_capros_SWCA_getAbsorptionTime:
        WaiterRequest(&Msg, 9, 2);
        break;

      case OC_capros_SWCA_setAbsorptionTime:
        IntRequest(&Msg, 9, 2, 0, 23*6+5);
        break;

      case OC_capros_SWCA_getFloatVolts:
        WaiterRequest(&Msg, 9, 3);
        break;

      case OC_capros_SWCA_setFloatVolts:
        VoltsRequest(&Msg, 9, 3, 400, 640);
        break;

      case OC_capros_SWCA_getEqualizeVolts:
        WaiterRequest(&Msg, 9, 4);
        break;

      case OC_capros_SWCA_setEqualizeVolts:
        VoltsRequest(&Msg, 9, 4, 400, 640);
        break;

      case OC_capros_SWCA_getEqualizeTime:
        WaiterRequest(&Msg, 9, 5);
        break;

      case OC_capros_SWCA_setEqualizeTime:
        IntRequest(&Msg, 9, 5, 0, 23*6+5);
        break;

      case OC_capros_SWCA_getMaxChargeAmps:
        WaiterRequest(&Msg, 9, 6);
        break;

      case OC_capros_SWCA_setMaxChargeAmps:
        IntRequest(&Msg, 9, 6, 1, 35);
        break;

      case OC_capros_SWCA_getGenAmps:
        WaiterRequest(&Msg, 10, 2);
        break;

      case OC_capros_SWCA_setGenAmps:
        IntRequest(&Msg, 10, 2, 0, 63);
        break;

      case OC_capros_SWCA_get24HourStartVolts:
        WaiterRequest(&Msg, 11, 4);
        break;

      case OC_capros_SWCA_set24HourStartVolts:
        VoltsRequest(&Msg, 11, 4, 200, 710);
        break;

      case OC_capros_SWCA_get2HourStartVolts:
        WaiterRequest(&Msg, 11, 5);
        break;

      case OC_capros_SWCA_set2HourStartVolts:
        VoltsRequest(&Msg, 11, 5, 200, 710);
        break;

      case OC_capros_SWCA_get15MinStartVolts:
        WaiterRequest(&Msg, 11, 6);
        break;

      case OC_capros_SWCA_set15MinStartVolts:
        VoltsRequest(&Msg, 11, 6, 200, 710);
        break;
      }
      break;
    }

    default:	// of keyInfo
    {
    }
    }
  }
}
