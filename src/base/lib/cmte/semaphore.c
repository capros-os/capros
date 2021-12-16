/*
 * Copyright (C) 2007, 2008, Strawberry Development Group.
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
 * Foundation, 59 Temple Place - Suite 330 Boston, MA 02111-1307, USA.
 */
/* This material is based upon work supported by the US Defense Advanced
Research Projects Agency under Contract No. W31P4Q-07-C-0070.
Approved for public release, distribution unlimited. */

#include <domain/cmtesync.h>
#include <domain/CMTESemaphore.h>
#include <eros/machine/atomic.h>
#include <idl/capros/LSync.h>

void
CMTESemaphore_init(CMTESemaphore * sem, int val)
{
  capros_atomic32_set(&sem->count, val);
  sem->wakeupsWaiting = 0;
  link_Init(&sem->task_list);
}

void
CMTESemaphore_down(CMTESemaphore * sem)
{
  // might_sleep();
  // Decrement the count:
  if ((int32_t) capros_atomic32_add_return(&sem->count, -1) < 0) {
    CMTEWaitQueue wq;
    wq.threadNum = lk_getCurrentThreadNum();
    capros_LSync_semaWait(KR_LSYNC, (capros_LSync_pointer)sem,
                          (capros_LSync_pointer)&wq);
  }
#ifdef CONFIG_DEBUG_SEMAPHORE
  sem->locker = lk_getCurrentThreadNum();	// remember we have it
#endif
}

// Returns true iff successful.
int
CMTESemaphore_tryDown(CMTESemaphore * sem)
{
  int32_t newcnt = capros_atomic32_read(&sem->count);
  int32_t cnt;
  do {
    if (newcnt <= 0) return false;	// too bad
    cnt = newcnt;
    /* If the count is still cnt, decrement it. */
    newcnt = capros_atomic32_cmpxchg(&sem->count, cnt, cnt - 1);
  } while (newcnt != cnt);
  return true;	// success
}

// Private interface for sync process.
bool
CMTESemaphore_tryUp(CMTESemaphore * sem)
{
  return (int32_t) capros_atomic32_add_return(&sem->count, 1) <= 0;
}

void
CMTESemaphore_up(CMTESemaphore * sem)
{
  if (CMTESemaphore_tryUp(sem)) {
    capros_LSync_semaWakeup(KR_LSYNC, (capros_LSync_pointer)sem);
  }
}
