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

/**
 * struct completion - structure used to maintain state for a "completion"
 *
 * This is the opaque structure used to maintain the state for a "completion".
 * Completions currently use a FIFO to queue threads that have to wait for
 * the "completion" event.
 *
 * See also:  complete(), wait_for_completion() (and friends _timeout,
 * _interruptible, _interruptible_timeout, and _killable), init_completion(),
 * and macros DECLARE_COMPLETION(), DECLARE_COMPLETION_ONSTACK(), and
 * INIT_COMPLETION().
 */
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
	{ ATOMIC_INIT(0), __SEMAPHORE_INITIALIZER((work).sem, 0) }

#define COMPLETION_INITIALIZER_ONSTACK(work) \
	({ init_completion(&work); work; })

/**
 * DECLARE_COMPLETION: - declare and initialize a completion structure
 * @work:  identifier for the completion structure
 *
 * This macro declares and initializes a completion structure. Generally used
 * for static declarations. You should use the _ONSTACK variant for automatic
 * variables.
 */
#define DECLARE_COMPLETION(work) \
	struct completion work = COMPLETION_INITIALIZER(work)

/*
 * Lockdep needs to run a non-constant initializer for on-stack
 * completions - so we use the _ONSTACK() variant for those that
 * are on the kernel stack:
 */
/**
 * DECLARE_COMPLETION_ONSTACK: - declare and initialize a completion structure
 * @work:  identifier for the completion structure
 *
 * This macro declares and initializes a completion structure on the kernel
 * stack.
 */
#ifdef CONFIG_LOCKDEP
# define DECLARE_COMPLETION_ONSTACK(work) \
	struct completion work = COMPLETION_INITIALIZER_ONSTACK(work)
#else
# define DECLARE_COMPLETION_ONSTACK(work) DECLARE_COMPLETION(work)
#endif

/**
 * init_completion: - Initialize a dynamically allocated completion
 * @x:  completion structure that is to be initialized
 *
 * This inline function will initialize a dynamically created completion
 * structure.
 */
static inline void init_completion(struct completion *x)
{
	atomic_set(&x->done, 0);
	sema_init(&x->sem, 0);
}

extern void wait_for_completion(struct completion *);

static inline int wait_for_completion_interruptible(struct completion * x)
{
  wait_for_completion(x);
  return 0;	// no signals in CapROS
}

extern int wait_for_completion_killable(struct completion *x);
extern unsigned long wait_for_completion_timeout(struct completion *x,
					   unsigned long timeout);

static inline unsigned long
wait_for_completion_interruptible_timeout(struct completion *x,
                                          unsigned long timeout)
{
  return wait_for_completion_timeout(x,timeout);
}

extern bool try_wait_for_completion(struct completion *x);
extern bool completion_done(struct completion *x);

extern void complete(struct completion *);
extern void complete_all(struct completion *);

#if 0 // CapROS
/**
 * INIT_COMPLETION: - reinitialize a completion structure
 * @x:  completion structure to be reinitialized
 *
 * This macro should be used to reinitialize a completion structure so it can
 * be reused. This is especially important after complete_all() is used.
 */
#define INIT_COMPLETION(x)	((x).done = 0)
#endif // CapROS


#endif
