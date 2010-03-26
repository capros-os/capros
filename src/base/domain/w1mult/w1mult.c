/*
 * Copyright (C) 2008-2010, Strawberry Development Group.
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
#include <eros/fls.h>
#include <eros/target.h>
#include <eros/container_of.h>
#include <eros/machine/cap-instr.h>

#include <idl/capros/key.h>
#include <idl/capros/Node.h>
#include <idl/capros/Process.h>
#include <idl/capros/Sleep.h>
#define infiniteTime 18446744073709000000ULL
#include <idl/capros/Constructor.h>
#include <idl/capros/SuperNode.h>
#include <idl/capros/W1Mult.h>
#include <idl/capros/DS2450.h>

#include <domain/domdbg.h>
#include <domain/assert.h>

#include "w1mult.h"
#include "w1multConfig.h"
#include "DS18B20.h"
#include "DS2438.h"
#include "DS2450.h"

/* Bypass all the usual initialization. */
unsigned long __rt_runtime_hook = 0;

/* Memory:
  0: nothing
  0x01000: code
  0x1d000: main stack
  0x1e000: timer stack
  0x1f000: config file */
unsigned long __rt_stack_pointer = 0x1e000;
#define timer_stack_pointer 0x1f000
#define configFileAddr 0x1f000

static void ScanBus(void);

bool haveBusKey = false;	// no key in KR_W1BUS
bool haveNextBusKey = false;	// no key in KR_NEXTW1BUS
bool busNeedsReinit;

capros_Sleep_nanoseconds_t latestConvertTTime = 0;

#define maxDevices 50

struct W1Device devices[maxDevices];

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

/*************************** SuperNode stuff ***************************/

unsigned int nextSnodeSlot = 0;
// Slots in the SuperNode are never deallocated.

unsigned int
AllocSuperNodeSlot(void)
{
  result_t result;
  result = capros_SuperNode_allocateRange(KR_KEYSTORE,
             nextSnodeSlot, nextSnodeSlot);
  if (result != RC_OK) {
    DEBUG(errors)
      kprintf(KR_OSTREAM, "w1mult SuperNode_allocateRange got %#x!\n", result);
    assert(result == RC_OK);	// FIXME
  }
  return nextSnodeSlot++;
}

result_t
CreateLog(int32_t * pSlot)
{
  result_t result;

  unsigned int slot = AllocSuperNodeSlot();
  capros_Node_getSlotExtended(KR_CONSTIT, KC_LOGFILEC, KR_TEMP1);
  result = capros_Constructor_request(KR_TEMP1, KR_BANK, KR_SCHED, KR_VOID,
               KR_TEMP0);
  if (result != RC_OK)
    return result;
  // Save data 32 days:
  capros_Logfile_setDeletionPolicyByID(KR_TEMP0, 32*24*60*60*1000000000ULL);

  result = capros_Node_swapSlotExtended(KR_KEYSTORE, slot,
               KR_TEMP0, KR_VOID);
  assert(result == RC_OK);
  *pSlot = slot;
  return RC_OK;
}

void
GetLogfile(unsigned int slot)
{
  result_t result;
  result = capros_Node_getSlotExtended(KR_KEYSTORE, slot, KR_TEMP0);
  assert(result == RC_OK);
  result = capros_Logfile_getReadOnlyCap(KR_TEMP0, KR_TEMP0);
  assert(result == RC_OK);
}

/* Add a log record.
 * Returns true iff record was successfully added.
 */
bool
AddLogRecord16(unsigned int slot,
  capros_RTC_time_t sampledRTC,
  capros_Sleep_nanoseconds_t sampledTime,
  int16_t value, int16_t param)
{
  result_t result;
  capros_Logfile_LogRecord16 rec16;

  rec16.header.length = rec16.trailer = sizeof(rec16);
  rec16.header.rtc = sampledRTC;
  rec16.header.id = sampledTime;
  rec16.value = value;
  rec16.param = param;

  result = capros_Node_getSlotExtended(KR_KEYSTORE, slot, KR_TEMP0);
  assert(result == RC_OK);
  result = capros_Logfile_appendRecord(KR_TEMP0,
             sizeof(rec16), (uint8_t *)&rec16);
  switch (result) {
  default:	// should not happen
    kdprintf(KR_OSTREAM, "%s:%d: result=%#x\n", __FILE__, __LINE__, result);
    return false;	// Not much we can do but drop the record.

  case RC_capros_Logfile_Full:
    DEBUG(errors) kprintf(KR_OSTREAM, "W1Mult log %u full!\n", slot);
    return false;	// Not much we can do but drop the record.

  case RC_capros_SpaceBank_LimitReached:
    DEBUG(errors) kprintf(KR_OSTREAM, "W1Mult log %u out of space!\n", slot);
    return false;	// Not much we can do but drop the record.

  case RC_OK:
    return true;
  }
}

/*************************** Branch stuff ***************************/

struct Branch root = {
  .whichBranch = branchRoot
};

/* If we have programmed a reset for a branch, resetBranch identifies it,
 * otherwise it is NULL.
 * This helps us avoid unnecessary resets.
 * A reset takes about 1 ms, so it's worth avoiding. */
struct Branch * resetBranch;

// Initialize a branch.
void
Branch_Init(struct Branch * br)
{
  br->childCouplers = NULL;
  br->childDevices = NULL;
  br->needsWork = false;
  br->shorted = false;
}

struct W1Device *
BranchToCoupler(struct Branch * br)
{
  switch (br->whichBranch) {
  case branchMain:
    return container_of(br, struct W1Device, u.coupler.mainBranch);
  case branchAux:
    return container_of(br, struct W1Device, u.coupler.auxBranch);
  default:
    assert(false);
  case branchRoot:
    return NULL;
  }
}

/************************** time stuff  ****************************/

capros_Sleep_nanoseconds_t currentTime;
capros_RTC_time_t currentRTC;

volatile	// because shared between threads
capros_Sleep_nanoseconds_t timeToWake = infiniteTime;

// An ordered list of w1Timer's:
Link timerHead;

void
InsertTimer(struct w1Timer * timer)
{
  DEBUG(timer) kprintf(KR_OSTREAM,
                 "InsertTimer tim=%#x %llu head=(%#x, %#x)\n",
                 timer, timer->expiration/1000000,
                 timerHead.next, timerHead.prev);
  assert(link_isSingleton(&timer->link));
  // Insert into the ordered list.
  Link * cur = &timerHead;
  Link * nxt;
  while (1) {
    nxt = cur->next;
    if (nxt == &timerHead)
      break;	// at end of list
    struct w1Timer * nxtTimer = container_of(nxt, struct w1Timer, link);
    if (nxtTimer->expiration >= timer->expiration)
      break;    // insert before nxt
    cur = nxt;
  }
  link_insertBetween(&timer->link, cur, nxt);
}

void
RecordCurrentTime(void)
{
  result_t result;
  result = capros_Sleep_getPersistentMonotonicTime(KR_SLEEP, &currentTime);
  assert(result == RC_OK);
}

void
RecordCurrentRTC(void)
{
  result_t result;
  result = capros_RTC_getTime(KR_RTC, &currentRTC);
  assert(result == RC_OK);
}

unsigned int crc;

uint8_t
CalcCRC8(uint8_t * data, unsigned int len)
{
  int i, j;
  crc = 0;

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

void
CalcCRC16Byte(unsigned char c)
{
  int i;
  for (i = 0; i < 8; i++) {     // for each bit of data
    // Use low bit of c.
    if ((crc ^ c) & 0x1) {
      crc ^= 0x14002;
    }
    crc >>= 1;
    c >>= 1;
  }
}

void
ProgramByteCRC16(uint8_t c)
{
  wp(c)
  CalcCRC16Byte(c);
}

bool	// true iff OK
CheckCRC16(void)
{
  uint8_t c1 = inBuf[0];
  uint8_t c2 = inBuf[1];

  bool ok = (((c2 << 8) + c1) ^ crc) == 0xffff;
  if (! ok) {
    DEBUG(errors) kprintf(KR_OSTREAM,
                   "CRC16 calc = 0x%.4x, read = 0x%.2x%.2x\n", crc, c2, c1);
  }
  return ok;
}

/************** Stuff for programming the 1-Wire bus *******************/

unsigned char outBuf[capros_W1Bus_maxProgramSize + 1];
unsigned char * const outBeg = &outBuf[0];
unsigned char * outCursor;
unsigned char inBuf[capros_W1Bus_maxReadSize];

/* After a program is executed, we can do some post-processing on some steps. */
#define maxPPItems 20
struct PPItem {
  unsigned short itemLoc;	/* the number of bytes of the program
		to this point including this step */
  /* After executing the program, we call function(status, arg). */
  void (*function)(capros_W1Bus_StatusCode status, void * arg);
  void * arg;
} PPItems[maxPPItems],
  * nextPPItem;

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

void
AddPPItem(
  void (*function)(capros_W1Bus_StatusCode status, void * arg),
  void * arg)
{
  nextPPItem->itemLoc = outCursor - outBeg;
  nextPPItem->function = function;
  nextPPItem->arg = arg;
  nextPPItem++;
}

void
ClearProgram(void)
{
  outCursor = outBeg;
  nextPPItem = &PPItems[0];
}

void
WriteOneByte(uint8_t b)
{
  wp(capros_W1Bus_stepCode_writeBytes)
  wp(1)
  wp(b)
  NotReset();
}

static void
AllDevsNotFound(void)
{
  int i;
  for (i = 0; i < numDevices; i++) {
    struct W1Device * dev = &devices[i];
    dev->found = false;
    // We may have lost the state of the devices:
    if (w1dev_IsCoupler(dev))
        dev->u.coupler.activeBranch = branchUnknown;
  }
}

// Return true if Restart exception or void key, false if OK.
static bool
CheckRestart(result_t result)
{
  if (result == RC_capros_key_Restart || result == RC_capros_key_Void) {
    // No bus key, so don't continue heartbeat:
    if (haveBusKey) {
      haveBusKey = false;
      if (! haveNextBusKey)
        DisableHeartbeat(hbBit_bus);
    }
    AllDevsNotFound();
    return true;
  }
  if (result != RC_OK)
    DEBUG(errors) kprintf(KR_OSTREAM, "CheckRestart result=%#x\n", result);
  assert(result == RC_OK);
  return false;
}

// If no exception, returns false.
// If an expected exception, returns true.
static bool
CheckModeResult(result_t result)
{
  if (result == RC_capros_W1Bus_BusError)
    return false;
  return CheckRestart(result);
}

/* Run the program from outBeg to outCursor.
Returns:
  -1 if W1Bus cap is gone
  else a W1Bus.StatusCode (but not capros_W1Bus_StatusCode_SysRestart).
 */
int
RunProgram(void)
{
  int returnValue;
  RunPgmMsg.snd_len = outCursor - outBeg;
  result_t result = CALL(&RunPgmMsg);
  if (CheckRestart(result)) {
    returnValue = -1;
    goto exit;
  }

  uint32_t status = RunPgmMsg.rcv_w1;
  returnValue = status;	// default

  switch (status) {
  case capros_W1Bus_StatusCode_ProgramError:
  case capros_W1Bus_StatusCode_SequenceError:
  default:
    // These errors indicate a software bug.
    kdprintf(KR_OSTREAM, "w1mult RunProgram got status %d!!\n", status);
    break;	// this is unrecoverable

  unsigned int errorLoc;

  case capros_W1Bus_StatusCode_OK:
    errorLoc = outCursor - outBeg;
    goto doPP;

  case capros_W1Bus_StatusCode_BusShorted:
  case capros_W1Bus_StatusCode_BusError:
    busNeedsReinit = true;
  // The following are not too serious:
  case capros_W1Bus_StatusCode_CRCError:
  case capros_W1Bus_StatusCode_Timeout:
    DEBUG(errors)
      kprintf(KR_OSTREAM, "w1mult RunProgram got status %d\n", status);
    goto errPP;

  case capros_W1Bus_StatusCode_SysRestart:
    returnValue = -1;
    AllDevsNotFound();
errPP:
  case capros_W1Bus_StatusCode_AlarmingPresencePulse:
  case capros_W1Bus_StatusCode_NoDevicePresent:
    errorLoc = RunPgmMsg.rcv_w2;	// # bytes successfully executed

doPP: ;
    // Do post-processing:
    struct PPItem * ppi;
    for (ppi = &PPItems[0]; ppi < nextPPItem; ppi++) {
      if (ppi->itemLoc <= errorLoc) {
        // This item was successfully executed.
        (*ppi->function)(capros_W1Bus_StatusCode_OK, ppi->arg);
      } else {
        // The error occurred on this step.
        (*ppi->function)(status, ppi->arg);
        break;		// no other steps ran
      }
    }
  }
exit:
  ClearProgram();	// set up for the next program
#if 0
  if (returnValue)
    kprintf(KR_OSTREAM, "w1mult: RunProgram returning %d.\n", returnValue);
#endif
  return returnValue;
}

/************************* Heartbeat stuff ************************/

uint32_t heartbeatCount;
capros_Sleep_nanoseconds_t lastHeartbeatTime;
uint32_t heartbeatDisable = hbBit_bus;	// no bus key yet

static void
HeartbeatAction(void * arg)
{
  RecordCurrentTime();
  lastHeartbeatTime = currentTime;

  // Let each type of device do its thing:
/* DS18B20 and DS2438 both respond to Convert T (0x44).
 * Thus when the DS18B20 heartbeat broadcasts a Convert T command
 * (that is, issues it after a Skip ROM), some DS2438's may also convert.
 * (Note, a temperature conversion takes 10 ms for a DS2438 
 * and up to 750 ms for a DS18B20.)
 * We don't want a DS2438 to receive a Convert T command while it is
 * already busy doing something else (such as a Convert V),
 * because it might get confused.
 * Therefore, DS2438_HeartbeatAction will initiate any necessary conversions
 * AND WAIT for them to complete (only 4 ms for a Convert V),
 * before continuing on to DS18B20_HeartbeatAction.
 *
 * Conversely, DS2438_HeartbeatAction will never broadcast Convert T,
 * to avoid busying DS18B20's. 
 */
  DS2438_HeartbeatAction(heartbeatCount);
  DS18B20_HeartbeatAction(heartbeatCount);
  DS2450_HeartbeatAction(heartbeatCount);
  heartbeatCount++;

  EnableHeartbeat(hbBit_timer);	// Schedule next heartbeat if we can
}

struct w1Timer heartbeatTimer = {
  .link = link_Initializer(heartbeatTimer.link),
  .function = &HeartbeatAction
};

void
DisableHeartbeat(uint32_t bit)
{
  assert(link_isSingleton(&heartbeatTimer.link));
  assert(! (heartbeatDisable & bit));
  heartbeatDisable |= bit;
}

#if (dbg_flags & dbg_doall)
#define heartbeatInterval 10000000000ULL	// 10 seconds in nanoseconds
#else
#define heartbeatInterval 1000000000ULL	// 1 second in nanoseconds
#endif

void
EnableHeartbeat(uint32_t bit)
{
  assert(heartbeatDisable & bit);
  if ((heartbeatDisable &= ~bit) == 0) {
    // Finished the current heartbeat.
    if (haveNextBusKey) {
      // Start using the new bus key.
      COPY_KEYREG(KR_NEXTW1BUS, KR_W1BUS);
      haveNextBusKey = false;
      haveBusKey = true;
      busNeedsReinit = true;
    }
    if (busNeedsReinit) {
      ScanBus();
    }
    if (heartbeatDisable == 0) {	// ScanBus was successful
      heartbeatTimer.expiration = lastHeartbeatTime + heartbeatInterval;
      DisableHeartbeat(hbBit_timer);
      InsertTimer(&heartbeatTimer);
    }
  }
}

static void
PPSmartOn(capros_W1Bus_StatusCode status, void * arg)
{
  struct Branch * br = arg;
  struct W1Device * coup = BranchToCoupler(br);
  switch (status) {
  case capros_W1Bus_StatusCode_OK:
  case capros_W1Bus_StatusCode_NoDevicePresent:
    coup->u.coupler.activeBranch = br->whichBranch;
    break;

  case capros_W1Bus_StatusCode_BusShorted:
    // The requested branch is not activated:
    if (coup->u.coupler.activeBranch == br->whichBranch)
      coup->u.coupler.activeBranch = branchUnknown;
    br->shorted = true;
    kdprintf(KR_OSTREAM, "Coup %#.16llx br %#x is shorted!!\n",
             coup->rom, br->whichBranch);
    break;

  default:
    kprintf(KR_OSTREAM, "Smart-on coup %#.16llx br %#x got status %d\n",
            coup->rom, br->whichBranch, status);
  }
}

void
PPCouplerDirectOnMain(capros_W1Bus_StatusCode status, void * arg)
{
  struct W1Device * coup = arg;
  if (status == capros_W1Bus_StatusCode_OK) {
    if (inBuf[0] == 0xa5) {	// got correct confirmation byte
      coup->u.coupler.activeBranch = branchMain;
    } else {		// wrong confirmation byte
      // Treat as bus error.
      status = capros_W1Bus_StatusCode_BusError;
      goto error;
    }
  } else {
error:
    kprintf(KR_OSTREAM, "Direct on main coup %#.16llx got status %d\n",
            coup->rom, status);
  }
}

void
ProgramSmartOn(struct W1Device * coup, struct Branch * br)
{
  assert(coup == BranchToCoupler(br));
  
  wp(br->whichBranch)	// capros_W1Bus_stepCode_setPath*
  CopyToProgram(&coup->rom, 8);
  resetBranch = br;
  AddPPItem(PPSmartOn, br);
}

void
ProgramReset(void)
{
  wp(capros_W1Bus_stepCode_resetSimple);
  resetBranch = &root;
}

/* Address a path through couplers.

If mustSmartOn is true, the last coupler will always be given
a Smart-On command (useful for limiting a search to that branch).
*/
void
AddressPath(struct Branch * branch, bool mustSmartOn)
{
  ProgramReset();
  // Set up path through any couplers.
  // Address parents from the bus root to the device. 
  struct Branch * addressedSoFar = &root;
  while (branch != addressedSoFar) {
    struct Branch * pbr;
    struct W1Device * coup;
    for (pbr = branch;
         coup = BranchToCoupler(pbr),
           coup->parentBranch != addressedSoFar;
         pbr = coup->parentBranch) {
    }
    if ((mustSmartOn && pbr == branch)
        || coup->u.coupler.activeBranch != pbr->whichBranch) {
      ProgramSmartOn(coup, pbr);
    }
    addressedSoFar = pbr;
  }
}

void
ProgramMatchROM(struct W1Device * dev)
{
  wp(capros_W1Bus_stepCode_matchROM)
  CopyToProgram(&dev->rom, 8);	// little endian
  NotReset();
}

void
AddressDevice(struct W1Device * dev)
{
  AddressPath(dev->parentBranch, false);
  ProgramMatchROM(dev);
}

/********************************  DoAll stuff  ******************************/

static void
MarkDevForSampling(struct W1Device * dev)
{
  dev->sampling = true;
  // Mark all parent branches as needsWork too.
  struct Branch * pbr = dev->parentBranch;
  struct W1Device * coup;
  while (1) {
    pbr->needsWork = true;
    coup = BranchToCoupler(pbr);
    if (!coup)
      break;
    pbr = coup->parentBranch;
  }
}

void
MarkSamplingList(struct W1Device * dev)
{
  while (dev) {
    MarkDevForSampling(dev);
    dev = dev->nextInSamplingList;
  }
}

void
UnmarkSamplingList(struct W1Device * dev)
{
  while (dev) {
    dev->sampling = false;
    // Unmark all parent branches as needsWork too.
    struct Branch * pbr = dev->parentBranch;
    struct W1Device * coup;
    while (1) {
      pbr->needsWork = false;
      coup = BranchToCoupler(pbr);
      if (!coup)
        break;
      pbr = coup->parentBranch;
    }
    dev = dev->nextInSamplingList;
  }
}

// samplingQueue is a pointer to
// the first of an array of (maxLog2Seconds+1) Links.
void
MarkForSampling(uint32_t hbCount, Link * samplingQueue,
  struct W1Device * * samplingListHead,
  size_t devLinkOffset)
{
  DEBUG(doall) kprintf(KR_OSTREAM, "MarkForSampling hbCount=%#x\n", hbCount);
  *samplingListHead = NULL;
  unsigned int i;
  uint32_t mask;
  for (i = 0, mask = 0;
       i <= maxLog2Seconds;
       i++, samplingQueue++, mask = mask * 2 + 1) {
    if ((hbCount & mask) == 0) {
      // Mark all devices on this samplingQueue.
      Link * lk;
      struct W1Device * dev;
      for (lk = samplingQueue->next; lk != samplingQueue; lk = lk->next) {
        dev = (struct W1Device *) ((char *)lk - devLinkOffset);
        DEBUG(doall) kprintf(KR_OSTREAM, "MFS i=%d dev %#.16llx found %d\n",
                             i, dev->rom, dev->found);
        if (dev->found) {
          /* Link into the sampling list.
          The sampling list records the devices being sampled,
          so they can be queried later,
          because the set of devices on the samplingQueues may change. */
          dev->nextInSamplingList = *samplingListHead;
          *samplingListHead = dev;
          MarkDevForSampling(dev);
        }
      }
    }
  }
}

static struct Branch *
GetActiveBranch(struct W1Device * coup)
{
  switch (coup->u.coupler.activeBranch) {
  case branchMain:
    return &coup->u.coupler.mainBranch;

  case branchAux:
    return &coup->u.coupler.auxBranch;

  default:
    assert(false);
  case branchUnknown:
    return NULL;
  }
}

/* Report any errors, except those expected from a setPath command. */
static void
CheckSetPathErrors(int err)
{
  switch (err) {
  default:
    // Unexpected status
    kdprintf(KR_OSTREAM,
             "w1mult: unexpected status %d!\n", err);
  case capros_W1Bus_StatusCode_BusShorted:
  case capros_W1Bus_StatusCode_NoDevicePresent:
  case capros_W1Bus_StatusCode_BusError:
  case capros_W1Bus_StatusCode_OK:
  case -1:	// system restart
    break;
  }
}

// br must be active
void
EnsureBranchReset(struct Branch * br)
{
  if (resetBranch) {
    // Make sure THIS branch is reset.
    while (1) {
      if (br == resetBranch)
        return;		// we're good
      struct W1Device * dev = BranchToCoupler(br);
      if (!dev)		// reached the root, this branch isn't reset
        break;
      br = dev->parentBranch;
    }
  }
  // Just need a reset, since this branch is active.
  ProgramReset();
}

/* Ensure that just branch br is reset.
 * br must be active, that is, reached from the root via active branches. */
void
EnsureBranchSmartReset(struct Branch * br)
{
  if (resetBranch != br) {
    DEBUG(doall) kprintf(KR_OSTREAM, "EnsureBranchSmartReset br=%#x\n", br);
    ProgramReset();
    if (br != &root) {
      // Branch is active, so we can address it directly.
      struct W1Device * coup = BranchToCoupler(br);
      ProgramSmartOn(coup, br);
    }
  }
}

/* Switch child couplers if doing so helps and does no harm.
 * On entry, the branch is active.
 * Returns:
 *   -1 if W1Bus key is gone.
 *    0 if OK
 *   >0 if bus error */
int
ActivateNeededBranches(struct Branch * br)
{ 
  DEBUG(doall) kprintf(KR_OSTREAM, "ActivateNeededBranches br=%#x\n", br);
  struct W1Device * coup;
  int err;
  // For all child couplers:
  for (coup = br->childCouplers; coup; coup = coup->nextChild) {
    DEBUG(doall) kprintf(KR_OSTREAM, "ANB coup=%#.16llx main %c aux %c\n",
                         coup->rom,
                         coup->u.coupler.mainBranch.needsWork ? 'y' : 'n',
                         coup->u.coupler.auxBranch.needsWork ? 'y' : 'n');
    if (coup->u.coupler.mainBranch.needsWork) {
      if (! coup->u.coupler.auxBranch.needsWork
          && coup->u.coupler.activeBranch != branchMain) {
        // Switch to main branch.
        DEBUG(doall) kprintf(KR_OSTREAM,
          "ActivateNeededBranches switching to main of %#.16llx\n", coup->rom);
        EnsureBranchReset(coup->parentBranch);
        ProgramMatchROM(coup);
        WriteOneByte(0xa5);	// direct-on main
        wp(capros_W1Bus_stepCode_readBytes)
        wp(1)		// read confirmation byte
        AddPPItem(PPCouplerDirectOnMain, coup);
        err = RunProgram();
        if (err) {
          CheckSetPathErrors(err);
          return err;
        }
      }
    } else {
      if (coup->u.coupler.auxBranch.needsWork) {
        if (coup->u.coupler.activeBranch != branchAux) {
          // Switch to aux branch.
          DEBUG(doall) kprintf(KR_OSTREAM,
            "ActivateNeededBranches switching to aux of %#.16llx\n", coup->rom);
          EnsureBranchReset(coup->parentBranch);
          // There is no direct-on for the aux branch.
          ProgramSmartOn(coup, &coup->u.coupler.auxBranch);
          err = RunProgram();
          if (err)
            CheckSetPathErrors(err);
            return err;
        }
      } else {
        // Neither branch needs work.
        continue;
      }
    }
    // Recurse on the active branch:
    struct Branch * activeBranch = GetActiveBranch(coup);
    if (activeBranch)
      if (ActivateNeededBranches(activeBranch))
        break;		// no point in going on
  }
  return 0;
}

/* Work has been done on the active devices on this branch.
 * Do the work on the inactive devices.
 * Recursive procedure. */
static int
FinishWork(struct Branch * br)
{
  DEBUG(doall) kprintf(KR_OSTREAM, "FinishWork br=%#x\n", br);
  struct W1Device * coup;
  int err;

  assert(ProgramIsClear());

  // For all child couplers:
  for (coup = br->childCouplers; coup; coup = coup->nextChild) {
    struct Branch * activeBranch = GetActiveBranch(coup);
    if (! activeBranch) {
      // These might not be true, if a bus error caused
      //   the active branch to become unknown:
      // assert(! coup->u.coupler.mainBranch.needsWork);
      // assert(! coup->u.coupler.auxBranch.needsWork);
    } else {
      // Completely finish with the active branch before switching
      // away from it:
      if (activeBranch->needsWork) {
        err = FinishWork(activeBranch);
        if (err) goto exit;
      }
      // Now deal with the other branch:
      struct Branch * otherBranch;
      uint32_t otherBranchCommand;
      if (coup->u.coupler.activeBranch == capros_W1Bus_stepCode_setPathMain) {
        otherBranch = &coup->u.coupler.auxBranch;
        otherBranchCommand = capros_W1Bus_stepCode_setPathAux;
      } else {
        otherBranch = &coup->u.coupler.mainBranch;
        otherBranchCommand = capros_W1Bus_stepCode_setPathMain;
      }
      if (otherBranch->needsWork) {
        DEBUG(doall)
          kprintf(KR_OSTREAM, "FinishWork coup=%#.16llx switching to %#x\n",
                             coup->rom, otherBranch->whichBranch);
        // Switch to the other branch with a smart-on:
        EnsureBranchReset(coup->parentBranch);
        ProgramSmartOn(coup, otherBranch);
        err = RunProgram();
        if (err) {
          CheckSetPathErrors(err);
          goto exit;
        }
        err = DoAll(otherBranch);
        if (err) goto exit;
      }
    }
  }
  err = 0;
exit:
  br->needsWork = false;	// we finished the work on this branch
  return err;
}

void (*DoEachWorkFunction)(struct W1Device * dev);

/* Use this as the DoAllWorkFunction when each device needs to be
 * addressed individually. */
void
DoEach(struct Branch * br)
{
  DEBUG(doall) kprintf(KR_OSTREAM, "DoEach br=%#x\n", br);
  struct W1Device * dev;
  // For all child devices:
  for (dev = br->childDevices; dev; dev = dev->nextChild) {
    if (dev->sampling)
      DoEachWorkFunction(dev);
  }
  struct W1Device * coup;
  // For all child couplers:
  for (coup = br->childCouplers; coup; coup = coup->nextChild) {
    struct Branch * activeBranch = GetActiveBranch(coup);
    if (activeBranch && activeBranch->needsWork)
      DoEach(activeBranch);
  }
}

void (*DoAllWorkFunction)(struct Branch * br);

/* Perform an action on all devices marked as sampling.
 *
 * This procedure organizes the work on the marked devices
 * so as to minimize the amount of switching of couplers.
 *
 * On entry, the branch is active.
 *
 * On exit, the action has been performed on all the marked devices. */
int
DoAll(struct Branch * br)
{
  int err;
  DEBUG(doall) kprintf(KR_OSTREAM, "DoAll br=%#x\n", br);
  // First, see if any couplers serve just one branch and need to be switched.
  err = ActivateNeededBranches(br);
  if (err) return err;
  // Do the real work on the active tree:
  (*DoAllWorkFunction)(br);
  // Now work on the inactive branches:
  return FinishWork(br);
}

/**********************************************************************/

/* Returns one of:
 *   -1 if W1Bus cap is gone
 *   0
 *   capros_W1Bus_StatusCode_BusError
 *   capros_W1Bus_StatusCode_CRCError
 */
int
SearchPath(struct Branch * br)
{
  int statusCode;
  int i;
  uint64_t rom;
  uint64_t discrep;

  assert(ProgramIsClear());

  AddressPath(br, true);

  // Deactivate the branch lines of any couplers at this level,
  // so we will search just at this level.
  wp(capros_W1Bus_stepCode_skipROM)
  NotReset();
  WriteOneByte(0x66);	// all lines off
  wp(capros_W1Bus_stepCode_readBytes)
  wp(1)			// read one byte
  // If there are any couplers, they will transmit a confirmation byte.
  // If there are none, this will read ones.
  statusCode = RunProgram();
  if (statusCode) {
    CheckSetPathErrors(statusCode);
    switch (statusCode) {
    case capros_W1Bus_StatusCode_BusError:
    case -1:			// key went away due to reboot
      return statusCode;

    case capros_W1Bus_StatusCode_BusShorted:
    {
      struct W1Device * dev = BranchToCoupler(br);
      if (dev) {
        kprintf(KR_OSTREAM, "w1mult: Branch %x of coupler %#.16llx shorted!\n",
                br->whichBranch, dev->rom);
      } else {
        kprintf(KR_OSTREAM, "w1mult: Root branch shorted!\n");
      }
      return 0;		// nothing more we can do here
    }

    case capros_W1Bus_StatusCode_NoDevicePresent:
      // No devices on this branch. Nothing to do.
      return 0;
    }
  }

  if (RunPgmMsg.rcv_sent != 1
      || (inBuf[0] != 0x66 && inBuf[0] != 0xff) ) {
    kprintf(KR_OSTREAM, "All lines off got status %d %d %d bytes=%d",
        statusCode, RunPgmMsg.rcv_w2, RunPgmMsg.rcv_w3,
        RunPgmMsg.rcv_sent);
    kprintf(KR_OSTREAM, "All lines off got byte %#x", inBuf[0]);
    return capros_W1Bus_StatusCode_BusError;
  }
  else	// there are devices, and all lines off did not fail
  {
    // Find ROMs on this branch.
    rom = 0;		// next ROM to find
    unsigned int tries = 0;
    while (1) {
      AddressPath(br, true);
      wp(capros_W1Bus_stepCode_searchROM)
      CopyToProgram(&rom, 8);
      NotReset();
      statusCode = RunProgram();
      switch (statusCode) {
      default:
        kdprintf(KR_OSTREAM, "SearchROM got status $d!\n", statusCode);
      case capros_W1Bus_StatusCode_BusError:
      case -1:
        return statusCode;

      case capros_W1Bus_StatusCode_NoDevicePresent:
        goto endLoop;

      case capros_W1Bus_StatusCode_OK:
        break;
      }

      if (RunPgmMsg.rcv_sent != 16)
        kdprintf(KR_OSTREAM, "SearchROM got %d bytes", RunPgmMsg.rcv_sent);

      unsigned int d = CalcCRC8(inBuf, 8);
      if (d) {		// invalid CRC
        DEBUG(errors) kprintf(KR_OSTREAM, "SearchROM got CRC %#.2x!\n", d);
        if (++tries >= 2)
          return capros_W1Bus_StatusCode_CRCError;
        continue;
      }

      memcpy(&rom, inBuf, 8);	// little-endian
      memcpy(&discrep, inBuf+8, 8);
      DEBUG(search) kprintf(KR_OSTREAM, "ROM %#.16llx\n", rom);

      // Is this device in our configuration?
      /* This search could be more efficient, but I don't expect
      large numbers of devices. */
      for (i = 0; i < numDevices; i++) {
        struct W1Device * dev = &devices[i];
        if (dev->rom == rom
            && dev->parentBranch == br) {
          // Do device-independent initialization:
          dev->found = true;
          dev->sampling = false;
          // Add to its branch:
          if (w1dev_IsCoupler(dev)) {
            dev->nextChild = br->childCouplers;
            br->childCouplers = dev;
          } else {
            dev->nextChild = br->childDevices;
            br->childDevices = dev;
          }
          // Do device-specific initialization:
          switch (w1dev_getFamilyCode(dev)) {
          case famCode_DS2409:	// coupler
            Branch_Init(&dev->u.coupler.mainBranch);
            Branch_Init(&dev->u.coupler.auxBranch);
            break;
          case famCode_DS18B20:	// thermometer
            DS18B20_InitDev(dev);
            break;
          case famCode_DS2438:	// battery monitor
            DS2438_InitDev(dev);
            break;
          case famCode_DS2450:	// A/D
            DS2450_InitDev(dev);
            break;
          default: break;
          }
          break;
        }
      }
      if (i == numDevices) {	// did not find it
        struct W1Device * parentCoup = BranchToCoupler(br);
        if (parentCoup) 
          kprintf(KR_OSTREAM, "From parent %#.16llx %#x ",
                  parentCoup->rom, br->whichBranch);
        kprintf(KR_OSTREAM, "ROM %#.16llx found but not configured.\n", rom);
      }
      else {
        struct W1Device * dev = &devices[i];

        DEBUG(search) kprintf(KR_OSTREAM, "ROM %#.16llx found.\n", rom);

        if (w1dev_IsCoupler(dev)) {	// recurse
          DEBUG(search) kprintf(KR_OSTREAM, "Searching coupler %#.16llx\n",
                                dev->rom);
          statusCode = SearchPath(&dev->u.coupler.mainBranch);
          if (statusCode != 0)
            return statusCode;
          statusCode = SearchPath(&dev->u.coupler.auxBranch);
          if (statusCode != 0)
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
  endLoop: ;
  }
  return 0;
}

/* We need to reinitialize the bus.
 */
static void
ScanBus(void)
{
  result_t result;
  int statusCode;
  int i;

rescan:
  result = capros_W1Bus_resetDevice(KR_W1BUS);
  if (CheckModeResult(result)) return;

  // Set bus parameters:
  result = capros_W1Bus_setSpeed(KR_W1BUS, capros_W1Bus_W1Speed_flexible);
  if (CheckRestart(result)) return;

  result = capros_W1Bus_setPDSR(KR_W1BUS, capros_W1Bus_PDSR_PDSR137);
        // 1.37 V/us
  if (CheckModeResult(result)) return;

  result = capros_W1Bus_setW1LT(KR_W1BUS, capros_W1Bus_W1LT_W1LT11);    // 11 us
  if (CheckModeResult(result)) return;

  result = capros_W1Bus_setDSO(KR_W1BUS, capros_W1Bus_DSO_DSO10);  // 10 us
  if (CheckModeResult(result)) return;

  Branch_Init(&root);

  // Check out the main bus:
  ClearProgram();
  wp(capros_W1Bus_stepCode_resetNormal)
  int status = RunProgram();
  switch (status) {
  case capros_W1Bus_StatusCode_BusShorted:
    root.shorted = true;
    kdprintf(KR_OSTREAM, "Main bus is shorted!!\n");
  case capros_W1Bus_StatusCode_NoDevicePresent:
  case -1:	// W1Bus key went away already, probably due to restart
    return;

  case capros_W1Bus_StatusCode_BusError:
    // Should there be a limit on this loop? Or a delay?
    goto rescan;
  
  default:
    kdprintf(KR_OSTREAM, "Main bus reset got status %d!!\n", status);
    return;

  case capros_W1Bus_StatusCode_AlarmingPresencePulse:
  case capros_W1Bus_StatusCode_OK:

    // Search for all ROMs.
    statusCode = SearchPath(&root);
    if (statusCode < 0)
      return;
    if (statusCode > 0)
      goto rescan;

    for (i = 0; i < numDevices; i++) {
      if (! devices[i].found) {
        kprintf(KR_OSTREAM, "ROM %#.16llx configured but not found.\n",
                devices[i].rom);
      }
    }

    heartbeatCount = 0;
    RecordCurrentTime();
  }
  busNeedsReinit = false;
}

#define TimerKeyInfo 0xfffe
// The timer thread executes this procedure.
void
TimerProcedure(void)
{
  result_t result;
  
  Message Msg = {
    .snd_invKey = 31,	// a start key to the main process, keyInfo=TimerKeyInfo
    .snd_key0 = KR_VOID,
    .snd_key1 = KR_VOID,
    .snd_key2 = KR_VOID,
    .snd_rsmkey = KR_VOID,
    .snd_len = 0,
    .snd_code = 0,
    .snd_w1 = 0,
    .snd_w2 = 0,
    .snd_w3 = 0,

    .rcv_key0 = KR_VOID,
    .rcv_key1 = KR_VOID,
    .rcv_key2 = KR_VOID,
    .rcv_rsmkey = KR_VOID,
    .rcv_limit = 0
  };

  for (;;) {
    DEBUG(timer) kprintf(KR_OSTREAM, "Timer waiting till %lld",
                         timeToWake/1000000);
    result = capros_Sleep_sleepTillPersistent(KR_SLEEP, timeToWake);
    assert(result == RC_OK || result == RC_capros_key_Restart);
    CALL(&Msg);		// call the main process
  }
}

/* If timerRunning == false, we have a resume key to the timer
in KR_TIMRESUME. */
bool timerRunning;

static void
InitTimerProcess(void)
{
  result_t result;
  result = capros_Process_makeResumeKey(KR_TIMPROC, KR_TIMRESUME);
  assert(result == RC_OK);
  capros_Process_CommonRegisters32 regs = {
    .procFlags = 0,
    .faultCode = 0,
    .pc = (uint32_t)&TimerProcedure,
    .sp = timer_stack_pointer
  };
  result = capros_Process_setRegisters32(KR_TIMPROC, regs);
  assert(result == RC_OK);
  timerRunning = false;
}

static void
StopTimerProcess(void)
{
  InitTimerProcess();
}

capros_Sleep_nanoseconds_t
GetDesiredExpiration(void)
{
  return link_isSingleton(&timerHead) ? infiniteTime
           : container_of(timerHead.next, struct w1Timer, link)->expiration;
}

static void
DeliverAnyMessage(Message * msg)
{
  if (msg->snd_invKey != KR_VOID) {
    PSEND(msg);
    msg->snd_invKey = KR_VOID;	// don't send twice
  }
}

// capros_DS2450_portsConfiguration is the largest string we receive.
uint32_t MsgBuf[(sizeof(capros_DS2450_portsConfiguration)+3)/4];
int
main(void)
{
  result_t result;
  
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

  link_Init(&timerHead);
  InitTimerProcess();

  // Create supernode.
  result = capros_Node_getSlotExtended(KR_CONSTIT, KC_SNODEC, KR_TEMP0);
  assert(result == RC_OK);
  result = capros_Constructor_request(KR_TEMP0,
             KR_BANK, KR_SCHED, KR_VOID, KR_KEYSTORE);
  assert(result == RC_OK);	// FIXME

  // Let each device type initialize:
  DS18B20_Init();
  DS2438_Init();
  DS2450_Init();

  DEBUG(server) kprintf(KR_OSTREAM, "w1mult started\n");

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
      dev->parentBranch = &root;
    } else {
      assert(cfg->parentIndex < numDevices);
      struct W1Device * parentCoup = &devices[cfg->parentIndex];
      switch (cfg->mainOrAux) {
      case branch_main:
        dev->parentBranch = &parentCoup->u.coupler.mainBranch;
        break;
      case branch_aux:
        dev->parentBranch = &parentCoup->u.coupler.auxBranch;
        break;
      default:
        kdprintf(KR_OSTREAM, "Configuration error %d\n", numDevices);
      }
    }
    dev->rom = cfg->rom;
    // dev->found = false; // part of AllDevsNotFound
    dev->callerWaiting = false;
    dev->onWorkList = false;
    // Do device-specific initialization:
    switch (w1dev_getFamilyCode(dev)) {
    case famCode_DS2409:
      dev->u.coupler.mainBranch.whichBranch = capros_W1Bus_stepCode_setPathMain;
      dev->u.coupler.auxBranch.whichBranch = capros_W1Bus_stepCode_setPathAux;
      // dev->u.coupler.activeBranch = branchUnknown; // part of AllDevsNotFound
      break;
    case famCode_DS18B20:
      DS18B20_InitStruct(dev);
      break;
    case famCode_DS2438:
      DS2438_InitStruct(dev);
      break;
    case famCode_DS2450:
      DS2450_InitStruct(dev);
      break;
    default: break;
    }

    numDevices++;
  }
  AllDevsNotFound();
  
  // kdprintf(KR_OSTREAM, "w1mult: accepting requests\n");

  for(;;) {
    /* Perform any work whose time has come. */
    RecordCurrentTime();

    if (GetDesiredExpiration() <= currentTime) {
      /* There is work to do.
      But first, return to the caller if any. */
      DeliverAnyMessage(&Msg);

      // Handle expired timer tasks.
      do {
        Link * lnk = timerHead.next;
        link_Unlink(lnk);	// remove from list
        struct w1Timer * tim = container_of(lnk, struct w1Timer, link);
        // Call the function.
        (*tim->function)(tim->arg);
      } while (GetDesiredExpiration() <= currentTime);
    }

    /* Before becoming Available, make sure we will be awoken
    when we need to be. */
    capros_Sleep_nanoseconds_t desiredWakeupTime = GetDesiredExpiration();
    DEBUG(timer) kprintf(KR_OSTREAM, "About to wait, thead %#x dwut=%lld tr=%d ttw=%lld\n",
                   timerHead.next,
                   desiredWakeupTime/1000000,
                   timerRunning,
                   timeToWake/1000000);
    if (timerRunning
        && timeToWake > desiredWakeupTime) {
      /* The timer is waiting too long. Stop and restart him. */
      StopTimerProcess();
    }
    if (!timerRunning) {
      DeliverAnyMessage(&Msg);
      timeToWake = desiredWakeupTime;
      timerRunning = true;
      Msg.snd_invKey = KR_TIMRESUME;
      Msg.snd_len = 0;
      // Timer isn't particular about anything else.
    }

    RETURN(&Msg);
    DEBUG(server) kprintf(KR_OSTREAM, "w1mult was called, keyInfo=%#x\n",
                          Msg.rcv_keyInfo);

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

      case OC_capros_W1Mult_registerBus:
        // Return to the caller before invoking the W1Bus cap,
        // to prevent deadlock.
        SEND(&Msg);

        // We don't use the subtype in Msg.rcv_w1.
        COPY_KEYREG(KR_ARG(0), KR_NEXTW1BUS);

        if (haveNextBusKey) {
          DEBUG(errors)
            kprintf(KR_OSTREAM, "W1mult: new bus cap replacing next.\n");
        } else {
          haveNextBusKey = true;
          if (! haveBusKey)
            EnableHeartbeat(hbBit_bus);
        }
        break;
      }
      break;

    case TimerKeyInfo:	// timer thread has this key
      DEBUG(timer) kprintf(KR_OSTREAM, "Timer called");
      timerRunning = false;
      // We return to the timer only when done with timer work.
      COPY_KEYREG(KR_RETURN, KR_TIMRESUME);
      Msg.snd_invKey = KR_VOID;
      break;

    default:		// a device key, keyInfo == device number
    {
      assert(Msg.rcv_keyInfo < numDevices);
      struct W1Device * dev = &devices[Msg.rcv_keyInfo];

      switch (w1dev_getFamilyCode(dev)) {
      default:
        assert(false);

      case famCode_DS18B20:
        DS18B20_ProcessRequest(dev, &Msg);
        break;

      case famCode_DS2438:
        DS2438_ProcessRequest(dev, &Msg);
        break;

      case famCode_DS2450:
        DS2450_ProcessRequest(dev, &Msg);
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
