#ifndef __ASM_GENERIC_SEMAPHORE_H
#define __ASM_GENERIC_SEMAPHORE_H
/*
 * Copyright (C) 2007, Strawberry Development Group.
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

/*
 * linux/include/asm-generic/semaphore.h
 */

#include <linux/kernel.h>
#include <linux/wait.h>
#include <asm/atomic.h>
#include <idl/capros/LSync.h>
#include <linuxk/lsync.h>

struct semaphore {
  atomic_t count;
  int wakeupsWaiting;
  wait_queue_head_t wait;
};

#define __SEMAPHORE_INIT(name, cnt)				\
{								\
	.count	= ATOMIC_INIT(cnt),				\
	.wakeupsWaiting = 0,					\
	.wait	= __WAIT_QUEUE_HEAD_INITIALIZER((name).wait)	\
}

#define __DECLARE_SEMAPHORE_GENERIC(name,count)	\
	struct semaphore name = __SEMAPHORE_INIT(name,count)

#define DECLARE_MUTEX(name)		__DECLARE_SEMAPHORE_GENERIC(name,1)
#define DECLARE_MUTEX_LOCKED(name)	__DECLARE_SEMAPHORE_GENERIC(name,0)

void sema_init(struct semaphore *sem, int val);

static inline void init_MUTEX(struct semaphore *sem)
{
  sema_init(sem, 1);
}

static inline void init_MUTEX_LOCKED(struct semaphore *sem)
{
  sema_init(sem, 0);
}


void down_slowpath(struct semaphore * sem);
static inline void down(struct semaphore * sem)
{
  might_sleep();
  if (atomic_dec_return(&sem->count) < 0) {
    down_slowpath(sem);
  }
}

static inline int down_interruptible (struct semaphore * sem)
{
  down(sem);
  return 0;  // there are no signals in CapROS
}

int down_trylock(struct semaphore * sem);

/*
 * Note! This is subtle. We jump to wake people up only if
 * the semaphore was negative (== somebody was waiting on it).
 * The default case (no contention) will result in NO
 * jumps for both down() and up().
 */
static inline void up(struct semaphore * sem)
{
  if (atomic_inc_return(&sem->count) <= 0) {
    capros_LSync_semaWakeup(KR_LSYNC, (capros_LSync_pointer)sem);
  }
}

#endif // __ASM_GENERIC_SEMAPHORE_H
