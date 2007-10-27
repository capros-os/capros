/*
 * Copyright (C) 2007, Strawberry Development Group.
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

#include <linuxk/linux-emul.h>
#include <linux/rwsem.h>
#include <idl/capros/LSync.h>
#include <linuxk/lsync.h>

/*
 * lock for reading
 */
void down_read(struct rw_semaphore * sem)
{
  // If there is no contention, get it quickly:
  if (__rwtrylock_read(sem)) return;

  // Too bad, need to go to sync thread.

  // First notify others that we are there.
  atomic_inc(&sem->contenders);

  wait_queue_t wq;
  wq.threadNum = lk_getCurrentThreadNum();
  capros_LSync_rwsemWait(KR_LSYNC, (capros_LSync_pointer)sem, false,
                         (capros_LSync_pointer)&wq);
}

/*
 * trylock for reading -- returns 1 if successful, 0 if contention
 */
int down_read_trylock(struct rw_semaphore * sem)
{
  return __rwtrylock_read(sem);
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
in down_read().
 */
void down_write(struct rw_semaphore * sem)
{
  // If there is no contention, get it quickly:
  if (__rwtrylock_write(sem)) return;

  // Too bad, need to go to sync thread.

  // First notify others that we are there.
  atomic_inc(&sem->contenders);

  wait_queue_t wq;
  wq.threadNum = lk_getCurrentThreadNum();
  capros_LSync_rwsemWait(KR_LSYNC, (capros_LSync_pointer)sem, true,
                         (capros_LSync_pointer)&wq);
}

/*
 * trylock for writing -- returns 1 if successful, 0 if contention
 */
int down_write_trylock(struct rw_semaphore * sem)
{
  return __rwtrylock_write(sem);
}

/*
 * release a read lock
 */
void up_read(struct rw_semaphore * sem)
{
  atomic_dec(&sem->activity);

  if (atomic_read(&sem->contenders) > 0)
    capros_LSync_rwsemWakeup(KR_LSYNC, (capros_LSync_pointer)sem);
}

/*
 * release a write lock
 */
void up_write(struct rw_semaphore * sem)
{
  atomic_inc(&sem->activity);

  if (atomic_read(&sem->contenders) > 0)
    capros_LSync_rwsemWakeup(KR_LSYNC, (capros_LSync_pointer)sem);
}

/*
 * downgrade write lock to read lock
 */
void downgrade_write(struct rw_semaphore * sem)
{
  atomic_add(2, &sem->activity);	// change -1 to +1

  if (atomic_read(&sem->contenders) > 0)
    capros_LSync_rwsemWakeup(KR_LSYNC, (capros_LSync_pointer)sem);
}

#ifdef CONFIG_DEBUG_LOCK_ALLOC
/* unimplemented */
#else
# define down_read_nested(sem, subclass)		down_read(sem)
# define down_write_nested(sem, subclass)	down_write(sem)
# define down_read_non_owner(sem)		down_read(sem)
# define up_read_non_owner(sem)			up_read(sem)
#endif

