/*
 * Copyright (C) 1998, 1999, Jonathan S. Shapiro.
 * Copyright (C) 2006, 2007, 2008, Strawberry Development Group.
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

/* The system time, the last time we read it. 
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

bool
IsLeapYear(uint32_t yr)
{
  if (yr % 400 == 0)
    return true;
  if (yr % 100 == 0)
    return false;
  if (yr % 4 == 0)
    return true;
  return false;
}

// May Yield.
uint64_t
sysT_NowPersistent(void)
{
  // monotonicTimeOfRestart isn't valid until restart is done.
  if (! restartIsDone()) {
    SleepOnPFHQueue(&RestartQueue);
  }

  return mach_TicksToNanoseconds(sysT_Now()) + monotonicTimeOfRestart;
}

uint64_t
sysT_WakeupTime(void)
{
  uint64_t ret = cpu->preemptTime;
  if (! sq_IsEmpty(&SleepQueue)) {
    Activity * t = container_of(SleepQueue.q_head.next, Activity, q_link);
    if (t->wakeTime < ret)
      return t->wakeTime;
  }
  return ret;
}

void
sysT_AddSleeper(Activity * t, uint64_t wakeTime)
{
  assert(link_isSingleton(&t->q_link));
  assert(t->context);
  assert(t->context->runState == RS_Waiting);

#if 0
  uint64_t now = sysT_Now();
  dprintf(false, "AddSleeper act=%#x wakeTime=%#llu now %#llx dur=%lld\n",
     t, wakeTime, now, wakeTime - now);
#endif

  t->lastq = &SleepQueue;
  t->wakeTime = wakeTime;

  irqFlags_t flags = local_irq_save();

  // Insert into SleepQueue, which is ordered.
  Link * cur = &SleepQueue.q_head;
  Link * nxt;
  while (nxt = cur->next, nxt != &SleepQueue.q_head) {
    if (container_of(nxt, Activity, q_link)->wakeTime >= wakeTime)
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
sysT_actWake(Activity * act, result_t rc)
{
  Process * invokee = act->context;
  assert(invokee);

  // The process's state may have been changed, so revalidate:
  if (invokee->runState != RS_Waiting)
    return;

  invokee->runState = RS_Running;

  keyR_ZapResumeKeys(&invokee->keyRing);

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
   This procedure is called from the clock interrupt with IRQ disabled.  */
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
    if (t->wakeTime > now)
      break;
    // Wake up this Activity.
    link_Unlink(&t->q_link);
    act_Wakeup(t);

    // Complete this process's sleep invocation.

    Process * invokee = t->context;
    if (invokee && proc_IsRunnable(invokee)) {
      // Return from its Sleep invocation.
      sysT_actWake(t, RC_OK);
    } else {
      t->actHazard = actHaz_WakeOK;	// remember to do it later
    }
  }

  sysT_ResetWakeTime();
}

#ifdef KKT_TIMEPAGE
#error this is not the case.
/* This is hopelessly stale! */
#include <eros/TimePage.h>
#include <eros/TimeOfDay.h>
/*#include <kerninc/PhysMem.h>*/

/**************************************************
 * SUPPORT FOR THE TIME PAGE
 **************************************************/
static volatile TimePageStruct *eros_tod_struct = 0;
PageHeader * sysT_TimePageHdr = 0;

Timer TimePageTimer;

struct timeval wallBase;

void TimePageTick(Timer *t)
{
  uint64_t timenow = sysT_Now();
  uint32_t now_secs = 0;
  uint32_t now_usecs = 0;

  timenow = mach_TicksToMilliseconds(timenow);

  now_secs = timenow/1000;
  now_usecs = timenow % 1000;

  irqFlags_t flags = local_irq_save();

  eros_tod_struct->tps_sinceboot.tv_secs = now_secs;
  eros_tod_struct->tps_sinceboot.tv_usecs = now_usecs;

  /* Time of day init zeroed usecs, so no need to do that
     arithmetic. */
  eros_tod_struct->tps_wall.tv_secs = wallBase.tv_secs + now_secs;
  eros_tod_struct->tps_wall.tv_usecs = now_usecs;

  local_irq_restore(flags);

  tim_WakeupIn(&TimePageTimer, 5ul, TimePageTick);
}

#define HRS_PER_DAY 24
#define MINS_PER_HR 60
#define SECS_PER_MIN 60
#define SECS_PER_HR (SECS_PER_MIN * MINS_PER_HR)
#define SECS_PER_DAY (HRS_PER_DAY * SECS_PER_HR)

void
sysT_InitTimePage()
{
  TimeOfDay tod;
  printf("Fabricating TOD Page\n");

  sysT_TimePageHdr = objC_GrabPageFrame();
  
  eros_tod_struct = (TimePageStruct *)
    pageH_GetPageVAddr(sysT_TimePageHdr);

  eros_tod_struct->tps_version = TIMEPAGE_VERSION;
  eros_tod_struct->tps_sinceboot.tv_secs = 0;
  eros_tod_struct->tps_sinceboot.tv_usecs = 0;
  eros_tod_struct->tps_wall.tv_secs = 0;
  eros_tod_struct->tps_wall.tv_usecs = 0;

  mach_GetHardwareTimeOfDay(&tod);
  wallBase.tv_usecs = 0;
  wallBase.tv_secs = tod.utcDay * SECS_PER_DAY;

  tim_WakeupIn(&TimePageTimer, 5ul, TimePageTick);
}

#endif
