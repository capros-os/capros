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
#include <linuxk/linux-emul.h>
#include <linuxk/lsync.h>
#include <linux/kthread.h>
#include <linux/mutex.h>
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

/* Following should be an OR of some of the above */
#define dbg_flags   ( 0u | dbg_errors )

#define DEBUG(x) if (dbg_##x & dbg_flags)

#define KC_RTC 0

#define KR_RTC     KR_APP2(0)	// Input process only
#define KR_SERIAL  KR_APP2(1)
#define KR_NOTIFY  KR_APP2(2)	// Input process only

#define LKSN_SERIAL LKSN_APP	// holds the serial port key if we have one
#define LKSN_NOTIFY (LKSN_SERIAL+1)

#define keyInfo_nplinkee 0xffff	// nplink has this key
#define keyInfo_swca   0
#define keyInfo_notify 1

typedef capros_RTC_time_t RTC_time;		// real time, seconds
typedef capros_Sleep_nanoseconds_t mono_time;	// monotonic time since restart,
						// in nanoseconds

#define inBufEntries 100	// hopefully enough
struct InputPair {
  uint8_t flag;
  uint8_t data;
} __attribute__ ((packed))
inBuf[inBufEntries];

struct ValueAndTime {
  int val;
  RTC_time time;
};

#define numAdapters 8
typedef struct AdapterState {
  unsigned int num;	// 0 through numAdapters-1, fixed for this adapter
  char lcd[2][16];	// the 2-line LCD display
  char * cursor;
  char * underscore;	// NULL if none
  uint8_t LEDs;
  int menuNum;		// -1 if unknown
  int menuItemNum;

  // Values read from the inverter:
  // (Read under lock to ensure value and time match.)
  struct ValueAndTime invChg;	// value in amps AC
  struct ValueAndTime load;	// value in amps AC
} AdapterState;
AdapterState adapterStates[numAdapters];

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
unsigned int wantedAdapterNum;
// The menu we want to select (meaningless if wantedAdapterNum == -1):
int wantedMenuNum;
// The menu item we want to select (meaningless if wantedAdapterNum == -1):
int wantedMenuItemNum;
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

// After receiving a 227, the next character is the LEDs value:
bool nextIsLEDs;

DEFINE_MUTEX(inputProcMutex);
DEFINE_MUTEX(lock);

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

DEFINE_TIMER(tmr, &timerProcedure, 0, 0);

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
  result = capros_Sleep_getTimeMonotonic(KR_SLEEP, &monoNow);
  assert(result == RC_OK);
  DEBUG(time) kprintf(KR_OSTREAM, "monoNow set %llu\n", monoNow);
}

/****************************  Task management  ***************************/

/* We loop through the task list, going to each specified menu item,
 * to sample data. */
struct Task {
  mono_time period;	// how often, in ns
  unsigned int adapterNum;
  unsigned int menuNum;	// -1 means get LEDs
  unsigned int menuItemNum;
  mono_time lastSampled;	// monotonic time in ns
} taskList[] = {
#define oneThirdSec 333333333
#define twoSec 2000000000
//  {oneThirdSec, 0, -1},	// LED tasks not implemented yet
  {twoSec,      0, 3, 1},
  {twoSec,      0, 3, 3},
//  {oneThirdSec, 1, -1},
  {twoSec,      1, 3, 1},
  {twoSec,      1, 3, 3},
//  {oneThirdSec, 2, -1},
  {twoSec,      2, 3, 1},
  {twoSec,      2, 3, 3},
};
#define numInverters 3
#define numTasks (sizeof(taskList) / sizeof(struct Task))
struct Task * currentTask = &taskList[0];
mono_time soonestTaskTime;

// Find the next task to do.
// monoNow must be up to date.
// Call under lock.
void
NextTask(void)
{
  soonestTaskTime = infiniteTime;
  struct Task * loopStart = currentTask;
  do {
    if (++currentTask >= &taskList[numTasks])
      currentTask = &taskList[0];	// wrap
    // Do we need to do this task now?
    mono_time timeToDo = currentTask->lastSampled + currentTask->period;
    if (timeToDo <= monoNow) {		// do it now
      DEBUG(sched) kprintf(KR_OSTREAM, "Starting task (%d,%d,%d).\n",
                     currentTask->adapterNum, currentTask->menuNum,
                     currentTask->menuItemNum);
      wantedAdapterNum = currentTask->adapterNum;
      transmittingAdapterNum = ta_NeedFirstSelect;
      assert(currentTask->menuNum >= 0);//// LEDs not implemented yet
      wantedMenuNum = currentTask->menuNum;
      wantedMenuItemNum = currentTask->menuItemNum;
      soonestTaskTime = 0;	// signal that this task is in progress
      return;
    } else {		// do it later
      if (soonestTaskTime > timeToDo )	// take min
        soonestTaskTime = timeToDo;
    }
  } while (currentTask != loopStart);
  // No task needs to be done now. Return with soonestTaskTime > monoNow.
  DEBUG(sched) kprintf(KR_OSTREAM, "No task till %lld.\n", soonestTaskTime);
}

// We have lost synchronization with the input stream.
void
ResetInputState(void)
{
  transmittingAdapterNum = ta_NeedFirstSelect;
  /* Set nextIsLEDs is true to ignore the next character received,
  because it might be an LEDs value, which could be any value. */
  nextIsLEDs = true;
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
}

/********************** Menu item procedures *****************************/
/* These are called by the Input process only. */

// All return true iff input is invalid.

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
procValue(AdapterState * as, struct ValueAndTime * vt)
{
  int value;
  if (ConvertInt(as, &value))
    return true;
  vt->val = value;
  vt->time = GetRTCTime();
  return false;
}

bool
procInvChg(AdapterState * as)
{
  return procValue(as, &as->invChg);
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
  return procValue(as, &as->load);
}

bool
procBattAct(AdapterState * as)
{
  int value;
  if (Convert2p1(as, &value))
    return true;
  // Value not currently stored.
  return false;
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
  [1][1] = {{"Set Generator   ","OFF AUTO  ON EQ "}},
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
  [3][4] = {{"Battery actual  ","volts DC        "}, procBattAct},
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
  [8][2] = {{"Set Low battery ","cut out VDC     "}, proc2p1, 0, 11},
  [8][3] = {{"Set LBCO delay  ","minutes         "}, procInt},
  [8][4] = {{"Set Low battery ","cut in VDC      "}, proc2p1, 0, 11},
  [8][5] = {{"Set High battery","cut out VDC     "}, proc2p1},
  [8][6] = {{"Set search      ","watts           "}, procInt, 0, 7},
  [8][7] = {{"Set search      ","spacing         "}, procInt, 0, 7},

  [9][0] = {{"Battery Charging","              10"}},

  [10][0] = {{"AC Inputs       ","              11"}},

  [11][0] = {{"Gen Auto Start  ","setup         12"}},
  [11][1] = {{"Set Load Start  ","amps AC         "}, procInt, 2, 9},
  [11][2] = {{"Set Load Start  ","delay min       "}, proc2p1, 0, 9},
  [11][3] = {{"Set Load Stop   ","delay min       "}, proc2p1},
  [11][4] = {{"Set 24 hr start ","volts DC        "}, proc2p1},
  [11][5] = {{"Set 2  hr start ","volts DC        "}, proc2p1},
  [11][6] = {{"Set 15 min start","volts DC        "}, proc2p1},
  [11][7] = {{"Read LBCO 30 sec","start VDC       "}, proc2p1},
  [11][8] = {{"Set Exercise    ","period days     "}, procInt},

  [12][0] = {{"Gen starting    ","details       13"}},

  [13][0] = {{"Auxiliary Relays","R9 R10 R11    14"}},

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
                          *(p-2), c8, c8);
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
    DEBUG(errors) kdprintf(KR_OSTREAM, "Convert2p1 of %c%c%c%c failed!\n",
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

// Call not under lock.
void
CheckEndOfField(AdapterState * as)
{
  mutex_lock(&lock);
  if (as->menuNum >= 0) {	// we are on a known menu
    struct MenuItem * mi = &menuItems[as->menuNum][as->menuItemNum];
    if (as->cursor + mi->valueFieldEnd == &as->lcd[1][16]) {
      // We just received the last character of a value field.
      bool (*valueProc)(AdapterState *) = mi->valueProc;
      if (valueProc) {
        if ((*valueProc)(as))
          ;	// invalid value: what to do here?
        else {		// successful
          // Does this complete the current task?
          if (soonestTaskTime == 0) {	// there is a current task
            struct Task * ct = currentTask;
            if (ct->adapterNum == transmittingAdapterNum
                && ct->menuNum == as->menuNum
                && ct->menuItemNum == as->menuItemNum) {	// it does
              DEBUG(sched) kprintf(KR_OSTREAM, "Task (%d,%d,%d) done.\n",
                 currentTask->adapterNum, currentTask->menuNum,
                 currentTask->menuItemNum);
              GetMonoTime();
              currentTask->lastSampled = monoNow;
              NextTask();
              mutex_unlock(&lock);
              InputNotifyServer();	// not under lock!
              return;
            }
          }
        }
      }
    }
  }
  mutex_unlock(&lock);
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
bool
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
    return true;	// it was a printable character
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
    // Don't know what the following mean:
    case 160:
    case 224:
      break;

    case 225:
      as->underscore = as->cursor;
      if (as->menuNum >= 0) {	// we are on a known menu
        struct MenuItem * mi = &menuItems[as->menuNum][as->menuItemNum];
        bool (*underscoreProc)(AdapterState *) = mi->underscoreProc;
        if (underscoreProc)
          // Call procedure under lock:
          if ((*underscoreProc)(as))
            ;	// invalid position; what to do here?
      }
      break;

    case 227:
      nextIsLEDs = true;
      break;
    }
  }
  return false;	// not a printable character
}

// The input thread executes this procedure.
int
InputProcedure(void * data /* unused */ )
{
  result_t result;

  result = capros_Node_getSlotExtended(KR_KEYSTORE, LKSN_NOTIFY, KR_NOTIFY);
  assert(result == RC_OK);
  result = capros_Node_getSlotExtended(KR_CONSTIT, KC_RTC, KR_RTC);
  assert(result == RC_OK);

  mutex_lock(&inputProcMutex);	// wait for the SerialPort key
  result = capros_Node_getSlotExtended(KR_KEYSTORE, LKSN_SERIAL, KR_SERIAL);
  assert(result == RC_OK);

  DEBUG(input) kprintf(KR_OSTREAM, "Input process starting\n");

  for (;;) {
    uint32_t pairsRcvd;
    result = capros_SerialPort_read(KR_SERIAL, 
               inBufEntries, &pairsRcvd, (uint8_t *)&inBuf[0]);
    if (result != RC_OK) {
      DEBUG(errors) kdprintf(KR_OSTREAM,
                      "SerialPort_read returned %#x\n", result);
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
        break;
      case capros_SerialPort_Flag_PARITY:
        DEBUG(errors) kprintf(KR_OSTREAM, "PARITY ");
        ResetInputState();
        break;
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
      mutex_lock(&lock);
      AdapterState * as = (transmittingAdapterNum >= 0
                     ? &adapterStates[transmittingAdapterNum] : &dummyAdapter);
      if (nextIsLEDs) {
        as->LEDs = c;
        nextIsLEDs = false;
        mutex_unlock(&lock);
      } else {
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
          mutex_unlock(&lock);
          break;

        case ta_InFirstScreen:
          if (ProcessCharacter(as, c)) {	// process char w/ dummyAdapter
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
          if (ProcessCharacter(as, c)) {
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
                mutex_unlock(&lock);
                // Skip CheckEndOfField, because the field may not be
                // filled in yet.
                InputNotifyServer();
              } else {
                mutex_unlock(&lock);
                CheckEndOfField(as);
              }
            } else {	// printable character not at end of screen
              mutex_unlock(&lock);
              CheckEndOfField(as);
            }
          } else {
            mutex_unlock(&lock);
          }
        }	// end of switch (transmittingAdapterNum)
      }		// end of not nextIsLEDs
      nochar: ;
    }
  }
  // SerialPort key no longer valid.
  assert(false);// not implemented
  return 0;
}

/****************************  Server stuff  *****************************/

void
InitSerialPort(void)
{
  result_t result;
  uint32_t openErr;

  result = capros_SerialPort_open(KR_SERIAL, &openErr);
  assert(result == RC_OK);

  struct capros_SerialPort_termios2 termio = {
    .c_iflag = capros_SerialPort_BRKINT | capros_SerialPort_INPCK,
    /* 8 bits No parity 1 stop bit: */
    .c_cflag = capros_SerialPort_CS8 | capros_SerialPort_CREAD,
    .c_line = 0,
    .c_ospeed = 9600
  };
  result = capros_SerialPort_setTermios2(KR_SERIAL, termio);
  assert(result == RC_OK);

  ResetInputState();
  haveSerialKey = true;
}

#define selectTimeout 1000000000 // one second
void
SelectAdapter(unsigned int num)	// 0-7
{
  result_t result;
  uint8_t sendData[2];

  assert(num < numAdapters);
  sendData[0] = num + 1;
//  sendData[1] = 227;	// code to get LEDs
  selectTime = monoNow;
  capros_mod_timer_duration(&tmr, selectTimeout);
  DEBUG(time) kprintf(KR_OSTREAM, "SelectAdapter set timeout\n");
  result = capros_SerialPort_write(KR_SERIAL, 1/*2*/, &sendData[0]);
  assert(result == RC_OK);
}

void
SendCommand(uint8_t cmd)
{
  result_t result;

  commandTime = monoNow;
  result = capros_SerialPort_write(KR_SERIAL, 1, &cmd);
  assert(result == RC_OK);
}

void
DoGetValue(Message * msg, AdapterState * as, struct ValueAndTime * vt)
{
  if (msg->rcv_w1 >= numInverters) {
    msg->snd_code = RC_capros_SWCA_noInverter;
    return;
  }
  // Note, as and vt are not validated until this point.
  // Don't use them before this point.
  mutex_lock(&lock);
  RTC_time t = vt->time;
  int val = vt->val;
  mutex_unlock(&lock);
  if (t == 0) {
    msg->snd_code = RC_capros_SWCA_noData;
    return;
  }
  msg->snd_w1 = val;
  msg->snd_w2 = t;
}

uint32_t MsgBuf[16/4];

int
driver_main(void)
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

  // wait for things to settle:
  DEBUG(server) capros_Sleep_sleep(KR_SLEEP, 2000);

  DEBUG(server) kprintf(KR_OSTREAM, "swca started\n");

  // Allocate slot in KR_KEYSTORE:
  result = capros_SuperNode_allocateRange(KR_KEYSTORE,
                                          LKSN_SERIAL, LKSN_NOTIFY);
  if (result != RC_OK) {
    assert(result == RC_OK);	// FIXME
  }

  // Create key for input process to call us:
  result = capros_Process_makeStartKey(KR_SELF, keyInfo_notify, KR_TEMP0);
  assert(result == RC_OK);
  result = capros_Node_swapSlotExtended(KR_KEYSTORE, LKSN_NOTIFY, KR_TEMP0,
                                        KR_VOID);
  assert(result == RC_OK);

  for (i = 0; i < numAdapters; i++) {
    AdapterState * as = &adapterStates[i];
    as->num = i;
  }
  ResetAdapterState(&dummyAdapter);
  dummyAdapter.num = -1;

  mutex_lock(&inputProcMutex);
  struct task_struct * inputThread = kthread_run(&InputProcedure, 0, "");
  if (IS_ERR(inputThread)) {
    assert(false);	// FIXME
  }

  for(;;) {
    // Before RETURNing, see if there is any work to be done.
    if (haveSerialKey) {
checkWork:
      GetMonoTime();
      mutex_lock(&lock);
      DEBUG(sched) kprintf(KR_OSTREAM, "taNum=%d ", transmittingAdapterNum);
      switch (transmittingAdapterNum) {
      case ta_NeedFirstScreen:
      case ta_InFirstScreen:
      case ta_NeedSecondScreen: ;
        // Have we timed out?
        int64_t timeToWait = selectTime + selectTimeout - monoNow;
        if (timeToWait <= 0) {	// timed out
          DEBUG(errors) kprintf(KR_OSTREAM, "Selecting %d timed out\n",
                                wantedAdapterNum);
          transmittingAdapterNum = ta_NeedFirstSelect;
          if (--selectRetries == 0) {		// too many retries
            DEBUG(errors)
              printk("Too many retries to select adapter; selecting a different adapter\n");
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
#define commandTimeout 1000000000 // 1 second
        if (as->menuNum != wantedMenuNum
            || as->menuItemNum != wantedMenuItemNum ) {
          // Not on the menu item we want.
          mutex_unlock(&lock);
          while (1) {	// loop at most twice
            char cmd;
            if (as->menuNum == -1) { // current menu unknown, or executing cmd
              // Allow time for the previous command:
              int64_t timeToWait = commandTime + commandTimeout - monoNow;
              if (timeToWait > 0) {
                // Give the previous command some time to work.
                capros_mod_timer_duration(&tmr, timeToWait);
                DEBUG(time) kprintf(KR_OSTREAM,
                              "Select menu set timeout in %llu\n", timeToWait);
                DEBUG(sched) kprintf(KR_OSTREAM, "Wait for cmd for %lld\n",
                                     timeToWait);
                break;
              }
              // current menu is unknown, and previous command timed out
              if (--menuRetries == 0) {		// too many retries
                DEBUG(errors)
                  printk("Menu select timed out; reselecting adapter\n");
                // Try reselecting the adapter:
                transmittingAdapterNum = ta_NeedFirstSelect;
                goto checkWork;
              }
              cmd = 'U';	// menu item up
            } else {		// current menu is known
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
                bool useS;
                if (wantedMenuNum >= setupMenuNum) {
                  // Going to a setup menu.
                  useS = as->menuNum < setupMenuNum	// must go to setup
                         // else already in setup menus, but
                         // go directly to Setup Menu if it is shorter:
                         || wantedMenuNum - setupMenuNum + 1 < diff;
                } else {
                  // Going to a non-setup menu.
                  // Go directly to the Setup menu if it is shorter:
                  useS = setupMenuNum + 1 - wantedMenuNum
                         < (diff >= 0 ? diff : -diff);
                }
                if (useS)
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
          }
          mutex_lock(&lock);
        } else {
          // we have the menu item we want
          // Any task to do?
          DEBUG(time) kprintf(KR_OSTREAM, "stt=%llu ", soonestTaskTime);
          if (soonestTaskTime != 0) {	// no task is in progress
            int64_t timeToSoonestTask = soonestTaskTime - monoNow;
            if (timeToSoonestTask > 0) {
              // Wait until time for a task.
              capros_mod_timer_duration(&tmr, timeToSoonestTask);
              DEBUG(time) kprintf(KR_OSTREAM,
                            "Task set timeout in %llu\n", timeToSoonestTask);
              DEBUG(sched) kprintf(KR_OSTREAM, "Wait for task time\n");
            } else {
              NextTask();	// select the next task to do
              assert(soonestTaskTime == 0);
              mutex_unlock(&lock);
              goto checkWork;
            }
          } else {
            DEBUG(sched) kprintf(KR_OSTREAM, "Waiting for task ");
            // Just let the current task finish.
            del_timer(&tmr);	// currently no timeout for this
          }
        }
      }		// end of switch (transmittingAdapterNum)
      mutex_unlock(&lock);
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

        mutex_unlock(&inputProcMutex);
        GetMonoTime();
        NextTask();	// select the first task
        break;
      }
      break;

    case keyInfo_swca:
    {
      AdapterState * as;
      switch (Msg.rcv_code) {
      default:
        Msg.snd_code = RC_capros_key_UnknownRequest;
        break;

      case OC_capros_key_getType:
        Msg.snd_w1 = IKT_capros_SWCA;
        break;

      case OC_capros_SWCA_getInvChgAmps:
        as = &adapterStates[Msg.rcv_w1];
        DoGetValue(&Msg, as, &as->invChg);
        break;

      case OC_capros_SWCA_getLoadAmps:
        as = &adapterStates[Msg.rcv_w1];
        DoGetValue(&Msg, as, &as->load);
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
