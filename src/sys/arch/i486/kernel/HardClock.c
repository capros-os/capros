/*
 * Copyright (C) 1998, 1999, Jonathan S. Shapiro.
 * Copyright (C) 2005, 2006, 2007, 2008, Strawberry Development Group.
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
#include <kerninc/Machine.h>
#include <kerninc/Activity.h>
#include <kerninc/SysTimer.h>
#include <kerninc/IRQ.h>
#include <eros/arch/i486/io.h>
#include "IDT.h"
#include "IRQ386.h"
#include <kerninc/CPU.h>

volatile uint64_t sysT_now = 0llu;
volatile uint64_t sysT_wakeup = ~(0llu);

/* The timer chip on the PC has three channels. There is a fourth on
 * the PS/2, but for the moment I am ignoring that platform.
 * 
 * These channels are hard-wired to the following purposes on PC
 * compatible motherboards:
 * 
 *    Channel 0		Generates IRQ0, aka system clock
 *    Channel 1		Memory refresh
 *    Channel 3		Routed to internal speaker
 * 
 * Needless to say, dorking with channel 1 is, um, unhealthy, and I do
 * not plan to mess with channel 3 until I understand the speaker better.
 * Fortunately, the BIOS can be more or less trusted to establish
 * reasonable initial defaults for each of these channels.
 * 
 * The timer chip is driven by a clock rate of 1.193182 MHz; the
 * motherboard hardware is responsible to see that this happens.
 * Ordinarily, channel 0 is programmed to further divide this by
 * 65536, giving the PC timer interrupt speed of 18.2 cycles per
 * second.
 * 
 * For EROS, the desired timer granularity is milliseconds.  Unfortunately
 * this is very much too fast for the x86 hardware.
 */

/* The hardware clock is tricky - we configure a hardware clock, and
 * give sysT_Wakeup() as the handler function.
 * There is a fast path interrupt filter such that the only interrupts
 * that actually make it to the interrupt dispatch mechanism are the
 * ones where a wake up is really required (wakeup > now).
 */

/* Speed of the actual base tick rate, in ticks/sec */
#ifdef OPTION_FAST_CLOCK
#define BASETICK 2000 /* 4000 possible on pentium only */
#else
#define BASETICK 200
#endif

// The frequency of the input to the 8253 timer in Hz:
#define HARD_TICK_RATE		1193182L

/* SOFT_TICK_DIVIDER is the divider to use to get the soft tick rate down
 * as close as possible to the value specified by BASETICK. The soft tick
 * rate MUST be >= BASETICK.
 * 
 * Given a value for SOFT_TICK_DIVIDER, the number of ticks per second
 * is given by HARD_TICK_RATE/SOFT_TICK_DIVIDER, and the number of ticks
 * per millisecond is (HARD_TICK_RATE/SOFT_TICK_DIVIDER) * (1/1000).
 * Here's where it all starts to become interesting.  
 * 
 * We need to convert milliseconds to ticks using integer arithmetic. 
 * What we're going to do is take the milliseconds number handed us,
 * multiply that by 2^20 * (ticks/ms), and then divide the whole mess
 * by 2^20 giving ticks.  The end result will be to correct for tick
 * drift.
 * 
 * TICK_MULTIPLIER, then, is the result of computing (by hand):
 * 	2^20 * (HARD_TICK_RATE/(SOFT_TICK_DIVIDER*1000))
 * 
 * Note: if you use UNIX bc, compute these with scale=8
 * 
 * The other conversion we need to be able to do is to go from ticks
 * since last reboot to milliseconds.  This is basically the same
 * calculation done in the opposite direction.  The goal is not to be
 * off by more than 1ms per 1024*1024 seconds.
 * 
 * TICK_TO_MS_MULTIPLIER, then, is the result of computing (by hand):
 *      2^20 * ((1000 * 2^20) / (SOFT_TICK_RATE * 2^20))
 *  where
 *    SOFT_TICK_RATE = HARD_TICK_RATE / SOFT_TICK_DIVIDER
 */

#if (BASETICK == 4000)
# define SOFT_TICK_DIVIDER	298
# define TICK_MULTIPLIER	4198463ll
# define TICK_TO_MS_MULTIPLIER  261884ll
#define TICK_TO_NS_MULTIPLIER	249752ll
#elif (BASETICK == 2000)
# define SOFT_TICK_DIVIDER	596
# define TICK_MULTIPLIER	2099231ll
# define TICK_TO_MS_MULTIPLIER  523768ll
#define TICK_TO_NS_MULTIPLIER	499504ll
#elif (BASETICK == 1000)
# define SOFT_TICK_DIVIDER	1193
# define TICK_MULTIPLIER	1048736ll
# define TICK_TO_MS_MULTIPLIER  1048416ll
#define TICK_TO_NS_MULTIPLIER	999847ll
#elif (BASETICK == 500)
# define SOFT_TICK_DIVIDER	2386
# define TICK_MULTIPLIER	524368ll
# define TICK_TO_MS_MULTIPLIER  2096832ll
#define TICK_TO_NS_MULTIPLIER	1999695ll
#elif (BASETICK == 200)
# define SOFT_TICK_DIVIDER	5965
# define TICK_MULTIPLIER	209747ll
# define TICK_TO_MS_MULTIPLIER  5242080ll
#define TICK_TO_NS_MULTIPLIER	4999237ll
#elif (BASETICK == 100)
# define SOFT_TICK_DIVIDER	11931
# define TICK_MULTIPLIER	104865ll
# define TICK_TO_MS_MULTIPLIER  10485039ll
#define TICK_TO_NS_MULTIPLIER	9999312ll
#elif (BASETICK == 60)
# define SOFT_TICK_DIVIDER	19886
# define TICK_MULTIPLIER	62912ll
# define TICK_TO_MS_MULTIPLIER  17475944ll
#define TICK_TO_NS_MULTIPLIER	16666359ll
#elif (BASETICK == 50)
# define SOFT_TICK_DIVIDER	23863
# define TICK_MULTIPLIER	52430ll
# define TICK_TO_MS_MULTIPLIER  20991457ll
#define TICK_TO_NS_MULTIPLIER	20019013ll
#else
# error "BASETICK not properly defined"
#endif

#define TIMER_PORT_0		0x40
#define TIMER_MODE		0x43
// 8253 control word:
#define SQUARE_WAVE0            0x34

uint64_t
mach_MillisecondsToTicks(uint64_t ms)
{
  uint64_t ticks;
  ticks = ms;
  ticks *= TICK_MULTIPLIER;
  ticks >>= 20;			/* divide by (1024*1024) */

  /* It's okay to sleep for zero ticks, since we will sleep for at
   * least one tick interval before being woken up.
   */
  
  return ticks;
}

/* Pragmatically, this only works if the machine has been up for less
 * than 136 years since the last reboot.
 */
uint64_t
mach_TicksToMilliseconds(uint64_t ticks)
{
  uint64_t ms = ticks;
  ms *= TICK_TO_MS_MULTIPLIER;
  ms >>= 20;			/* divide by (1024*1024) */

  return ms;
}

uint64_t
mach_NanosecondsToTicks(uint64_t ns)
{
  return ns / TICK_TO_NS_MULTIPLIER;
}

uint64_t
mach_TicksToNanoseconds(uint64_t ticks)
{
  return ticks * TICK_TO_NS_MULTIPLIER;
}

void
sysT_Wakeup(savearea_t *sa)
{
  /* Processing wakeups on the SleepQueue requires completing the
  sleeping processes' invocations. 
  If we did that work now, all the variables involved would have to be
  protected with irq disable.
  To avoid that, we just set the flag timerWork here,
  and do the wakeups in ExitTheKernel (a "software interrupt).
  Also set act_yieldState so timerWork will be noticed. */

  /* Nothing to wait for until that work is done. */
  sysT_wakeup = UINT64_MAX;
  timerWork = true;
  act_yieldState = true;

  irq_Enable(IRQ_FROM_EXCEPTION(sa->ExceptNo));
}

void
sysT_ResetWakeTime(void)
{
  if (timerWork)
    sysT_wakeup = UINT64_MAX;
  else
    sysT_wakeup = sysT_WakeupTime();
}

void
mach_InitHardClock()
{
  uint64_t calibrateDone;

  /* Set up the hardware clock: */
  outb(SQUARE_WAVE0, TIMER_MODE);
  outb(SOFT_TICK_DIVIDER & 0xff, TIMER_PORT_0);
  outb((SOFT_TICK_DIVIDER >> 8) & 0xff, TIMER_PORT_0);
    
  irq_SetHandler(irq_HardClock, sysT_Wakeup);
  irq_Enable(irq_HardClock);

  /* Above should have taken an interrupt instantaneously, since the
   * clock has by this point been disabled a while.  Calibrate the
   * microsecond spin multiplier.
   */

  printf("Calibrating SpinWait... ");
  const uint64_t ticksToMeasure = 5;
  const int loopsPerCall = 2048;

  uint32_t count = 0;
  calibrateDone = sysT_Now() + ticksToMeasure;
  while (sysT_Now() < calibrateDone) {
    count ++;
    mach_Delay(loopsPerCall);
  }
  // count * loopsPerCall is approximate # of loops per tick.

  // Now get a more accurate measurement by reducing the number of
  // iterations checking sysT_Now and calling mach_Delay.
  calibrateDone = sysT_Now() + ticksToMeasure;
  if (count > 0)
    mach_Delay((count - 1)*loopsPerCall);	// majority of waiting is here
  unsigned int count2 = 0;
  while (sysT_Now() < calibrateDone) {
    count2 ++;
    mach_Delay(loopsPerCall);
  }

  printf("%d, %d, ", count, count2);

  count += count2 - 1;		// counts per test
  assert(count < (1UL << (32 - 11)));	// else the following will overflow
  count *= loopsPerCall;	// loops per test
  count *= 8;		// loops per 8 tests
  count /= (mach_TicksToNanoseconds(ticksToMeasure) / 1000);
	// loops per 8 microseconds
  assert(count < (1UL << (32 - 15)));	// else could overflow in SpinWaitUs
  loopsPer8us = count;
  printf("%d\n", loopsPer8us);

#if 0
  // Test it:
  for (count = 0; count < 61; count++)
    SpinWaitUs(32767);
  printf("End of 2 second delay.\n");
#endif
}
