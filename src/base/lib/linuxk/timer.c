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
#include <idl/capros/Sleep.h>
#define capros_Sleep_infiniteTime UINT64_MAX
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

/**
 * __round_jiffies - function to round jiffies to a full second
 * @j: the time in (absolute) jiffies that should be rounded
 * @cpu: the processor number on which the timeout will happen
 *
 * __round_jiffies() rounds an absolute time in the future (in jiffies)
 * up or down to (approximately) full seconds. This is useful for timers
 * for which the exact time they fire does not matter too much, as long as
 * they fire approximately every X seconds.
 *
 * By rounding these timers to whole seconds, all such timers will fire
 * at the same time, rather than at various times spread out. The goal
 * of this is to have the CPU wake up less, which saves power.
 *
 * The exact rounding is skewed for each processor to avoid all
 * processors firing at the exact same time, which could lead
 * to lock contention or spurious cache line bouncing.
 *
 * The return value is the rounded version of the @j parameter.
 */
unsigned long __round_jiffies(unsigned long j, int cpu)
{
	int rem;
	unsigned long original = j;

	/*
	 * We don't want all cpus firing their timers at once hitting the
	 * same lock or cachelines, so we skew each extra cpu with an extra
	 * 3 jiffies. This 3 jiffies came originally from the mm/ code which
	 * already did this.
	 * The skew is done by adding 3*cpunr, then round, then subtract this
	 * extra offset again.
	 */
	j += cpu * 3;

	rem = j % HZ;

	/*
	 * If the target jiffie is just after a whole second (which can happen
	 * due to delays of the timer irq, long irq off times etc etc) then
	 * we should round down to the whole second, not up. Use 1/4th second
	 * as cutoff for this rounding as an extreme upper bound for this.
	 */
	if (rem < HZ/4) /* round down */
		j = j - rem;
	else /* round up */
		j = j - rem + HZ;

	/* now that we have rounded, subtract the extra skew again */
	j -= cpu * 3;

	if (j <= jiffies) /* rounding ate our timeout entirely; */
		return original;
	return j;
}

#if 0 // CapROS
/**
 * __round_jiffies_relative - function to round jiffies to a full second
 * @j: the time in (relative) jiffies that should be rounded
 * @cpu: the processor number on which the timeout will happen
 *
 * __round_jiffies_relative() rounds a time delta  in the future (in jiffies)
 * up or down to (approximately) full seconds. This is useful for timers
 * for which the exact time they fire does not matter too much, as long as
 * they fire approximately every X seconds.
 *
 * By rounding these timers to whole seconds, all such timers will fire
 * at the same time, rather than at various times spread out. The goal
 * of this is to have the CPU wake up less, which saves power.
 *
 * The exact rounding is skewed for each processor to avoid all
 * processors firing at the exact same time, which could lead
 * to lock contention or spurious cache line bouncing.
 *
 * The return value is the rounded version of the @j parameter.
 */
unsigned long __round_jiffies_relative(unsigned long j, int cpu)
{
	/*
	 * In theory the following code can skip a jiffy in case jiffies
	 * increments right between the addition and the later subtraction.
	 * However since the entire point of this function is to use approximate
	 * timeouts, it's entirely ok to not handle that.
	 */
	return  __round_jiffies(j + jiffies, cpu) - jiffies;
}
EXPORT_SYMBOL_GPL(__round_jiffies_relative);
#endif // CapROS

/**
 * round_jiffies - function to round jiffies to a full second
 * @j: the time in (absolute) jiffies that should be rounded
 *
 * round_jiffies() rounds an absolute time in the future (in jiffies)
 * up or down to (approximately) full seconds. This is useful for timers
 * for which the exact time they fire does not matter too much, as long as
 * they fire approximately every X seconds.
 *
 * By rounding these timers to whole seconds, all such timers will fire
 * at the same time, rather than at various times spread out. The goal
 * of this is to have the CPU wake up less, which saves power.
 *
 * The return value is the rounded version of the @j parameter.
 */
unsigned long round_jiffies(unsigned long j)
{
	return __round_jiffies(j, raw_smp_processor_id());
}

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
  unsigned long flags;
  result_t result;

  switch (setjmp(jb)) {
  case 0:
    spin_lock_irqsave(&timerLock, flags);
    break;

  default:	// longjmp comes here
    spin_lock_irqsave(&timerLock, flags);

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

    spin_unlock_irqrestore(&timerLock, flags);

    // Sleep.
    result = capros_Sleep_sleepTill(KR_SLEEP, timeWaitingFor);
#ifdef TIMERDEBUG
    kprintf(KR_OSTREAM, "timer result=0x%x\n", result);
#endif
    assert(result == RC_OK);

#ifdef TIMERDEBUG
    kprintf(KR_OSTREAM, "timer awake\n");
#endif

    spin_lock_irqsave(&timerLock, flags);

#ifdef TIMERDEBUG
    kprintf(KR_OSTREAM, "timer has awoken, state %d\n", timerThreadState);
#endif

    switch (timerThreadState) {
    case ttsWorking:
      assert(false);

    case ttsTeleporting:
      // Another process is teleporting us. Just wait while they do it.
      capros_Sleep_sleepTill(KR_SLEEP, capros_Sleep_infiniteTime);
      assert(false);	// shouldn't get here

    case ttsSleeping:
      timerThreadState = ttsWorking;
      break;
    }
    
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

#ifdef TIMERDEBUG
      kprintf(KR_OSTREAM, "Before timer function 0x%x", fn);
#endif

      (*fn)(data);	// Call the timer function.

#ifdef TIMERDEBUG
      kprintf(KR_OSTREAM, "After timer function");
#endif

      spin_lock_irqsave(&timerLock, flags);
    }
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
        spin_unlock_irqrestore(&timerLock, flags);

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
        spin_unlock_irqrestore(&timerLock, flags);
        break;
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
__mod_timer_duration(struct timer_list * timer, unsigned long duration,
                     uint64_t now64)
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

  unsigned long now = (unsigned long) now64;	// jiffies, correctly truncated
  unsigned int expires = now + duration;
  timer->expires = expires;	// for the client, not for us

  uint64_t caprosExpiration
    = timer->caprosExpiration
    = jiffies64_to_usecs(now64 + duration) * 1000;	// nanoseconds

#if 0
  printk("mod_timer, exp=%u jif=%u capexp=%llu\n",
         expires, now, timer->caprosExpiration);
#endif

  // Insert into the ordered list.
  struct list_head * cur = &timerHead;
  while (cur->next != &timerHead) {
    struct list_head * nxt = cur->next;
    struct timer_list * nxtTimer = container_of(nxt, struct timer_list, entry);
    if (nxtTimer->caprosExpiration >= caprosExpiration)
      break;	// insert before nxt
    cur = nxt;
  }
  __list_add(&timer->entry, cur, cur->next);	// insert after cur

  update_soonest_wait(cur, flags);
  return ret;
}

/* Returns 0 if timer was inactive, 1 if was active. */
int
__mod_timer(struct timer_list * timer, unsigned long expires)
{
  /* jiffies overflows 32 bits in 497 days (at HZ=100).
  We have to assume that no one waits for longer than half that.
  The following calculations will convert the 32-bit expires time
  to a 64 bit true expiration time. */
  uint64_t now64 = get_jiffies_64();
  unsigned long now = (unsigned long) now64;	// jiffies, correctly truncated

  /* Ignore any overflow on the following subtraction: */
  int32_t duration = expires - now;

  return __mod_timer_duration(timer, duration, now64);
}

/* Returns 0 if timer was inactive, 1 if was active. */
int
mod_timer(struct timer_list * timer, unsigned long expires)
{
#ifdef TIMERDEBUG
	kprintf(KR_OSTREAM, "mod_timer 0x%x %d\n", timer, expires);
#endif
        /*
         * If the timer is modified to be the same, then just return:
         */
        if (timer->expires == expires && timer_pending(timer))
                return 1;

        return __mod_timer(timer, expires);
}

/* Returns 0 if timer was inactive, 1 if was active. */
int
mod_timer_duration(struct timer_list * timer, unsigned long duration)
{
#ifdef TIMERDEBUG
  kprintf(KR_OSTREAM, "mod_timer_dur 0x%x %d\n", timer, duration);
#endif

  return __mod_timer_duration(timer, duration, get_jiffies_64());
}

unsigned long
timer_remaining_time(struct timer_list * timer)
{
  /* Ignore any overflow on the following subtraction.
     It must be due to jiffies overflowing 32 bits, 
     not to the remaining time overflowing. */
  long remaining = timer->expires - capros_getJiffies();

  if (remaining > 0)
    return remaining;
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
