/* rwsem-spinlock.h: fallback C implementation
 *
 * Copyright (c) 2001   David Howells (dhowells@redhat.com).
 * - Derived partially from ideas by Andrea Arcangeli <andrea@suse.de>
 * - Derived also from comments by Linus
 * Portions Copyright (C) 2007, Strawberry Development Group.
 */
/* This material is based upon work supported by the US Defense Advanced
Research Projects Agency under Contract No. W31P4Q-07-C-0070.
Approved for public release, distribution unlimited. */

#ifndef _LINUX_RWSEM_SPINLOCK_H
#define _LINUX_RWSEM_SPINLOCK_H

#ifndef _LINUX_RWSEM_H
#error "please don't include linux/rwsem-spinlock.h directly, use linux/rwsem.h instead"
#endif

#include <linux/list.h>
#include <linux/lockdep.h>
#include <asm/atomic.h>

#ifdef __KERNEL__

#include <linux/types.h>

struct rwsem_waiter;

/*
 * the rw-semaphore definition
 * - if activity is 0 then there are no active readers or writers
 * - if activity is +ve then that is the number of active readers
 * - if activity is -1 then there is one active writer
 * - if contenders is nonzero, then there are processes
     waiting for the semaphore
 */
struct rw_semaphore {
  atomic_t activity;
  atomic_t contenders;
  // readWaiters must be accessed only by the sync process.
  struct list_head readWaiters;
  // writeWaiters must be accessed only by the sync process.
  struct list_head writeWaiters;
#ifdef CONFIG_DEBUG_LOCK_ALLOC
	struct lockdep_map dep_map;
#endif
};

#ifdef CONFIG_DEBUG_LOCK_ALLOC
# define __RWSEM_DEP_MAP_INIT(lockname) , .dep_map = { .name = #lockname }
#else
# define __RWSEM_DEP_MAP_INIT(lockname)
#endif

#define __RWSEM_INITIALIZER(name) \
{ \
  .activity = ATOMIC_INIT(0), \
  .contenders = ATOMIC_INIT(0), \
  .readWaiters  = { &(name).readWaiters, &(name).readWaiters } \
  .writeWaiters = { &(name).writeWaiters, &(name).writeWaiters } \
  __RWSEM_DEP_MAP_INIT(name) \
}

#define DECLARE_RWSEM(name) \
	struct rw_semaphore name = __RWSEM_INITIALIZER(name)

extern void __init_rwsem(struct rw_semaphore *sem, const char *name,
			 struct lock_class_key *key);

#define init_rwsem(sem)						\
do {								\
	static struct lock_class_key __key;			\
								\
	__init_rwsem((sem), #sem, &__key);			\
} while (0)

static inline bool
__rwtrylock_read(struct rw_semaphore * sem)
{
  int newcnt = atomic_read(&sem->activity);
  int cnt;
  do {
    if (newcnt < 0) return false;	// too bad, there is a writer
    cnt = newcnt;
    /* If the activity is still cnt, increment it. */
    newcnt = atomic_cmpxchg(&sem->activity, cnt, cnt + 1);
  } while (newcnt != cnt);
  return true;	// success
}

static inline bool
__rwtrylock_write(struct rw_semaphore * sem)
{
  int newcnt = atomic_read(&sem->activity);
  int cnt;
  do {
    if (newcnt != 0) return false;	// there are reader(s) or a writer
    cnt = newcnt;
    /* If the activity is still 0, set it to -1. */
    newcnt = atomic_cmpxchg(&sem->activity, cnt, -1);
  } while (newcnt != cnt);
  return true;	// success
}

static inline int rwsem_is_locked(struct rw_semaphore *sem)
{
	return (atomic_read(&sem->activity) != 0);
}

#endif /* __KERNEL__ */
#endif /* _LINUX_RWSEM_SPINLOCK_H */
