#ifndef __LINUX_SPINLOCK_UP_H
#define __LINUX_SPINLOCK_UP_H

#ifndef __LINUX_SPINLOCK_H
# error "please don't include this file directly"
#endif

/*
 * include/linux/spinlock_up.h - UP-debug version of spinlocks.
 *
 * portions Copyright 2005, Red Hat, Inc., Ingo Molnar
 * Portions Copyright (C) 2007, Strawberry Development Group.
 * Released under the General Public License (GPL).
 */

#define __raw_spin_is_locked(x)		(atomic_read((x)->ssem.count) == 0)

static inline void __raw_spin_lock(raw_spinlock_t *lock)
{
	down(&lock->sem);
}

static inline int __raw_spin_trylock(raw_spinlock_t *lock)
{
	return down_trylock(&lock->sem);
}

static inline void __raw_spin_unlock(raw_spinlock_t *lock)
{
	up(&lock->sem);
}

#define __raw_read_can_lock(lock)	unimplemented
#define __raw_write_can_lock(lock)	unimplemented

#define __raw_spin_unlock_wait(lock)	unimplemented

#endif /* __LINUX_SPINLOCK_UP_H */
