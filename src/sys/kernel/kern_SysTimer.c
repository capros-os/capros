/*
 * Copyright (C) 1998, 1999, Jonathan S. Shapiro.
 * Copyright (C) 2006, 2007, 2008, 2009, Strawberry Development Group.
 *
 * This file is part of the CapROS Operating System,
 * and is derived from the EROS Operating System.
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
#include <kerninc/IRQ.h>
#include <kerninc/Machine.h>
#include <kerninc/Activity.h>
#include <kerninc/ObjectCache.h>
#include <kerninc/SysTimer.h>
#include <kerninc/CPU.h>
#include <kerninc/CpuReserve.h>
#include <kerninc/Invocation.h>
#include <kerninc/Ckpt.h>
#include <kerninc/IORQ.h>

/* The system time, the last time we read it, in ticks.
Call sysT_Now() to update this. 
Read with irq disabled, because it may be updated in an interrupt. */
uint64_t sysT_latestTime;

DEFQUEUE(SleepQueue);

/* Using 8 microseconds instead of 1 microsecond gives better resolution
on slow processors. */
uint32_t loopsPer8us;	// number of loops of mach_Delay in 8 microseconds

/* Delay for w microseconds. */
void
SpinWaitUs(uint32_t w)
{
  assert(w <= 32768);	// else we risk overflow below
  w *= loopsPer8us;
  w /= 8;
  if (w > 0)
    mach_Delay(w);
}

uint64_t lastUnique = 0;
/* Return the current time in nanoseconds, but always a unique increasing value.
 */
uint64_t
sysT_NowUniqueNS(void)
{
  uint64_t now = mach_TicksToNanoseconds(sysT_Now());
  // printf("sysT_NowUniqueNS: now %llu last %llu\n", now, lastUnique);
  if (now <= lastUnique) {
    now = lastUnique + 1;
  }
  lastUnique = now;
  return now;
}

// May Yield.
uint64_t
sysT_NowPersistent(void)
{
  // monotonicTimeOfRestart isn't valid until restart is done.
  WaitForRestartDone();

  return sysT_NowUniqueNS() + monotonicTimeOfRestart;
}

uint64_t
sysT_WakeupTime(void)
{
  uint64_t ret = cpu->preemptTime;
  if (! sq_IsEmpty(&SleepQueue)) {
    Activity * t = container_of(SleepQueue.q_head.next, Activity, q_link);
    assertex(t, t->state == act_Sleeping);
    if (t->u.wakeTime < ret)
      return t->u.wakeTime;
  }
  return ret;
}

void
sysT_AddSleeper(Activity * t, uint64_t wakeTime)
{
  assert(link_isSingleton(&t->q_link));
  assert(act_HasProcess(t));
  Process * proc = act_GetProcess(t);
  (void)proc;	// for the compiler
  assert(proc->curActivity == t);
  assert(! (proc->hazards & hz_DomRoot));
  assert(proc->runState == RS_Waiting);

#if 0
  uint64_t now = sysT_Now();
  dprintf(false, "AddSleeper act=%#x wakeTime=%#llu now %#llx dur=%lld\n",
     t, wakeTime, now, wakeTime - now);
#endif

  t->lastq = &SleepQueue;
  t->u.wakeTime = wakeTime;
  t->state = act_Sleeping;

  irqFlags_t flags = local_irq_save();

  // Insert into SleepQueue, which is ordered.
  Link * cur = &SleepQueue.q_head;
  Link * nxt;
  while (nxt = cur->next, nxt != &SleepQueue.q_head) {
    if (container_of(nxt, Activity, q_link)->u.wakeTime >= wakeTime)
      break;	// insert before nxt
    cur = nxt;
  }
  link_insertBetween(&t->q_link, cur, nxt);

  if (cur == &SleepQueue.q_head)	// inserted at the front
    sysT_ResetWakeTime();

  local_irq_restore(flags);
}


void
sysT_CancelAlarm(Activity * t)
{
  irqFlags_t flags = local_irq_save();

#if 0
  printf("Canceling alarm on activity 0x%x\n", &t);
#endif

  if (SleepQueue.q_head.next == &t->q_link)	// if first on the list
    sysT_ResetWakeTime();

  link_Unlink(&t->q_link);
  
  local_irq_restore(flags);
}

void
sysT_BootInit()
{
}

void
sysT_procWake(Process * invokee, result_t rc)
{
  assert(! (invokee->hazards & hz_DomRoot));

  invokee->runState = RS_Running;

  proc_ZapResumeKeys(invokee);

  if (proc_IsExpectingMsg(invokee)) {
    // We may be in an interrupt, so we can't use the global inv.
    Invocation tempInv = {
      .exit.code = rc,
      .exit.w1 = 0,
      .exit.w2 = 0,
      .exit.w3 = 0,
      .sentLen = 0
    };
    proc_DeliverResult(invokee, &tempInv);
    proc_AdvancePostInvocationPC(invokee);
  }
}

/* Perform all wakeups to be done at (or before) the specified time.
   After exit, caller must recalculate the wakeup time.
   This procedure is called from HandleDeferredWork, so we know we
   did not interrupt the kernel. */
void
sysT_WakeupAt(void)
{
  uint64_t now = sysT_latestTime;
  if (cpu->preemptTime <= now) {
    cpu->preemptTime = UINT64_MAX;
    res_ActivityTimeout(now);
  }

  while (! sq_IsEmpty(&SleepQueue)) {
    Activity * t = container_of(SleepQueue.q_head.next, Activity, q_link);
    assert(t->state == act_Sleeping);
    if (t->u.wakeTime > now)
      break;
    // Wake up this Activity.
    link_Unlink(&t->q_link);
    act_Wakeup(t);
//#define RESPONSE_TEST
#ifdef RESPONSE_TEST
    assert(act_HasProcess(t));	// true?
    extern void ClockWakeup(Process * proc);
    ClockWakeup(act_GetProcess(t));
#endif

    // Complete this process's sleep invocation.

    if (act_HasProcess(t) && proc_IsRunnable(act_GetProcess(t))) {
      // Return from its Sleep invocation.
      sysT_procWake(act_GetProcess(t), RC_OK);
    } else {
      t->actHazard = actHaz_WakeOK;	// remember to do it later
    }
  }

  sysT_ResetWakeTime();
}
