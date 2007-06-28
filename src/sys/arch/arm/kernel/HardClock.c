/*
 * Copyright (C) 1998, 1999, Jonathan S. Shapiro.
 * Copyright (C) 2006, 2007, Strawberry Development Group.
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
#include <arch-kerninc/IRQ-inline.h>
#include "ep93xx-timer.h"
#include "ep93xx-vic.h"
#include "Interrupt.h"

/* The Cirrus EP93xx has three timers.

We use Timer 3 free-running to keep track of the current time since boot.
(See sysT_Now for details.)

We use Timer 1 to interrupt when we next need to wake up.

We run both timers at 2 kHz (more precisely, 1.9939 kHz).

We don't use the tick (64 Hz) interrupt at all.
 */

volatile uint32_t sysT_now_high;
volatile uint64_t sysT_wakeup = ~(0llu);
static uint32_t usec_calibration_count = 0;

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
  irq_ENABLE();
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
  return now;
}

static void
RecalcWakeupTime(void)
{
  if (ActivityChain && ActivityChain->wakeTime < cpu->preemptTime) {
    sysT_wakeup = ActivityChain->wakeTime;
  } else {
    sysT_wakeup = cpu->preemptTime;
  }
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
  irq_ENABLE();
  Timers.Timer1.Clear = 0;	// clear the interrupt

  /* Capture a stable value of now to use for all subsequent calculations. */
  uint64_t now = sysT_Now();

  if (sysT_wakeup <= now) {	// wake up now
    irq_DISABLE();
  
    sysT_WakeupAt(now);
    RecalcWakeupTime();

    irq_ENABLE();
#if 0
    /* Note, kernel printf does not support long long sizes. */
    printf("Timer 1 woke up, now=%x%08x, wakeup=%x%08x\n",
       (uint32_t)(now>>32), (uint32_t)now,
       (uint32_t)(sysT_wakeup>>32), (uint32_t)sysT_wakeup);
#endif
  }
  /* If sysT_wakeup > now, it is because the time could not hold the
     full wait time. Just reload the timer. */
  ReloadWakeupTimer(now);
  irq_DISABLE();
  VIC1.VectAddr = 0;	// write it to reenable interrupts
			// of lower or equal priority
}

void
mach_InitHardClock(void)
{
  int i;

  InterruptSourceSetup(VIC_Source_TC1OI, 15, TC1OIHandler);
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
  t--;

#if 1
  /* Measure time to the next tick. */
  while (t == Timers.Timer3.Value) {
    count ++;
    for (i = 256; i > 0; i--) {
    }
  }
#else
  t -= 10;	// Measure for 10 ticks
  while (t != Timers.Timer3.Value) {
    count ++;
    for (i = 256; i > 0; i--) {
    }
  }
#endif

  printf("%d\n", count);
  usec_calibration_count = count * 256;

  /* Enable timer interrupts. */
  InterruptSourceEnable(VIC_Source_TC1OI);
  InterruptSourceEnable(VIC_Source_TC3OI);
}

/* Delay for w microseconds. */
void
mach_SpinWaitUs(uint32_t w)
{
  /* On a 200 MHz processor, the following multiplication will not overflow
     if w < 42863. */
  w *= usec_calibration_count;
  w /= 501;	// microseconds per timer3 tick

  for ( ; w > 0; w--) {
  }
}
