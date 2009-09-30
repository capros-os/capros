#ifndef __LINUX_COMPLETION_H
#define __LINUX_COMPLETION_H

/*
 * (C) Copyright 2001 Linus Torvalds
 * Copyright (C) 2007, 2009, Strawberry Development Group.
 *
 * Atomic wait-for-completion handler data structures.
 * See kernel/sched.c for details.
 */

#include <linux/semaphore.h>

/* done has the number of unacknowledged completions times 2,
   plus one if there is a timeout.
sem has:
-1: a process is waiting and done is zero
0: no process is waiting and done is zero
1: no process is waiting and done is nonzero.
*/
#define compl_completed 0x2
#define compl_timedout  0x1
struct completion {
	atomic_t done;
	struct semaphore sem;
};

#define COMPLETION_INITIALIZER(work) \
	{ ATOMIC_INIT(0), __SEMAPHORE_INIT((work).sem, 0) }

#define COMPLETION_INITIALIZER_ONSTACK(work) \
	({ init_completion(&work); work; })

#define DECLARE_COMPLETION(work) \
	struct completion work = COMPLETION_INITIALIZER(work)

/*
 * Lockdep needs to run a non-constant initializer for on-stack
 * completions - so we use the _ONSTACK() variant for those that
 * are on the kernel stack:
 */
#ifdef CONFIG_LOCKDEP
# define DECLARE_COMPLETION_ONSTACK(work) \
	struct completion work = COMPLETION_INITIALIZER_ONSTACK(work)
#else
# define DECLARE_COMPLETION_ONSTACK(work) DECLARE_COMPLETION(work)
#endif

static inline void init_completion(struct completion *x)
{
	atomic_set(&x->done, 0);
	sema_init(&x->sem, 0);
}

void wait_for_completion(struct completion * x);

static inline void wait_for_completion_interruptible(struct completion * x)
{
  wait_for_completion(x);
}

unsigned long wait_for_completion_timeout(struct completion *x,
					   unsigned long timeout);

static inline unsigned long
wait_for_completion_interruptible_timeout(struct completion *x,
                                          unsigned long timeout)
{
  return wait_for_completion_timeout(x,timeout);
}

void complete(struct completion * x);

extern void FASTCALL(complete_all(struct completion *));

#endif
