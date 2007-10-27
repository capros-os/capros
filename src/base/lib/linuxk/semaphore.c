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
#include <asm-generic/semaphore.h>
#include <idl/capros/LSync.h>
#include <linuxk/lsync.h>


void sema_init(struct semaphore *sem, int val)
{
  atomic_set(&sem->count, val);
  sem->wakeupsWaiting = 0;
  init_waitqueue_head(&sem->wait);
}

void down_slowpath(struct semaphore * sem)
{
  wait_queue_t wq;
  wq.threadNum = lk_getCurrentThreadNum();
  capros_LSync_semaWait(KR_LSYNC, (capros_LSync_pointer)sem,
                        (capros_LSync_pointer)&wq);
}

int
down_trylock(struct semaphore * sem)
{
  int newcnt = atomic_read(&sem->count);
  int cnt;
  do {
    if (newcnt <= 0) return 1;	// too bad
    cnt = newcnt;
    /* If the count is still cnt, decrement it. */
    newcnt = atomic_cmpxchg(&sem->count, cnt, cnt - 1);
  } while (newcnt != cnt);
  return 0;	// success
}
