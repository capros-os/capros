/* rwsem.h: R/W semaphores, public interface
 *
 * Written by David Howells (dhowells@redhat.com).
 * Derived from asm-i386/semaphore.h
 */
/*
 * Copyright (C) 2009, Strawberry Development Group.
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

#ifndef _LINUX_RWSEM_H
#define _LINUX_RWSEM_H

#include <linux/lockdep.h>	// not needed here, but others rely on it
#include <domain/CMTERWSemaphore.h>

struct rw_semaphore {
  CMTERWSemaphore cmterwsem;
};

#define __RWSEM_INITIALIZER(name) \
  { .cmterwsem = CMTERWSemaphore_Initializer(name.cmterwsem) }

#define DECLARE_RWSEM(name) \
	struct rw_semaphore name = __RWSEM_INITIALIZER(name)

#define init_rwsem(sem) CMTERWSemaphore_init(&sem->cmterwsem)

static inline int
rwsem_is_locked(struct rw_semaphore *sem)
{
  return CMTERWSemaphore_isLocked(&sem->cmterwsem) != 0;
}

/*
 * lock for reading
 */
static inline void
down_read(struct rw_semaphore *sem)
{
  CMTERWSemaphore_downRead(&sem->cmterwsem);
}

/*
 * trylock for reading -- returns 1 if successful, 0 if contention
 */
static inline int
down_read_trylock(struct rw_semaphore *sem)
{
  return CMTERWSemaphore_tryDownRead(&sem->cmterwsem);
}

/*
 * lock for writing
 */
static inline void
down_write(struct rw_semaphore *sem)
{
  CMTERWSemaphore_downWrite(&sem->cmterwsem);
}

/*
 * trylock for writing -- returns 1 if successful, 0 if contention
 */
static inline int
down_write_trylock(struct rw_semaphore *sem)
{
  return CMTERWSemaphore_tryDownWrite(&sem->cmterwsem);
}

/*
 * release a read lock
 */
static inline void
up_read(struct rw_semaphore *sem)
{
  CMTERWSemaphore_upRead(&sem->cmterwsem);
}

/*
 * release a write lock
 */
static inline void
up_write(struct rw_semaphore *sem)
{
  CMTERWSemaphore_upWrite(&sem->cmterwsem);
}

/*
 * downgrade write lock to read lock
 */
static inline void
downgrade_write(struct rw_semaphore *sem)
{
  CMTERWSemaphore_downgradeWrite(&sem->cmterwsem);
}

#ifdef CONFIG_DEBUG_LOCK_ALLOC
/*
 * nested locking. NOTE: rwsems are not allowed to recurse
 * (which occurs if the same task tries to acquire the same
 * lock instance multiple times), but multiple locks of the
 * same lock class might be taken, if the order of the locks
 * is always the same. This ordering rule can be expressed
 * to lockdep via the _nested() APIs, but enumerating the
 * subclasses that are used. (If the nesting relationship is
 * static then another method for expressing nested locking is
 * the explicit definition of lock class keys and the use of
 * lockdep_set_class() at lock initialization time.
 * See Documentation/lockdep-design.txt for more details.)
 */
extern void down_read_nested(struct rw_semaphore *sem, int subclass);
extern void down_write_nested(struct rw_semaphore *sem, int subclass);
/*
 * Take/release a lock when not the owner will release it.
 *
 * [ This API should be avoided as much as possible - the
 *   proper abstraction for this case is completions. ]
 */
extern void down_read_non_owner(struct rw_semaphore *sem);
extern void up_read_non_owner(struct rw_semaphore *sem);
#else
# define down_read_nested(sem, subclass)		down_read(sem)
# define down_write_nested(sem, subclass)	down_write(sem)
# define down_read_non_owner(sem)		down_read(sem)
# define up_read_non_owner(sem)			up_read(sem)
#endif

#endif /* _LINUX_RWSEM_H */
