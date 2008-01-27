/*
 * Portions Copyright (C) 1991, 1992  Linus Torvalds
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

/* Emulation for Linux timer-related procedures.
*/

#include <setjmp.h>
#include <linuxk/linux-emul.h>
#include <linuxk/lsync.h>
#include <eros/Invoke.h>	// get RC_OK
#include <eros/machine/atomic.h>
#include <idl/capros/Sleep.h>
#include <idl/capros/Node.h>
#include <idl/capros/Process.h>
#include <domain/assert.h>
#include <linux/timer.h>
#include <linux/sched.h>

// #define TIMERDEBUG

spinlock_t timerLock = __SPIN_LOCK_UNLOCKED(timerLock);

// The following are protected by timerLock:
unsigned int timerThreadNum = noThread;
capros_Sleep_nanoseconds_t timeWaitingFor = capros_Sleep_infiniteTime;
LIST_HEAD(timerHead);
/* There is a separarate timer implementation for each driver, so we don't 
expect a long list of timers. So we just keep an ordered list. */

// Must be called under timerLock.
static inline capros_Sleep_nanoseconds_t
GetSoonestExpiration(void)
{
  if (list_empty(&timerHead))
    return capros_Sleep_infiniteTime;
  else
    return container_of(timerHead.next, struct timer_list, entry)
           ->caprosExpiration;
}

// Must be called under timerLock.
static inline void remove_timer(struct timer_list * timer)
{
  struct list_head *entry = &timer->entry;
  __list_del(entry->prev, entry->next);
  entry->next = NULL;	// mark no longer pending
  entry->prev = LIST_POISON2;
}

/*
The timerThread loops, waiting for the soonest expiration and waking up
any expired timers. 

The difficulty is that a timer with a sooner expiration may be added,
while the timerThread is stuck waiting for longer than that. 
When this happens, we must unstick the timerThread.
timerThreadState handles the synchronization for that. 

To understand the synchonization logic, we define four code regions.
timerThread is always executing in exactly one of these regions. 
The regions are:
  WorkRegion
  BeforeSleepRegion
  SleepRegion
  TeleportingRegion

Transitions between regions are noted in the code. 
Often a transition happens at the instant that timerThreadState
is atomically updated.

Whenever the timerLock is not held, the following is true:
   timerThreadState is ttsWorking
   and timerThread is in WorkRegion
       or timerThread is in BeforeSleepRegion
          and timeWaitingFor is the soonest expiration,
or timerThreadState is ttsSleeping
   and timerThread is in BeforeSleepRegion
       or timerThread is in SleepRegion
          and timeWaitingFor is the soonest expiration,
or timerThreadState is ttsTeleporting
   and timerThread is in TeleportingRegion.
         
 */

static const unsigned int timerThreadStackSize = 2048;

enum {
  ttsTeleporting = 0,
  ttsSleeping,
  ttsWorking
};
uint32_t timerThreadState = ttsWorking;

#define atomic_begin \
  oldVal = timerThreadState; \
  do {

#define atomic_end \
    newVal = capros_atomic_cmpxchg32(&timerThreadState, oldVal, newVal); \
    if (newVal == oldVal) break; \
    oldVal = newVal; \
  } while (1);

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
  unsigned long flags;
  unsigned long oldVal, newVal;
  result_t result;

  switch (setjmp(jb)) {
  case 0:
    break;

  default:	// longjmp comes here
#ifdef TIMERDEBUG
    kprintf(KR_OSTREAM, "Teleported.\n");
#endif
    atomic_begin
      assert(oldVal == ttsTeleporting);
      newVal = ttsWorking;	// begin WorkRegion
    atomic_end
  }
  spin_lock_irqsave(&timerLock, flags);
  while (1) {
    timeWaitingFor = GetSoonestExpiration();
    // Begin BeforeSleepRegion
    spin_unlock_irqrestore(&timerLock, flags);

    // Atomically update timerThreadState.
    static uint8_t beginSleepNextState[3] = {
      ttsTeleporting,	// from ttsTeleporting - should not happen
      ttsWorking,	// from ttsSleeping, begin WorkRegion
      ttsSleeping	// from ttsWorking, begin SleepRegion
    };
    atomic_begin
      newVal = beginSleepNextState[oldVal];
    atomic_end
    switch (newVal) {
    case ttsTeleporting:
      assert(false);

    case ttsWorking:
      goto getWaitTime;

    case ttsSleeping:
      break;
    }

    // Sleep.
    result = capros_Sleep_sleepTill(KR_SLEEP, timeWaitingFor);
    assert(result == RC_OK);

    // Atomically update timerThreadState.
    static uint8_t endSleepNextState[3] = {
      ttsTeleporting,	// from ttsTeleporting
      ttsWorking,	// from ttsSleeping, begin WorkRegion
      ttsSleeping	// from ttsWorking, should not happen
    };
    atomic_begin
      newVal = endSleepNextState[oldVal];
    atomic_end
    switch (newVal) {
    case ttsSleeping:
      assert(false);

    case ttsTeleporting:
      // Another process is teleporting us. Just wait while they do it.
      capros_Sleep_sleepTill(KR_SLEEP, capros_Sleep_infiniteTime);
      assert(false);	// shouldn't get here

    case ttsWorking:
      break;
    }
    
    spin_lock_irqsave(&timerLock, flags);

    // Wake up any expired timers.
    while (1) {
      struct list_head * cur = timerHead.next;
      if (cur == &timerHead)	// list is empty
        break;
      struct timer_list * tmr = container_of(cur, struct timer_list, entry);
      if (tmr->caprosExpiration > timeWaitingFor)
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
      spin_unlock_irqrestore(&timerLock, flags);

      (*fn)(data);	// Call the timer function.

      spin_lock_irqsave(&timerLock, flags);
    }
    continue;

getWaitTime:
    spin_lock_irqsave(&timerLock, flags);
  }
}

static void
ensure_timer_thread(void)
{
  unsigned long flags;

  spin_lock_irqsave(&timerLock, flags);
  if (timerThreadNum == noThread) {
    // Create the timer thread.
    timerThreadNum = noThread2;	// prevent duplicate creations
    spin_unlock_irqrestore(&timerLock, flags);
    result_t result = lthread_new_thread(timerThreadStackSize,
                        &timerThreadProc, NULL, &timerThreadNum);
    if (result != RC_OK) {
      printk("Unable to start timer thread, reason = 0x%x\n", result);
      assert(false);	// fatal error since there's no way to report it.
    }
  }
  else
    spin_unlock_irqrestore(&timerLock, flags);
}

void fastcall
init_timer(struct timer_list * timer)
{
  timer->entry.next = NULL;	// mark as not pending

  ensure_timer_thread();
}

// Called with timerLock held.
// Exits with spin_unlock_irqrestore(timerLock, flags).
void
update_soonest_wait(struct list_head * prev, unsigned long flags)
{
  result_t result;

  if (prev == &timerHead) {
    // We changed the first item on the list.

    capros_Sleep_nanoseconds_t newWaitTime = GetSoonestExpiration();
    if (newWaitTime < timeWaitingFor) {
      unsigned long oldVal, newVal;
      // If timerThread is sleeping, need to get it to wake up sooner.

      // Atomically update timerThreadState.
      static uint8_t teleportNextState[3] = {
        ttsTeleporting,	// from ttsTeleporting
        ttsTeleporting,	// from ttsSleeping, timerThread begins TeleportingRegion
        ttsSleeping	// from ttsWorking
      };

      oldVal = timerThreadState;
      do {
        newVal = capros_atomic_cmpxchg32(&timerThreadState, oldVal,
                   teleportNextState[oldVal]);
        if (newVal == oldVal) break;	// exchange succeeded
        oldVal = newVal;
      } while (1);

      /* Don't want to teleport while holding the lock. */
      spin_unlock_irqrestore(&timerLock, flags);

      switch (oldVal) {
      case ttsSleeping:
        // Need to teleport timerThread out of its sleep.
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

      case ttsWorking:
        /* In case timerThread was in BeforeSleepRegion, changing
        timerThreadState to ttsSleeping signals it to recheck timeWaitingFor. */
        break;

      case ttsTeleporting:
        break;	// was already teleporting, nothing to do
      }
    }
    else
      spin_unlock_irqrestore(&timerLock, flags);
  }
  else
    spin_unlock_irqrestore(&timerLock, flags);
}

/* Returns 0 if timer was inactive, 1 if was active. */
int
del_timer(struct timer_list * timer)
{
  unsigned long flags = 0;
  int ret = 0;

  if (timer_pending(timer)) {
    spin_lock_irqsave(&timerLock, flags);
    // Check again - it may have just expired:
    if (timer_pending(timer)) {
      struct list_head * prev = timer->entry.prev;
      remove_timer(timer);
      ret = 1;
      update_soonest_wait(prev, flags);
    }
    else
      spin_unlock_irqrestore(&timerLock, flags);
  }

  return ret;
}

/* Returns 0 if timer was inactive, 1 if was active. */
int
__mod_timer(struct timer_list * timer, unsigned long expires)
{
  unsigned long flags = 0;
  int ret = 0;

  BUG_ON(!timer->function);

  ensure_timer_thread();

  spin_lock_irqsave(&timerLock, flags);
  if (timer_pending(timer)) {
    struct list_head *entry = &timer->entry;
    __list_del(entry->prev, entry->next);
    entry->prev = LIST_POISON2;

    ret = 1;
  }

  timer->expires = expires;
  timer->caprosExpiration = jiffies_to_usecs(expires)*1000;	// nanoseconds

  // Insert into the ordered list.
  struct list_head * cur = &timerHead;
  while (cur->next != &timerHead) {
    struct list_head * nxt = cur->next;
    if (container_of(nxt, struct timer_list, entry)->expires >= expires)
      break;	// insert before this element
    cur = nxt;
  }
  __list_add(&timer->entry, cur, cur->next);	// insert after cur

  update_soonest_wait(cur, flags);
  return ret;
}

/* Returns 0 if timer was inactive, 1 if was active. */
int
mod_timer(struct timer_list * timer, unsigned long expires)
{
        /*
         * If the timer is modified to be the same, then just return:
         */
        if (timer->expires == expires && timer_pending(timer))
                return 1;

        return __mod_timer(timer, expires);
}

unsigned long
timer_remaining_time(struct timer_list * timer)
{
  unsigned long now = capros_getJiffies();
  if (now < timer->expires)
    return timer->expires - now;
  else return 0;
}

void
add_timer_on(struct timer_list *timer, int cpu)
{
  // We don't support this.
  add_timer(timer);
}

signed long
schedule_timeout_interruptible(signed long timeout)
{
  return schedule_timeout_uninterruptible(timeout);
}

signed long
schedule_timeout_uninterruptible(signed long timeout)
{
  capros_Sleep_sleep(KR_SLEEP, jiffies_to_msecs(timeout));
  return 0;	// no signals
}
