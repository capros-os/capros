/*
 * Copyright (C) 2007, 2008, Strawberry Development Group.
 *
 * This file is part of the CapROS Operating System runtime library.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, 59 Temple Place - Suite 330 Boston, MA 02111-1307, USA.
 */
/* This material is based upon work supported by the US Defense Advanced
Research Projects Agency under Contract No. W31P4Q-07-C-0070.
Approved for public release, distribution unlimited. */

#include <setjmp.h>
#include <eros/container_of.h>
#include <domain/cmtesync.h>
#include <eros/Invoke.h>	// get RC_OK
#include <idl/capros/Sleep.h>
#define capros_Sleep_infiniteTime UINT64_MAX
#include <idl/capros/Node.h>
#include <idl/capros/Process.h>
#include <domain/assert.h>
#include <domain/CMTESemaphore.h>
#include <domain/CMTEThread.h>
#include <domain/CMTETimer.h>

// #define TIMERDEBUG

CMTEMutex_DECLARE_Unlocked(timerLock);

// The following are protected by timerLock:
unsigned int timerThreadNum = noThread;
CMTETime_t timeWaitingFor = capros_Sleep_infiniteTime;
Link timerHead = link_Initializer(timerHead);
/* There is a separate timer implementation for each address space, so we don't 
expect a long list of timers. So we just keep an ordered list. */

INLINE CMTETimer *
LinkToTimer(Link * lk)
{
  return container_of(lk, CMTETimer, link);
}

static CMTETime_t
GetCurrentTime(void)
{
  CMTETime_t t;
  capros_Sleep_getPersistentMonotonicTime(KR_SLEEP, &t);
  return t;
}

// Must be called under timerLock.
static inline CMTETime_t
GetSoonestExpiration(void)
{
  if (link_isSingleton(&timerHead))
    return capros_Sleep_infiniteTime;
  else
    return LinkToTimer(timerHead.next)->expiration;
}

// Must be called under timerLock.
static inline void
remove_timer(CMTETimer * timer)
{
  Link * lk = &timer->link;
  link_UnlinkUnsafe(lk);
  lk->next = NULL;	// mark no longer pending
  lk->prev = NULL;	// poison value to catch errors
}

static const unsigned int timerThreadStackSize = 2048;

/*
The timerThread loops, waiting for the soonest expiration and waking up
any expired timers. 

The difficulty is that a timer with a sooner expiration may be added,
while the timerThread is stuck waiting for longer than that. 
When this happens, we must unstick the timerThread.

timerThreadState handles the synchronization for that. 
timerThreadState must only be referenced under the timerLock.

ttsWorking: timerThread is not sleeping, or is being teleported out of sleep,
  and will check the expiration time before sleeping again.

ttsSleeping: timerThread may be sleeping, and can be teleported.

ttsTeleporting: timerThread is being teleported out of sleep.
*/

enum {
  ttsWorking,
  ttsSleeping,
  ttsTeleporting
};
uint32_t timerThreadState = ttsWorking;

jmp_buf jb;
static void
DoJump(void)
{
  longjmp(jb, 1);
}

// Thread begins in WorkRegion.
void *
timerThreadProc(void * arg)
{
  result_t result;

  switch (setjmp(jb)) {
  case 0:
    CMTEMutex_lock(&timerLock);
    break;

  default:	// longjmp comes here
    CMTEMutex_lock(&timerLock);

    assert(timerThreadState == ttsTeleporting);
    timerThreadState = ttsWorking;

#ifdef TIMERDEBUG
    kprintf(KR_OSTREAM, "timer teleported\n");
#endif
  }
  while (1) {
    // We are holding timerLock.

    timeWaitingFor = GetSoonestExpiration();

    assert(timerThreadState == ttsWorking);
    timerThreadState = ttsSleeping;

#ifdef TIMERDEBUG
    kprintf(KR_OSTREAM, "timer to sleep, state %d\n", timerThreadState);
#endif

    CMTEMutex_unlock(&timerLock);

    // Sleep.
    result = capros_Sleep_sleepTillPersistent(KR_SLEEP, timeWaitingFor);
#ifdef TIMERDEBUG
    kprintf(KR_OSTREAM, "timer result=0x%x\n", result);
#endif
    assert(result == RC_OK);

#ifdef TIMERDEBUG
    kprintf(KR_OSTREAM, "timer awake\n");
#endif

    CMTEMutex_lock(&timerLock);

#ifdef TIMERDEBUG
    kprintf(KR_OSTREAM, "timer has awoken, state %d\n", timerThreadState);
#endif

    switch (timerThreadState) {
    case ttsWorking:
      assert(false);

    case ttsTeleporting:
      // Another process is teleporting us. Just wait while they do it.
      capros_Sleep_sleepTillPersistent(KR_SLEEP, capros_Sleep_infiniteTime);
      assert(false);	// shouldn't get here

    case ttsSleeping:
      timerThreadState = ttsWorking;
      break;
    }
    
    // Wake up any expired timers.
    while (1) {
      Link * cur = timerHead.next;
      if (cur == &timerHead)	// list is empty
        break;
      CMTETimer * tmr = LinkToTimer(cur);
      if (tmr->expiration > timeWaitingFor)
        break;		// not expired yet
      // This timer has expired.
#ifdef TIMERDEBUG
      kprintf(KR_OSTREAM, "Timer 0x%x expired.\n", tmr);
#endif
      // Get fn and data now, as the timer might be deallocated after
      // we have marked it not pending.
      void (*fn)(unsigned long) = tmr->function;
      unsigned long data = tmr->data;

      remove_timer(tmr);
      CMTEMutex_unlock(&timerLock);

#ifdef TIMERDEBUG
      kprintf(KR_OSTREAM, "Before timer function 0x%x", fn);
#endif

      (*fn)(data);	// Call the timer function.

#ifdef TIMERDEBUG
      kprintf(KR_OSTREAM, "After timer function");
#endif

      CMTEMutex_lock(&timerLock);
    }
  }
}

// Called with timerLock locked.
// Exits with timerLock unlocked.
static void
update_soonest_wait(Link * prev)
{
  result_t result;

  if (prev == &timerHead) {
    // We changed the first item on the list.

    CMTETime_t newWaitTime = GetSoonestExpiration();
#ifdef TIMERDEBUG
    kprintf(KR_OSTREAM, "twf=0x%llx new=0x%llx", timeWaitingFor, newWaitTime);
#endif
    if (newWaitTime < timeWaitingFor) {
#ifdef TIMERDEBUG
      kprintf(KR_OSTREAM, "newer time, state %d, ", timerThreadState);
#endif

      switch (timerThreadState) {
      case ttsSleeping:
        // timerThread is sleeping; need to get it to wake up sooner.
        timerThreadState = ttsTeleporting;

        /* Don't want to teleport while holding the lock. */
        CMTEMutex_unlock(&timerLock);

        // Regrettably, this is a lot of code in a common path.
#ifdef TIMERDEBUG
        kprintf(KR_OSTREAM, "Teleporting thread %d\n", timerThreadNum);
#endif
        result = capros_Node_getSlotExtended(KR_KEYSTORE,
                   LKSN_THREAD_PROCESS_KEYS + timerThreadNum, KR_TEMP0);
        assert(result == RC_OK);
        result = capros_Process_makeResumeKey(KR_TEMP0, KR_TEMP1);
        assert(result == RC_OK);
        {
          capros_Process_CommonRegisters32 regs;
          result = capros_Process_getRegisters32(KR_TEMP0, &regs);
          assert(result == RC_OK);
          regs.pc = (uint32_t)&DoJump;
          regs.procFlags &= ~capros_Process_PF_ExpectingMessage;
          result = capros_Process_setRegisters32(KR_TEMP0, regs);
          assert(result == RC_OK);
        }
        // Invoke the resume key to restart timerThread.
        Message msg = {
          .snd_invKey = KR_TEMP1,
          .snd_key0 = KR_VOID,
          .snd_key1 = KR_VOID,
          .snd_key2 = KR_VOID,
          .snd_rsmkey = KR_VOID,
          .snd_len = 0
          // It is not ExpectingMessage, so code, w1, w2, w3 don't matter.
        };
        PSEND(&msg);
        break;

      case ttsTeleporting:
        // Already teleporting, don't disturb it.
      case ttsWorking:
        // timerThread will recheck the time before sleeping.
        CMTEMutex_unlock(&timerLock);
        break;
      }
    }
    else
      CMTEMutex_unlock(&timerLock);
  }
  else
    CMTEMutex_unlock(&timerLock);
}

/* Call CMTETimer_setup once before using any timers.
 * Returns RC_capros_key_RequestError if called more than once.
 * Otherwise returns a result from CMTEThread_create. */
result_t
CMTETimer_setup(void)
{
  if (timerThreadNum != noThread)
    return RC_capros_key_RequestError;

  // Create the timer thread.
  return CMTEThread_create(timerThreadStackSize,
                           &timerThreadProc, NULL, &timerThreadNum);
}

void
CMTETimer_init(CMTETimer * timer,
  void (*function)(unsigned long), unsigned long data)
{
  timer->link.next = NULL;	// mark as not pending
  timer->function = function;
  timer->data = data;
}

/* Returns false if timer was inactive, true if was active. */
bool
CMTETimer_delete(CMTETimer * timer)
{
  if (CMTETimer_IsPending(timer)) {
    CMTEMutex_lock(&timerLock);
    // Check again - it may have just expired:
    if (CMTETimer_IsPending(timer)) {
      Link * prev = timer->link.prev;
      remove_timer(timer);
      update_soonest_wait(prev);
      return true;
    }
    CMTEMutex_unlock(&timerLock);
  }

  return false;
}

/* Returns false if timer was inactive, true if was active. */
bool
CMTETimer_setExpiration(CMTETimer * timer, CMTETime_t expirationTime)
{
  assert(timer->function);

#ifdef TIMERDEBUG
  kprintf(KR_OSTREAM, "CMTETimer_setExpiration 0x%x %d\n", timer, expirationTime);
#endif

  CMTEMutex_lock(&timerLock);

  bool ret = false;
  if (CMTETimer_IsPending(timer)) {
    link_UnlinkUnsafe(&timer->link);
    ret = true;
  }

  timer->expiration = expirationTime;

  // Insert into the ordered list.
  Link * cur = &timerHead;
  while (cur->next != &timerHead) {
    Link * nxt = cur->next;
    CMTETimer * nxtTimer = LinkToTimer(nxt);
    if (nxtTimer->expiration >= expirationTime)
      break;	// insert before nxt
    cur = nxt;
  }
  link_insertAfter(cur, &timer->link);

  update_soonest_wait(cur);
  return ret;
}

/* Returns false if timer was inactive, true if was active. */
bool
CMTETimer_setDuration(CMTETimer * timer, uint64_t durationNsec)
{
  return CMTETimer_setExpiration(timer, durationNsec + GetCurrentTime());
}

CMTETime_t
CMTETimer_remainingTime(CMTETimer * timer)
{
  CMTETime_t remaining = timer->expiration - GetCurrentTime();

  if (remaining > 0)
    return remaining;
  else return 0;
}
