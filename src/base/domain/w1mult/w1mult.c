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
#include <idl/capros/Process.h>
#include <idl/capros/Sleep.h>
#define infiniteTime 18446744073709000000ULL
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
  0x1d000: main stack
  0x1e000: timer stack
  0x1f000: config file */
#define configFileAddr 0x1f000

uint32_t __rt_unkept = 1;
unsigned long __rt_stack_pointer = 0x1e000;
#define timer_stack_pointer 0x1f000

bool haveBusKey = false;	// no key in KR_W1BUS yet

/*************************** Branch stuff ***************************/

struct Branch root = {
  .whichBranch = 0
};

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
  case capros_W1Bus_stepCode_setPathMain:
    return container_of(br, struct W1Device, u.coupler.mainBranch);
  case capros_W1Bus_stepCode_setPathAux:
    return container_of(br, struct W1Device, u.coupler.auxBranch);
  default:	// root branch
    return 0;
  }
}

/************************** time stuff  ****************************/

capros_Sleep_nanoseconds_t currentTime;
capros_Sleep_nanoseconds_t timeToWake = infiniteTime;

// An ordered list of w1Timer's:
Link timerHead;

void
InsertTimer(struct w1Timer * timer)
{
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
  result_t result = capros_Sleep_getTimeMonotonic(KR_SLEEP, &currentTime);
  assert(result == RC_OK);
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

/************** Stuff for programming the 1-Wire bus *******************/

unsigned char outBuf[capros_W1Bus_maxProgramSize + 1];
unsigned char * const outBeg = &outBuf[0];
unsigned char * outCursor;
unsigned char inBuf[capros_W1Bus_maxResultsSize];

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
ClearProgram(void)
{
  outCursor = outBeg;
  nextPPItem = &PPItems[0];
}

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
WriteOneByte(uint8_t b)
{
  wp(capros_W1Bus_stepCode_writeBytes)
  wp(1)
  wp(b)
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

  uint32_t status = RunPgmMsg.rcv_w1;

  switch (status) {
  case capros_W1Bus_StatusCode_ProgramError:
  case capros_W1Bus_StatusCode_SequenceError:
    // These errors indicate a software bug.
    kdprintf(KR_OSTREAM, "w1mult RunProgram got status %d!!", status);
    break;	// this is unrecoverable

  unsigned int errorLoc;

  case capros_W1Bus_StatusCode_OK:
    errorLoc = outCursor - outBeg;
    goto doPP;

  default:
    DEBUG(errors)
      kprintf(KR_OSTREAM, "w1mult RunProgram got status %d", status);

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
  return RunPgmMsg.rcv_w1;	// needed?
}

/************************* Heartbeat stuff ************************/

uint32_t heartbeatCount;
capros_Sleep_nanoseconds_t lastHeartbeatTime;

void
HeartbeatAction(void * arg)
{
  RecordCurrentTime();
  lastHeartbeatTime = currentTime;

  // Let each type of device do its thing:
  DS18B20_HeartbeatAction(heartbeatCount);
  heartbeatCount++;

  EnableHeartbeat(0);	// Schedule next heartbeat if we can
}

struct w1Timer heartbeatTimer = {
  .function = &HeartbeatAction
};

uint32_t heartbeatDisable = 0;

void
DisableHeartbeat(uint32_t bit)
{
  heartbeatDisable |= bit;
}

#define heartbeatInterval 1000000000ULL	// 1 second in nanoseconds
void
EnableHeartbeat(uint32_t bit)
{
  if ((heartbeatDisable &= ~bit) == 0) {
    heartbeatTimer.expiration = lastHeartbeatTime + heartbeatInterval;
    InsertTimer(&heartbeatTimer);
  }
}

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

void
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
    kdprintf(KR_OSTREAM, "Coup %#llx br %#x is shorted!!\n",
             coup->rom, br->whichBranch);
    break;

  default:
    kdprintf(KR_OSTREAM, "Smart-on coup %#llx br %#x got status %d\n",
             coup->rom, br->whichBranch, status);
  }
  if (status == capros_W1Bus_StatusCode_OK) {
  } else {
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
    kdprintf(KR_OSTREAM, "Direct on main coup %#llx got status %d\n",
             coup->rom, status);
  }
}

void
ProgramSmartOn(struct W1Device * coup, struct Branch * br)
{
  assert(coup == BranchToCoupler(br));
  
  wp(br->whichBranch)	// capros_W1Bus_stepCode_setPath*
  memcpy(outCursor, &coup->rom, 8);
  outCursor += 8;
  AddPPItem(PPSmartOn, br);
}

/* Address a path through couplers.

If mustSmartOn is true, the last coupler will always be given
a Smart-On command (useful for limiting a search to that branch).
*/

void
AddressPath(struct Branch * branch, bool mustSmartOn)
{
  wp(capros_W1Bus_stepCode_resetSimple)
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
  memcpy(outCursor, &dev->rom, 8);	// little endian
  outCursor += 8;
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

// workQueue is a pointer to the first of an array of (maxLog2Seconds+1) Links.
void
MarkForSampling(uint32_t hbCount, Link * workQueue,
  struct W1Device * * samplingListHead)
{
  DEBUG(doall) kprintf(KR_OSTREAM, "MarkForSampling hbCount=%#x\n", hbCount);
  *samplingListHead = NULL;
  unsigned int i;
  for (i = 0, hbCount = (hbCount << 1) + 1;
       i <= maxLog2Seconds;
       i++, workQueue++, hbCount >>= 1) {
    if (hbCount & 1) {
      // Mark all devices on this workQueue.
      Link * lk;
      struct W1Device * dev;
      for (lk = workQueue->next; lk != workQueue; lk = lk->next) {
        dev = container_of(lk, struct W1Device, workQueueLink);
        if (dev->found) {
          // Link into sampling list:
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
  if (coup->u.coupler.activeBranch == branchMain)
    return &coup->u.coupler.mainBranch;
  if (coup->u.coupler.activeBranch == branchAux)
    return &coup->u.coupler.auxBranch;
  return NULL;
}

bool DoAllBranchHasReset;

void
EnsureBranchReset(void)
{
  if (! DoAllBranchHasReset) {
    ClearProgram();
    // Just need a reset, since this branch is active.
    wp(capros_W1Bus_stepCode_resetSimple)
  }
}

void
EnsureDoAllBranchSmartReset(struct Branch * br)
{
  if (! DoAllBranchHasReset) {
    DEBUG(doall) kprintf(KR_OSTREAM, "EnsureDoAllBranchSmartReset br=%#x\n",
                         br);
    ClearProgram();
    wp(capros_W1Bus_stepCode_resetSimple)
    if (br != &root) {
      struct W1Device * coup = BranchToCoupler(br);
      ProgramSmartOn(coup, br);
    }
  }
}

/* Switch child couplers if doing so helps and does no harm.
 * On entry, the branch is active. */
void
ActivateNeededBranches(struct Branch * br)
{ 
  DEBUG(doall) kprintf(KR_OSTREAM, "ActivateNeededBranches br=%#x\n", br);
  struct W1Device * coup;
  // For all child couplers:
  for (coup = br->childCouplers; coup; coup = coup->nextChild) {
    DEBUG(doall) kprintf(KR_OSTREAM, "ANB coup=%#llx main %c aux %c\n",
                         coup->rom,
                         coup->u.coupler.mainBranch.needsWork ? 'y' : 'n',
                         coup->u.coupler.auxBranch.needsWork ? 'y' : 'n');
    if (coup->u.coupler.mainBranch.needsWork) {
      if (! coup->u.coupler.auxBranch.needsWork
          && coup->u.coupler.activeBranch != branchMain) {
        // Switch to main branch.
        DEBUG(doall) kprintf(KR_OSTREAM,
          "ActivateNeededBranches switching to main of %#llx\n", coup->rom);
        EnsureBranchReset();
        ProgramMatchROM(coup);
        WriteOneByte(0xa5);	// direct-on main
        wp(capros_W1Bus_stepCode_readBytes)
        wp(1)		// read confirmation byte
        AddPPItem(PPCouplerDirectOnMain, coup);
        RunProgram();
        ClearProgram();
        DoAllBranchHasReset = false;
      }
    } else {
      if (coup->u.coupler.auxBranch.needsWork) {
        if (coup->u.coupler.activeBranch != branchAux) {
        // Switch to aux branch.
        DEBUG(doall) kprintf(KR_OSTREAM,
          "ActivateNeededBranches switching to aux of %#llx\n", coup->rom);
        EnsureBranchReset();
        // There is no direct-on for the aux branch.
        ProgramSmartOn(coup, &coup->u.coupler.auxBranch);
        RunProgram();//// check return
        ClearProgram();
        DoAllBranchHasReset = false;
      }
      } else {
        // Neither branch needs work.
        continue;
      }
    }
    // Recurse on the active branch:
    struct Branch * activeBranch = GetActiveBranch(coup);
    if (activeBranch)
      ActivateNeededBranches(activeBranch);
  }
}

/* Work has been done on the active devices on this branch.
 * Do the work on the inactive devices. */
void
FinishWork(struct Branch * br)
{
  DEBUG(doall) kprintf(KR_OSTREAM, "FinishWork br=%#x\n", br);
  struct W1Device * coup;
  // For all child couplers:
  for (coup = br->childCouplers; coup; coup = coup->nextChild) {
    struct Branch * activeBranch = GetActiveBranch(coup);
    if (! activeBranch) {
      assert(! coup->u.coupler.mainBranch.needsWork);
      assert(! coup->u.coupler.auxBranch.needsWork);
    } else {
      // Completely finish with the active branch before switching
      // away from it:
      if (activeBranch->needsWork)
        FinishWork(activeBranch);
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
        EnsureBranchReset();
        ProgramSmartOn(coup, otherBranch);
        int status = RunProgram();
        if (status) {
          kdprintf(KR_OSTREAM, "FinishWork smart-on got status %d\n", status);
          // FIXME handle this
        }
        ClearProgram();
        DoAllBranchHasReset = true;	// smart-on includes a reset
        DoAll(otherBranch);
      }
    }
  }
  br->needsWork = false;	// we finished the work on this branch
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
 * On entry, the branch is active, and a reset has been programmed
 * for just this branch (a smart-on if this is a branch of a coupler).
 *
 * On exit, the action has been performed on all the marked devices. */
void
DoAll(struct Branch * br)
{
  DEBUG(doall) kprintf(KR_OSTREAM, "DoAll br=%#x\n", br);
  DoAllBranchHasReset = true;
  // First, see if any couplers serve just one branch and need to be switched.
  ActivateNeededBranches(br);
  // Do the real work on the active tree:
  DoAllWorkFunction(br);
  // Now work on the inactive branches:
  FinishWork(br);
}

/**********************************************************************/

/* Returns -1 if W1Bus cap is gone, else 0. */
int
SearchPath(struct Branch * br)
{
  int statusCode;
  int i;
  uint64_t rom;
  uint64_t discrep;

  ClearProgram();
  AddressPath(br, true);
  unsigned char * initCursor = outCursor;	// remember this point

  // Deactivate the branch lines of any couplers at this level,
  // so we will search just at this level.
  *outCursor++ = capros_W1Bus_stepCode_skipROM;
  WriteOneByte(0x66);	// all lines off
  *outCursor++ = capros_W1Bus_stepCode_readBytes;
  *outCursor++ = 1;		// read one byte
  // If there are any couplers, they will transmit a confirmation byte.
  // If there are none, this will read ones.
  statusCode = RunProgram();
  if (statusCode < 0)
    return statusCode;	// key went away due to reboot

  if (statusCode != capros_W1Bus_StatusCode_OK
      || RunPgmMsg.rcv_sent != 1
      || (inBuf[0] != 0x66 && inBuf[0] != 0xff) ) {
    if (statusCode == capros_W1Bus_StatusCode_NoDevicePresent
        && RunPgmMsg.rcv_w2 < (initCursor - outBeg) ) {
      // No devices on this branch. Nothing to do.
    } else {
      kprintf(KR_OSTREAM, "All lines off got status %d %d %d bytes=%d",
        statusCode, RunPgmMsg.rcv_w2, RunPgmMsg.rcv_w3,
        RunPgmMsg.rcv_sent);
      kprintf(KR_OSTREAM, "All lines off got byte %#x", inBuf[0]);
      // FIXME recover; repeat?
    }
    ClearProgram();
  }
  else	// there are devices, and all lines off did not fail
  {
    ClearProgram();
    // Find ROMs on this branch.
    rom = 0;		// next ROM to find
    while (1) {
      AddressPath(br, true);
      wp(capros_W1Bus_stepCode_searchROM)
      memcpy(outCursor, &rom, 8);
      outCursor += 8;
      statusCode = RunProgram();
      ClearProgram();
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
          DEBUG(search) kprintf(KR_OSTREAM, "Searching coupler %#llx\n",
                                dev->rom);
          statusCode = SearchPath(&dev->u.coupler.mainBranch);
          if (statusCode < 0)
            return statusCode;
          statusCode = SearchPath(&dev->u.coupler.auxBranch);
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

  Branch_Init(&root);

  // Check out the main bus:
  ClearProgram();
  wp(capros_W1Bus_stepCode_resetNormal)
  int status = RunProgram();
  ClearProgram();
  switch (status) {
  case capros_W1Bus_StatusCode_BusShorted:
    root.shorted = true;
    kdprintf(KR_OSTREAM, "Main bus is shorted!!\n");
  case capros_W1Bus_StatusCode_NoDevicePresent:
    break;
  
  default:
    kdprintf(KR_OSTREAM, "Main bus reset got status %d!!\n", status);
    break;

  case capros_W1Bus_StatusCode_AlarmingPresencePulse:
  case capros_W1Bus_StatusCode_OK:

    // Search for all ROMs.
    for (i = 0; i < numDevices; i++) {
      struct W1Device * dev = &devices[i];
      dev->found = false;
      // We may have lost the state of the devices:
      if (w1dev_IsCoupler(dev))
          dev->u.coupler.activeBranch = branchUnknown;
    }

    statusCode = SearchPath(&root);
    if (statusCode < 0)
      return statusCode;

    for (i = 0; i < numDevices; i++) {
      if (! devices[i].found) {
        kprintf(KR_OSTREAM, "ROM %#.16llx configured but not found.\n",
                devices[i].rom);
      }
    }

    haveBusKey = true;
    heartbeatCount = 0;
    RecordCurrentTime();
    HeartbeatAction(NULL);	// First heartbeat
  }
  return 0;
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
    result = capros_Sleep_sleepTill(KR_SLEEP, timeToWake);
    assert(result == RC_OK);
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
    .rcv_limit = 0
  };

  link_Init(&timerHead);
  InitTimerProcess();

  // Let each device type initialize:
  DS18B20_Init();

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
    link_Init(&dev->workQueueLink);
    dev->callerWaiting = false;
    // Do device-specific initialization:
    switch (w1dev_getFamilyCode(dev)) {
    case famCode_DS2409:
      dev->u.coupler.mainBranch.whichBranch = capros_W1Bus_stepCode_setPathMain;
      dev->u.coupler.auxBranch.whichBranch = capros_W1Bus_stepCode_setPathAux;
      dev->u.coupler.activeBranch = branchUnknown;
      break;
    case famCode_DS18B20:
      DS18B20_InitStruct(dev);
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
    if (timerRunning
        && timeToWake > desiredWakeupTime) {
      /* The timer is waiting too long. Stop and restart him. */
      StopTimerProcess();
    }
    if (!timerRunning) {
      DeliverAnyMessage(&Msg);
      timeToWake = desiredWakeupTime;
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

      case OC_capros_W1Mult_RegisterBus:
        // We don't use the subtype in Msg.rcv_w1.
        COPY_KEYREG(KR_ARG(0), KR_W1BUS);
        ScanBus();
        break;
      }
      break;

    case TimerKeyInfo:	// timer thread has this key
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
