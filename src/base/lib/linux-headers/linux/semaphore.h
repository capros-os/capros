#ifndef __LINUX_SEMAPHORE_H
#define __LINUX_SEMAPHORE_H
/*
 * Copyright (c) 2008 Intel Corporation
 * Author: Matthew Wilcox <willy@linux.intel.com>
 * Copyright (C) 2007, 2008, 2009, Strawberry Development Group.
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

#include <linux/kernel.h>
#include <linux/list.h>
#include <asm/atomic.h>
#include <idl/capros/LSync.h>
#include <linuxk/lsync.h>
#include <domain/CMTESemaphore.h>

struct semaphore {
  CMTESemaphore csem;
};

#define __SEMAPHORE_INITIALIZER(name, n)				\
{ .csem = CMTESemaphore_Initializer((name).csem, n)			\
}

#define __DECLARE_SEMAPHORE_GENERIC(name,count)	\
	struct semaphore name = __SEMAPHORE_INITIALIZER(name,count)

#define DECLARE_MUTEX(name)		__DECLARE_SEMAPHORE_GENERIC(name,1)
#define DECLARE_MUTEX_LOCKED(name)	__DECLARE_SEMAPHORE_GENERIC(name,0)

static inline void sema_init(struct semaphore *sem, int val)
{
  CMTESemaphore_init(&sem->csem, val);
}

static inline void init_MUTEX(struct semaphore *sem)
{
  sema_init(sem, 1);
}

static inline void init_MUTEX_LOCKED(struct semaphore *sem)
{
  sema_init(sem, 0);
}

static inline void down(struct semaphore * sem)
{
  CMTESemaphore_down(&sem->csem);
}

static inline int __must_check down_interruptible(struct semaphore *sem)
{
  down(sem);
  return 0;  // there are no signals in CapROS
}

static inline int __must_check down_killable(struct semaphore *sem)
{
  down(sem);
  return 0;  // there are no signals in CapROS
}

static inline int __must_check down_trylock(struct semaphore *sem)
{
  return ! CMTESemaphore_tryDown(&sem->csem);
}

extern int __must_check down_timeout(struct semaphore *sem, long jiffies);

static inline void up(struct semaphore * sem)
{
  CMTESemaphore_up(&sem->csem);
}

#endif /* __LINUX_SEMAPHORE_H */
