/*
 * Copyright (C) 1998, 1999, Jonathan S. Shapiro.
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
#include <kerninc/SysTimer.h>
#include <eros/i486/io.h>
#include "IDT.h"

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

/* The hardware clock is tricky - we configure a hardware clock, but
 * give SysTimer::Wakeup() as the handler function.  We're going to
 * build a fast path interrupt filter such that the only interrupts
 * that actually make it to the interrupt dispatch mechanism are the
 * ones where a wake up is really required.
 */

/* Speed of the actual base tick rate, in ticks/sec */
#ifdef OPTION_FAST_CLOCK
#define BASETICK 2000 /* 4000 possible on pentium only */
#else
#define BASETICK 200
#endif

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
 * One year is longer than we are going to go without a reboot (random
 * alpha hits, if nothing else), so we set that as an upper bound on the
 * sleep call.  One year is just less than 2^35 milliseconds, and 64 bit
 * integer arithmetic is the largest convenient kind on this machine.
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
#elif (BASETICK == 2000)
# define SOFT_TICK_DIVIDER	596
# define TICK_MULTIPLIER	2099231ll
# define TICK_TO_MS_MULTIPLIER  523768ll
#elif (BASETICK == 1000)
# define SOFT_TICK_DIVIDER	1193
# define TICK_MULTIPLIER	1048736ll
# define TICK_TO_MS_MULTIPLIER  1048416ll
#elif (BASETICK == 500)
# define SOFT_TICK_DIVIDER	2386
# define TICK_MULTIPLIER	524368ll
# define TICK_TO_MS_MULTIPLIER  2096832ll
#elif (BASETICK == 200)
# define SOFT_TICK_DIVIDER	5965
# define TICK_MULTIPLIER	209747ll
# define TICK_TO_MS_MULTIPLIER  5242080ll
#elif (BASETICK == 100)
# define SOFT_TICK_DIVIDER	11931
# define TICK_MULTIPLIER	104865ll
# define TICK_TO_MS_MULTIPLIER  10485039ll
#elif (BASETICK == 60)
# define SOFT_TICK_DIVIDER	19886
# define TICK_MULTIPLIER	62912ll
# define TICK_TO_MS_MULTIPLIER  17475944ll
#elif (BASETICK == 50)
# define SOFT_TICK_DIVIDER	23863
# define TICK_MULTIPLIER	52430ll
# define TICK_TO_MS_MULTIPLIER  20991457ll
#else
# error "BASETICK not properly defined"
#endif

#define TIMER_PORT_0		0x40
#define TIMER_MODE		0x43
#define SQUARE_WAVE0            0x34

static uint32_t usec_calibration_count = 0;

uint64_t
mach_MillisecondsToTicks(uint64_t ms)
{
  uint64_t ticks;
  if ((ms >> 35) > 0)
    ms = 1ll << 35;	/* don't sleep more than a year */

  ticks = ms;
  ticks *= TICK_MULTIPLIER;
  ticks >>= 20;			/* divide by (1024*1024) */

  /* It's okay to sleep for zero ticks, since we will sleep for at
   * least one tick interval before being woken up.
   */
  
  return (uint32_t) ticks;
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

  return (uint32_t) ms;
}

#ifdef GNU_INLINE_ASM
void
mach_InitHardClock()
{
  uint64_t ticks;
  uint32_t i;
  uint64_t calibrateDone;

  /* Set up the hardware clock: */
  old_outb(TIMER_MODE, SQUARE_WAVE0);
  old_outb(TIMER_PORT_0, SOFT_TICK_DIVIDER & 0xff);
  old_outb(TIMER_PORT_0, (SOFT_TICK_DIVIDER >> 8) & 0xff);
    
  irq_SetHandler(irq_HardClock, sysT_Wakeup);
  irq_Enable(irq_HardClock);

  /* Above should have taken an interrupt instantaneously, since the
   * clock has by this point been disabled a while.  Calibrate the
   * microsecond spin multiplier.
   */

  printf("Calibrating SpinWait... ");
  ticks = mach_MillisecondsToTicks(11);
 
  calibrateDone = sysT_Now() + ticks;

  usec_calibration_count = 0;
  while (sysT_Now() < calibrateDone) {
    usec_calibration_count ++;
    
    for (i = 0; i < 200; i++) {
      GNU_INLINE_ASM ("nop");
    }
  }
 
  
  printf("done\n");
}
#endif /* GNU_INLINE_ASM */

#ifdef GNU_INLINE_ASM
void
mach_SpinWaitUs(uint32_t w)
{
  uint32_t count;
  uint32_t i;
  w *= usec_calibration_count;
  w /= 10;			/* calibrated for 11 milliseconds, but */
				/* divide by something less than that! */

  if (w < 2) w = 2;		/* MIN sleep of 2 usec on 200Mhz */

  for (count = 0; count < w; count++) {
    for (i = 0; i < 200; i++) {
      GNU_INLINE_ASM ("nop");
    }
  }
}
#endif /* GNU_INLINE_ASM */
