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

#include <domain/CMTERWSemaphore.h>
#include <idl/capros/LSync.h>
#include <domain/cmtesync.h>

/*
 * trylock for reading -- returns true if successful, false if contention
 */
bool
CMTERWSemaphore_tryDownRead(CMTERWSemaphore * sem)
{
  int32_t newcnt = capros_atomic32_read(&sem->activity);
  int cnt;
  do {
    if (newcnt < 0) return false;	// too bad, there is a writer
    cnt = newcnt;
    /* If the activity is still cnt, increment it. */
    newcnt = capros_atomic32_cmpxchg(&sem->activity, cnt, cnt + 1);
  } while (newcnt != cnt);
  return true;	// success
}

/*
 * lock for reading
 */
void
CMTERWSemaphore_downRead(CMTERWSemaphore * sem)
{
  // If there is no contention, get it quickly:
  if (CMTERWSemaphore_tryDownRead(sem)) return;

  // Too bad, need to go to sync thread.

  // First notify others that we are there.
  capros_atomic32_add_return(&sem->contenders, 1);

  CMTEWaitQueue wq;
  wq.threadNum = lk_getCurrentThreadNum();
  capros_LSync_rwsemWait(KR_LSYNC, (capros_LSync_pointer)sem, false,
                         (capros_LSync_pointer)&wq);
}

/*
 * trylock for writing -- returns true if successful, false if contention
 */
bool
CMTERWSemaphore_tryDownWrite(CMTERWSemaphore * sem)
{
  int32_t newcnt = capros_atomic32_read(&sem->activity);
  int cnt;
  do {
    if (newcnt != 0) return false;	// there are reader(s) or a writer
    cnt = newcnt;
    /* If the activity is still 0, set it to -1. */
    newcnt = capros_atomic32_cmpxchg(&sem->activity, cnt, -1);
  } while (newcnt != cnt);
  return true;	// success
}

/*
 * lock for writing

This implementation has a minor bug. The following is possible:
- reader1 locks
- writer attempts to lock, calls sync thread, waits
- reader1 unlocks
- reader2 locks
- reader1 calls sync thread to check contending writer

Writers are supposed to have priority, so technically reader2 shouldn't 
have gotten the lock. 

We could reduce, but not eliminate, this window by checking for contenders
in CMTERWSemaphore_downRead().
 */
void
CMTERWSemaphore_downWrite(CMTERWSemaphore * sem)
{
  // If there is no contention, get it quickly:
  if (CMTERWSemaphore_tryDownWrite(sem)) return;

  // Too bad, need to go to sync thread.

  // First notify others that we are there.
  capros_atomic32_add_return(&sem->contenders, 1);

  CMTEWaitQueue wq;
  wq.threadNum = lk_getCurrentThreadNum();
  capros_LSync_rwsemWait(KR_LSYNC, (capros_LSync_pointer)sem, true,
                         (capros_LSync_pointer)&wq);
}

/*
 * release a read lock
 */
void
CMTERWSemaphore_upRead(CMTERWSemaphore * sem)
{
  capros_atomic32_add_return(&sem->activity, -1);

  if (capros_atomic32_read(&sem->contenders) > 0)
    capros_LSync_rwsemWakeup(KR_LSYNC, (capros_LSync_pointer)sem);
}

/*
 * release a write lock
 */
void
CMTERWSemaphore_upWrite(CMTERWSemaphore * sem)
{
  capros_atomic32_add_return(&sem->activity, 1);

  if (capros_atomic32_read(&sem->contenders) > 0)
    capros_LSync_rwsemWakeup(KR_LSYNC, (capros_LSync_pointer)sem);
}

/*
 * downgrade write lock to read lock
 */
void
CMTERWSemaphore_downgradeWrite(CMTERWSemaphore * sem)
{
  capros_atomic32_add_return(&sem->activity, 2);	// change -1 to +1

  if (capros_atomic32_read(&sem->contenders) > 0)
    capros_LSync_rwsemWakeup(KR_LSYNC, (capros_LSync_pointer)sem);
}
