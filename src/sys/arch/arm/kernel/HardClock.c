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
Research Projects Agency under Contract Nos. W31P4Q-06-C-0040 and
W31P4Q-07-C-0070.  Approved for public release, distribution unlimited. */

#include <kerninc/kernel.h>
#include <kerninc/Machine.h>
#include <kerninc/Activity.h>
#include <kerninc/SysTimer.h>
#include <kerninc/CPU.h>
#include <kerninc/IRQ.h>
#include "ep93xx-timer.h"
#include "ep93xx-vic.h"
#include "Interrupt.h"

#define VIC1 (VIC1Struct(AHB_VA + VIC1_AHB_OFS))
#define VIC2 (VIC2Struct(AHB_VA + VIC2_AHB_OFS))
#define Timers (TimersStruct(APB_VA + TIMER_APB_OFS))

/* The Cirrus EP93xx has four timers.

We use Timer 3 free-running to keep track of the current time since boot.
(See sysT_Now for details.)

We use Timer 1 to interrupt when we next need to wake up.

We run both timers at 2 kHz (more precisely, 1.9939 kHz).

We don't use timer 4 currently. It overflows in 12.9 days, and there is no
interrupt, so code must sample and count the overflows. 
We should use this instead of timer 3.

We don't use the tick (64 Hz) interrupt at all.
 */

volatile uint32_t sysT_now_high;
volatile uint64_t sysT_wakeup = UINT64_MAX;

// 501530 is 1000000 ns / 1.9939 kHz.
uint64_t
mach_MillisecondsToTicks(uint64_t ms)
{
  return (ms * 100000) / 50153;
}

uint64_t
mach_TicksToMilliseconds(uint64_t ms)
{
  return (ms * 50153) / 100000;
}

uint64_t
mach_NanosecondsToTicks(uint64_t ns)
{
  return ns / 501530;
}

uint64_t
mach_TicksToNanoseconds(uint64_t ms)
{
  return ms * 501530;
}

/* Timer 3 counts down in 32 bits at 2 kHz, so it will underflow in
   about 24.27 days.
   When it does, this interrupt service routine increments the
   high-order word of the time. */
void
TC3OIHandler(VICIntSource * vis)
{
  (void)VIC2.VectAddr;	// read it to mask interrupts of lower or equal priority
  irq_ENABLE_for_IRQ();
  Timers.Timer3.Clear = 0;	// clear the interrupt
  sysT_now_high++;
  irq_DISABLE();
  VIC2.VectAddr = 0;	// write it to reenable interrupts
			// of lower or equal priority
}

static void
ReloadWakeupTimer(uint64_t now)
{
  /* Load the wakeup time into Timer 1. */
  uint32_t timerValue;	// holds 16 bits
  if (sysT_wakeup <= now) {
    /* This can happen when called from sysT_ResetWakeTime.
       We should fix that to use a stable value of now. */
    timerValue = 1;	// minimum wait time
  } else {
    /* Because of the above test, the following subtraction cannot overflow. */
    uint64_t timeToWait = sysT_wakeup - now;
    /* Timer 1 is only 16 bits long */
    const unsigned int maxTimerValue = (1ul << 16) -1;
    if (timeToWait > maxTimerValue) {
      timerValue = maxTimerValue;
    } else {
      timerValue = timeToWait;
    }
  }

  Timers.Timer1.Control = TimerControlModePeriodic
                          | TimerControlClksel2k; // disable timer
  Timers.Timer1.Load = timerValue;
  Timers.Timer1.Control = TimerControlEnable | TimerControlModePeriodic
                          | TimerControlClksel2k;
}

uint64_t
sysT_Now(void)
{
  uint32_t high1;
  uint32_t high2;
  uint32_t low;
  bool lowCarry;
  do {
    high1 = sysT_now_high;

    /* We might arrive here with IRQ disabled.
       In that case, timer 3 could decrement to zero without the
       opportunity to interrupt and increment sysT_now_high.
       We check for this by testing whether there is a pending
       interrupt for timer 3.
       On the other hand, if the Timer3 interrupt is enabled,
       we will never see a pending interrupt for it. */
    /* Take the negative of the timer value, because it counts down. */
    low = -(Timers.Timer3.Value);	/* ignoring overflow */
    lowCarry = VIC2.RawIntr & (1ul << (VIC_Source_TC3OI - 32));

    high2 = sysT_now_high;
  } while (high1 != high2);

  uint64_t now = (((uint64_t)sysT_now_high
                   + (lowCarry ? 1 : 0) ) << 32) + low;
#if 0
  printf("Now=%x\n", (uint32_t)now);
#endif
  sysT_latestTime = now;
  return now;
}

INLINE void
RecalcWakeupTime(void)
{
  sysT_wakeup = sysT_WakeupTime();
}

void
sysT_ResetWakeTime(void)
{
  RecalcWakeupTime();

  ReloadWakeupTimer(sysT_Now());
}

void
TC1OIHandler(VICIntSource * vis)
{
  // We are on VIC1, no need to read VIC1VectAddr.
  Timers.Timer1.Clear = 0;	// clear the interrupt

  /* Capture a stable value of now to use for all subsequent calculations. */
  uint64_t now = sysT_Now();

  if (sysT_wakeup <= now) {	// wake up now
    /* Processing wakeups on the SleepQueue requires completing the
    sleeping processes' invocations. 
    If we did that work now, all the variables involved would have to be
    protected with irq disable.
    To avoid that, we just set the flag dw_timer here,
    and do the wakeups in ExitTheKernel (a "software interrupt"). */

    /* Nothing to wait for until that work is done. */
    sysT_wakeup = UINT64_MAX;
    deferredWork |= dw_timer;
  }
  /* If sysT_wakeup > now, it is because the time could not hold the
     full wait time. Just reload the timer. */
  ReloadWakeupTimer(now);
  VIC1.VectAddr = 0;	// write it to reenable interrupts
			// of lower or equal priority
}

void
mach_InitHardClock(void)
{
  InterruptSourceSetup(VIC_Source_TC1OI, 15, &TC1OIHandler);
  RecalcWakeupTime();
  /* Not necessary to call ReloadWakeupTimer(), because there is
     nothing to wake up yet. */

  InterruptSourceSetup(VIC_Source_TC3OI, 0, TC3OIHandler);
  Timers.Timer3.Control = TimerControlClksel2k;	// ensure disabled
  Timers.Timer3.Load = 0;
  sysT_now_high = 0;
  Timers.Timer3.Control = TimerControlEnable | TimerControlModeFreeRun
                          | TimerControlClksel2k;

  printf("Calibrating SpinWait... ");

  uint32_t count = 0;

  /* Wait for timer 3 to tick. */
  uint32_t t = Timers.Timer3.Value;
  while (t == Timers.Timer3.Value) ;

  /* Measure time to the next tick. */
  t--;
  while (t == Timers.Timer3.Value) {
    count ++;
    mach_Delay(256);
  }
  // count * 256 is approximate # of loops per tick.

  // Now get a more accurate measurement by reducing the number of
  // iterations checking Timers.Timer3.Value.
  if (count > 0)
    mach_Delay((count - 1)*256);	// majority of waiting is here
  unsigned int count2 = 0;
  t--;
  while (t == Timers.Timer3.Value) {
    count2 ++;
    mach_Delay(256);
  }

  printf("%d, %d, ", count, count2);

  count += count2 - 1;		// counts per tick
  assert(count < (1UL << (32 - 11)));	// else the following will overflow
  count *= 256;		// loops per tick
  count *= 8;		// loops per 8 ticks
  // 1 tick is 501 microseconds.
  count /= 501;		// loops per 8 microseconds
  assert(count < (1UL << (32 - 15)));	// else could overflow in SpinWaitUs
  loopsPer8us = count;
  printf("%d\n", loopsPer8us);

#if 0
  // Test it:
  for (count = 0; count < 31; count++)
    SpinWaitUs(32767);
  printf("End of 1 second delay.\n");
#endif

  /* Enable timer interrupts. */
  InterruptSourceEnable(VIC_Source_TC1OI);
  InterruptSourceEnable(VIC_Source_TC3OI);
}
