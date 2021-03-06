/*
 * Mutexes: blocking mutual exclusion locks
 *
 * started by Ingo Molnar:
 *
 *  Copyright (C) 2004, 2005, 2006 Red Hat, Inc., Ingo Molnar <mingo@redhat.com>
 *  Portions Copyright (C) 2007, 2008, 2009, Strawberry Development Group.
 *
 * This file contains the main data structure and API definitions.
 */
#ifndef __LINUX_MUTEX_H
#define __LINUX_MUTEX_H

#include <linux/semaphore.h>

/*
 * Simple, straightforward mutexes with strict semantics:
 *
 * - only one task can hold the mutex at a time
 * - only the owner can unlock the mutex
 * - multiple unlocks are not permitted
 * - recursive locking is not permitted
 * - a mutex object must be initialized via the API
 * - a mutex object must not be initialized via memset or copying
 * - task may not exit with mutex held
 * - memory areas where held locks reside must not be freed
 * - held mutexes must not be reinitialized
 * - mutexes may not be used in hardware or software interrupt
 *   contexts such as tasklets and timers
 *
 * These semantics are fully enforced when DEBUG_MUTEXES is
 * enabled. Furthermore, besides enforcing the above rules, the mutex
 * debugging code also implements a number of additional features
 * that make lock debugging easier and faster:
 *
 * - uses symbolic names of mutexes, whenever they are printed in debug output
 * - point-of-acquire tracking, symbolic lookup of function names
 * - list of all locks held in the system, printout of them
 * - owner tracking
 * - detects self-recursing locks and prints out all relevant info
 * - detects multi-task circular deadlocks and prints out all affected
 *   locks and tasks (and only those tasks)
 */
/* A mutex does not need a wakeupsWaiting field, but for now,
 * just define it as a semaphore. */
struct mutex {
	struct semaphore sem;
#if defined(CONFIG_DEBUG_MUTEXES) || defined(CONFIG_SMP)
	struct thread_info	*owner;
#endif
#ifdef CONFIG_DEBUG_MUTEXES
	const char 		*name;
	void			*magic;
#endif
#ifdef CONFIG_DEBUG_LOCK_ALLOC
	struct lockdep_map	dep_map;
#endif
};

#ifdef CONFIG_DEBUG_MUTEXES
# include <linux/mutex-debug.h>
#else
# define __DEBUG_MUTEX_INITIALIZER(lockname)
# define mutex_init(mutex) sema_init(&(mutex)->sem, 1)
# define mutex_destroy(mutex)				do { } while (0)
#endif

#ifdef CONFIG_DEBUG_LOCK_ALLOC
# define __DEP_MAP_MUTEX_INITIALIZER(lockname) \
		, .dep_map = { .name = #lockname }
#else
# define __DEP_MAP_MUTEX_INITIALIZER(lockname)
#endif

#define __MUTEX_INITIALIZER(lockname) \
	{.sem = __SEMAPHORE_INITIALIZER((lockname).sem, 1)}

#define DEFINE_MUTEX(mutexname) \
	struct mutex mutexname = __MUTEX_INITIALIZER(mutexname)

/**
 * mutex_is_locked - is the mutex locked
 * @lock: the mutex to be queried
 *
 * Returns 1 if the mutex is locked, 0 if unlocked.
 */
static inline int mutex_is_locked(struct mutex *lock)
{
  return capros_atomic32_read(&lock->sem.csem.count) <= 0;
}

/*
 * See kernel/mutex.c for detailed documentation of these APIs.
 * Also see Documentation/mutex-design.txt.
 */
#ifdef CONFIG_DEBUG_LOCK_ALLOC
extern void mutex_lock_nested(struct mutex *lock, unsigned int subclass);
extern int __must_check mutex_lock_interruptible_nested(struct mutex *lock,
					unsigned int subclass);
extern int __must_check mutex_lock_killable_nested(struct mutex *lock,
					unsigned int subclass);

#define mutex_lock(lock) mutex_lock_nested(lock, 0)
#define mutex_lock_interruptible(lock) mutex_lock_interruptible_nested(lock, 0)
#define mutex_lock_killable(lock) mutex_lock_killable_nested(lock, 0)
#else

static inline void mutex_lock(struct mutex *lock)
{
  down(&lock->sem);
}

static inline int __must_check mutex_lock_interruptible(struct mutex *lock)
{
  return down_interruptible(&lock->sem);
}

static inline int __must_check mutex_lock_killable(struct mutex *lock)
{
  return down_killable(&lock->sem);
}

# define mutex_lock_nested(lock, subclass) mutex_lock(lock)
# define mutex_lock_interruptible_nested(lock, subclass) mutex_lock_interruptible(lock)
# define mutex_lock_killable_nested(lock, subclass) mutex_lock_killable(lock)
#endif

/*
 * NOTE: mutex_trylock() follows the spin_trylock() convention,
 *       not the down_trylock() convention!
 *
 * Returns 1 if the mutex has been acquired successfully, and 0 on contention.
 */
static inline int mutex_trylock(struct mutex *lock)
{
  return ! down_trylock(&lock->sem);
}

static inline void mutex_unlock(struct mutex *lock)
{
  up(&lock->sem);
}

#endif
