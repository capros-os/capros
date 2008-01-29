#ifndef __INTERRUPT_H__
#define __INTERRUPT_H__
/*
 * Copyright (C) 2006, 2008, Strawberry Development Group.
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

#include <kerninc/StallQueue.h>

/* We record here the static assignments of vectored interrupts. 

 VIC1:
  15: TC1OI Timer 1

 VIC2:
  0: TC3OI Timer 3

 */

#define NUM_INTERRUPT_SOURCES 64

struct VICIntSource;

typedef void (*ISRType)(struct VICIntSource *);
	/* pointer to function returning void */

/* Stuff for the EP93xx Vectored Interrupt Controller (VIC) */
typedef struct VICIntSource {
  /* ISRAddr must be the first item, to match struct VICNonVect. */
  ISRType ISRAddr;

  /* priority is:
     -2 if unallocated
     -1 if assigned to FIQ
      0 if assigned to vectored interrupt 0 (relative to its VIC)
      ...
      15 if assigned to vectored interrupt 15 (relative to its VIC)
      16 if nonvectored IRQ */
#define PRIO_Unallocated (-2)
  signed char priority;

  unsigned char sourceNum;

  /* Enabled state and software interrupt state are stored only in
     the corresponding VIC register. */

  // The following are used if the source is allocated to a user-mode handler.

  // The queue where the interrupt waiter sleeps.
  // There can be only one waiter,
  // because one wakeup consumes the isPending flag.
  /* This queue must be manipulated only with IRQ disabled,
  because an interrupt can call sq_WakeAll(sleeper). */
  StallQueue sleeper;
  bool isPending;
} VICIntSource;

static inline bool
vis_IsAlloc(VICIntSource * vis)
{
  return vis->priority != PRIO_Unallocated;
}

extern VICIntSource VICIntSources[NUM_INTERRUPT_SOURCES];

struct VICInfo;

struct VICNonVect {
  /* ISRAddr must be the first item. */
  ISRType ISRAddr;
  struct VICInfo * vicInfo;
};
	
void InterruptSourceSetup(unsigned int source, int priority, ISRType handler);
void InterruptSourceUnset(unsigned int source);
void InterruptSourceEnable(unsigned int source);
void InterruptSourceDisable(unsigned int source);
void InterruptSourceSoftwareIntGen(unsigned int source);
void InterruptSourceSoftwareIntClear(unsigned int source);
void DoUsermodeInterrupt(VICIntSource * vis);
#endif /* __INTERRUPT_H__ */
