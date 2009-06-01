/*
 * Copyright (C) 1998, 1999, Jonathan S. Shapiro.
 * Copyright (C) 2006, 2007, 2009, Strawberry Development Group.
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
Research Projects Agency under Contract Nos. W31P4Q-06-C-0040 and
W31P4Q-07-C-0070.  Approved for public release, distribution unlimited. */

#include <kerninc/kernel.h>
#include <kerninc/Activity.h>
#include <arch-kerninc/IRQ-inline.h>
#include "arm.h"

//#define IdleTimeTest

#define StackSize 256

#ifdef IdleTimeTest // variables to measure idle time
#include <kerninc/mach-rtc.h>
#define IdleSlotTime 2	// seconds
#define NumIdleTimeSlots 60
// IdleTimeArray entries hold the number of idle iterations each IdleSlotTime.
uint32_t IdleTimeArray[NumIdleTimeSlots];
uint32_t * IdleTimeCursor = &IdleTimeArray[0];
static uint32_t rtc;
#endif

void
IdleActivity_Start(void)
{
  int stack;

  printf("Start IdleActivity (activity 0x%x,proc 0x%x,stack 0x%x)\n",
	 act_Current(), proc_Current(), &stack);

  // For some reason, cannot call Debugger here; this process gets wedged,
  // which is a Bad Thing.

  for(;;) {
#ifdef IdleTimeTest // code to measure idle time

    uint32_t expiration = rtc + IdleSlotTime;
    while ((rtc = RtcRead()) < expiration)
      (* IdleTimeCursor) ++;
    if (++IdleTimeCursor >= &IdleTimeArray[NumIdleTimeSlots])
      IdleTimeCursor = &IdleTimeArray[0];	// wrap
  
#else

#ifndef NDEBUG
    extern void CheckExceptionHandlerStacks(void);
    CheckExceptionHandlerStacks();
#endif

#if 1
    // Show idle activity
    volatile int x;
    int i;
    for (i=0; i<10000000; i++)
      (void)x;
    printf("i\b");
    for (i=0; i<10000000; i++)
      (void)x;
    printf("I\b");
#else
    __asm__ ("mcr p15,0,r0,c7,c0,4");	// wait for interrupt
#endif
#endif
  }
}

Activity *
StartIdleActivity(void)
{
  fixreg_t *stack = MALLOC(fixreg_t, StackSize);

  Activity *idleActivity = 
    kact_InitKernActivity("Idler", pr_Idle, dispatchQueues[pr_Idle], 
			&IdleActivity_Start, stack, &stack[StackSize]);

  act_Wakeup(idleActivity);	/* let it initialize itself... */

  printf("Initialized IdleActivity (activity %#x, process %#x)\n",
	 idleActivity, act_GetProcess(idleActivity));

  return idleActivity;
}
