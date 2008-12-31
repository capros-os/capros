#ifndef __CMTESEMAPHORE_H
#define __CMTESEMAPHORE_H
/*
 * Copyright (C) 2008, Strawberry Development Group.
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

#include <eros/machine/atomic.h>
#include <eros/Link.h>

typedef struct CMTESemaphore {
  capros_atomic32 count;
  int wakeupsWaiting;
  // task_list must be accessed only by the sync process.
  Link task_list;
#ifdef CONFIG_DEBUG_SEMAPHORE
  unsigned int locker;	// thread that last did a successful "down"
#endif
} CMTESemaphore;

#define CMTESemaphore_Initializer(name, cnt) \
  { .count = capros_atomic32_Initializer(cnt), \
    .wakeupsWaiting = 0, \
    .task_list = { &(name).task_list, &(name).task_list } \
  }

#define CMTESemaphore_DECLARE(name, cnt) \
  CMTESemaphore name = CMTESemaphore_Initializer(name, cnt)

void CMTESemaphore_init(CMTESemaphore * sem, int val);
void CMTESemaphore_down(CMTESemaphore * sem);
int CMTESemaphore_tryDown(CMTESemaphore * sem);
void CMTESemaphore_up(CMTESemaphore * sem);


/* A mutex is just a binary semaphore. */

#define CMTEMutex_DECLARE_Unlocked(name) \
  CMTESemaphore_DECLARE(name, 1)

#define CMTEMutex_DECLARE_Locked(name) \
  CMTESemaphore_DECLARE(name, 0)

INLINE void
CMTEMutex_lock(CMTESemaphore * sem)
{
  CMTESemaphore_down(sem);
}

INLINE void
CMTEMutex_unlock(CMTESemaphore * sem)
{
  CMTESemaphore_up(sem);
}

#endif // __CMTESEMAPHORE_H
