/*
 * Copyright (C) 1998, 1999, Jonathan S. Shapiro.
 * Copyright (C) 2006, Strawberry Development Group.
 *
 * This file is part of the EROS Operating System.
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

#include <kerninc/kernel.h>
#include <kerninc/IRQ.h>
#include <kerninc/Machine.h>
#include <kerninc/Activity.h>
#include <kerninc/ObjectCache.h>
#include <kerninc/SysTimer.h>
#include <kerninc/CPU.h>

/* For EROS, the desired timer granularity is milliseconds.
 * Unfortunately this is very much too fast for some hardware.  The
 * rule is that you will sleep for as long as you asked for or one
 * hardclock tick, whichever is LONGER.  Also, you will NEVER sleep
 * for more than a year.
 */

struct Activity *ActivityChain = 0;

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

void
sysT_AddSleeper(Activity* t)
{
  Activity **sleeper = 0;
  register Activity *cur = 0;
  register Activity *next = 0;

  irq_DISABLE();

  if (ActivityChain == 0 || t->wakeTime < ActivityChain->wakeTime) {
    t->nextTimedActivity = ActivityChain;
    ActivityChain = t;
    sysT_ResetWakeTime();
  }
  else {
    sleeper = &(ActivityChain->nextTimedActivity);
    while (*sleeper && (*sleeper)->wakeTime < t->wakeTime) 
      sleeper = &((*sleeper)->nextTimedActivity);

    /* We are either off the end of the list or looking at one whose
     * wakeup value is greater than ours:
     */
    t->nextTimedActivity = *sleeper;
    *sleeper = t;
  }

#if 1
  /* Sanity check.  We know there is at least 1 activity on the list. */
  /*register Activity *cur = ActivityChain;*/
  cur = ActivityChain;
  do {
    /*register Activity *next = cur->nextTimedActivity;*/
    next = cur->nextTimedActivity;
    if (next)
      assert(cur->wakeTime <= next->wakeTime);
    cur = next;
  } while(cur);
#endif
  
#if 0
  if ( t.IsUser() )
    printf("added sleeper; now %u waketime %u nextwake %u\n",
		   (uint32_t) now, (uint32_t)t.wakeTime,
		   (uint32_t) wakeup);
#endif

  irq_ENABLE();
}



void
sysT_CancelAlarm(Activity* t)
{
  Activity *sleeper = 0;
  irq_DISABLE();

#if 0
  printf("Canceling alarm on activity 0x%x\n", &t);
#endif
  
  if (ActivityChain == t) {
    ActivityChain = t->nextTimedActivity;
    sysT_ResetWakeTime();
  }
  else {
    for ( sleeper = ActivityChain;
	  sleeper; 
	  sleeper = sleeper->nextTimedActivity ) {
      if (sleeper->nextTimedActivity == t) {
	sleeper->nextTimedActivity = t->nextTimedActivity;
	break;
      }
    }
  }

  irq_ENABLE();
}

void
sysT_BootInit()
{
}

/* Perform all wakeups to be done at (or before) the specified time.
   On exit, sysT_ResetWakeTime will set the wakeup time to a value > now.
   This procedure is called with IRQ disabled.  */
void
sysT_WakeupAt(uint64_t now)
{
  if (cpu->preemptTime <= now) {
    cpu->preemptTime = ~0llu;
    sysT_ActivityTimeout();
  }

  /* The awkward loop must be used because calling act_Wakeup
   * mutates the sleeper list.
   */
    
  while (ActivityChain && ActivityChain->wakeTime <= now) {
    register Activity *t = ActivityChain;
    ActivityChain = ActivityChain->nextTimedActivity;
    act_Dequeue(t);
    act_Wakeup(t);
  }
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
ObjectHeader *sysT_TimePageHdr = 0;

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

  irq_DISABLE();

  eros_tod_struct->tps_sinceboot.tv_secs = now_secs;
  eros_tod_struct->tps_sinceboot.tv_usecs = now_usecs;

  /* Time of day init zeroed usecs, so no need to do that
     arithmetic. */
  eros_tod_struct->tps_wall.tv_secs = wallBase.tv_secs + now_secs;
  eros_tod_struct->tps_wall.tv_usecs = now_usecs;

  irq_ENABLE();

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
    objC_ObHdrToPage(sysT_TimePageHdr);

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
