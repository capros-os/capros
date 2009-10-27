#ifndef __CMTERWSEMAPHORE_H
#define __CMTERWSEMAPHORE_H
/*
 * Copyright (C) 2008, Strawberry Development Group.
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
 * Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */
/* This material is based upon work supported by the US Defense Advanced
Research Projects Agency under Contract No. W31P4Q-07-C-0070.
Approved for public release, distribution unlimited. */

#include <eros/machine/atomic.h>
#include <eros/Link.h>

typedef struct CMTERWSemaphore {
  /* If activity is -1 then there is one active writer.
     If activity is 0 then there are no active readers or writers.
     If activity is > 0 then that is the number of active readers. */
  capros_atomic32 activity;

  capros_atomic32 contenders;	// number of threads waiting

  // readWaiters must be accessed only by the sync process.
  Link readWaiters;
  // writeWaiters must be accessed only by the sync process.
  Link writeWaiters;
#ifdef CONFIG_DEBUG_SEMAPHORE
  unsigned int locker;	// thread that last did a successful "down"
#endif
} CMTERWSemaphore;

#ifdef CONFIG_DEBUG_SEMAPHORE
#define CMTERWSemaphore_DebugInitializer .locker = 0,
#else
#define CMTERWSemaphore_DebugInitializer
#endif

#define CMTERWSemaphore_Initializer(name) \
  { .activity = capros_atomic32_Initializer(0), \
    .contenders = capros_atomic32_Initializer(0), \
    .readWaiters = { &(name).readWaiters, &(name).readWaiters }, \
    .writeWaiters = { &(name).writeWaiters, &(name).writeWaiters }, \
    CMTERWSemaphore_DebugInitializer \
  }

#define CMTERWSemaphore_DECLARE(name) \
  CMTESemaphore name = CMTERWSemaphore_Initializer(name)

static inline void
CMTERWSemaphore_init(CMTERWSemaphore * sem)
{
  capros_atomic32_set(&sem->activity, 0);
  capros_atomic32_set(&sem->contenders, 0);
  link_Init(&sem->readWaiters);
  link_Init(&sem->writeWaiters);
#ifdef CONFIG_DEBUG_SEMAPHORE
  sem->locker = 0;
#endif
}

void CMTERWSemaphore_downRead(CMTERWSemaphore * sem);
bool CMTERWSemaphore_tryDownRead(CMTERWSemaphore * sem);
void CMTERWSemaphore_upRead(CMTERWSemaphore * sem);
void CMTERWSemaphore_downWrite(CMTERWSemaphore * sem);
bool CMTERWSemaphore_tryDownWrite(CMTERWSemaphore * sem);
void CMTERWSemaphore_upWrite(CMTERWSemaphore * sem);
void CMTERWSemaphore_downgradeWrite(CMTERWSemaphore * sem);

static inline bool
CMTERWSemaphore_isLocked(CMTERWSemaphore * sem)
{
  return capros_atomic32_read(&sem->activity) != 0;
}

#endif // __CMTERWSEMAPHORE_H
