#ifndef __LINUX_SPINLOCK_API_UP_H
#define __LINUX_SPINLOCK_API_UP_H

#ifndef __LINUX_SPINLOCK_H
# error "please don't include this file directly"
#endif

/*
 * include/linux/spinlock_api_up.h
 *
 * spinlock API implementation on UP-nondebug (inlined implementation)
 *
 * portions Copyright 2005, Red Hat, Inc., Ingo Molnar
 * Copyright (C) 2007, Strawberry Development Group.
 * Released under the General Public License (GPL).
 */
/* This material is based upon work supported by the US Defense Advanced
Research Projects Agency under Contract No. W31P4Q-07-C-0070.
Approved for public release, distribution unlimited. */

#define in_lock_functions(ADDR)		0

#define assert_spin_locked(lock)	do { (void)(lock); } while (0)

#define __LOCK(lock) \
  do { down(&(lock)->raw_lock.sem); } while (0)

#define __LOCK_BH(lock) \
  do { local_bh_disable(); __LOCK(lock); } while (0)

#define __LOCK_IRQ(lock) __LOCK(lock)

#define __LOCK_IRQSAVE(lock, flags) \
  do { (void)(flags); __LOCK(lock); } while (0)

#define __UNLOCK(lock) \
  do { up(&(lock)->raw_lock.sem); } while (0)

#define __UNLOCK_BH(lock) \
  do { local_bh_enable(); up(&(lock)->raw_lock.sem); } while (0)

#define __UNLOCK_IRQ(lock) __UNLOCK(lock)

#define __UNLOCK_IRQRESTORE(lock, flags) \
  do { (void)(flags); __UNLOCK(lock); } while (0)

#define _spin_lock(lock)			__LOCK(lock)
#define _spin_lock_nested(lock, subclass)	__LOCK(lock)
#define _read_lock(lock)			__LOCK(lock)
#define _write_lock(lock)			__LOCK(lock)
#define _spin_lock_bh(lock)			__LOCK_BH(lock)
#define _read_lock_bh(lock)			__LOCK_BH(lock)
#define _write_lock_bh(lock)			__LOCK_BH(lock)
#define _spin_lock_irq(lock)			__LOCK_IRQ(lock)
#define _read_lock_irq(lock)			__LOCK_IRQ(lock)
#define _write_lock_irq(lock)			__LOCK_IRQ(lock)
#define _spin_lock_irqsave(lock, flags)		__LOCK_IRQSAVE(lock, flags)
#define _read_lock_irqsave(lock, flags)		__LOCK_IRQSAVE(lock, flags)
#define _write_lock_irqsave(lock, flags)	__LOCK_IRQSAVE(lock, flags)
#define _spin_trylock(lock)			down_trylock(&(lock)->raw_lock.sem)
#define _read_trylock(lock)			unimplemented
#define _write_trylock(lock)			unimplemented
#define _spin_trylock_bh(lock)			unimplemented
#define _spin_unlock(lock)			__UNLOCK(lock)
#define _read_unlock(lock)			__UNLOCK(lock)
#define _write_unlock(lock)			__UNLOCK(lock)
#define _spin_unlock_bh(lock)			__UNLOCK_BH(lock)
#define _write_unlock_bh(lock)			__UNLOCK_BH(lock)
#define _read_unlock_bh(lock)			__UNLOCK_BH(lock)
#define _spin_unlock_irq(lock)			__UNLOCK_IRQ(lock)
#define _read_unlock_irq(lock)			__UNLOCK_IRQ(lock)
#define _write_unlock_irq(lock)			__UNLOCK_IRQ(lock)
#define _spin_unlock_irqrestore(lock, flags)	__UNLOCK_IRQRESTORE(lock, flags)
#define _read_unlock_irqrestore(lock, flags)	__UNLOCK_IRQRESTORE(lock, flags)
#define _write_unlock_irqrestore(lock, flags)	__UNLOCK_IRQRESTORE(lock, flags)

#endif /* __LINUX_SPINLOCK_API_UP_H */
