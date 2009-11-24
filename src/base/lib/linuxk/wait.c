/*
 * Copyright (C) 2007, 2008, 2009, Strawberry Development Group.
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
//#include <linuxk/lsync.h>
#include <linux/kernel.h>
#include <linux/wait.h>
#include <linux/semaphore.h>

void init_waitqueue_head(wait_queue_head_t *q)
{
  mutex_init(&q->mutx);
  INIT_LIST_HEAD(&q->task_list);
}

void fastcall add_wait_queue(wait_queue_head_t *q, wait_queue_t *wait)
{
	wait->flags &= ~WQ_FLAG_EXCLUSIVE;
	mutex_lock(&q->mutx);
	__add_wait_queue(q, wait);
	mutex_unlock(&q->mutx);
}

void fastcall remove_wait_queue(wait_queue_head_t *q, wait_queue_t *wait)
{
	mutex_lock(&q->mutx);
	__remove_wait_queue(q, wait);
	mutex_unlock(&q->mutx);
}

void remove_wait_queue_if_on_it(wait_queue_head_t *q, wait_queue_t *wait)
{
	/* Note: Even if the wait is not on the queue, the mutex_lock()
	below is necessary, because we must wait until wake_up()
	is done with the semaphore in wait->private,
	before we go on to deallocate the semaphore. */
	mutex_lock(&q->mutx);
	if (!list_empty(&wait->task_list)) {
		__remove_wait_queue(q, wait);
	}
	mutex_unlock(&q->mutx);
}

int wait_event_wake_function(wait_queue_t *wait,
	unsigned mode, int sync, void *key)
{
	list_del_init(&wait->task_list);
	struct semaphore * sem = wait->private;
	up(sem);
	return 1;	// This doesn't work quite right for exclusive wakeups
}

void wet_timer_function(unsigned long data)
{
  struct semaphore * sem = (struct semaphore *)data;
  up(sem);
}

/*
 * The core wakeup function.  Non-exclusive wakeups (nr_exclusive == 0) just
 * wake everything up.  If it's an exclusive wakeup (nr_exclusive == small +ve
 * number) then we wake all the non-exclusive tasks and one exclusive task.
 */
void __wake_up_common(wait_queue_head_t *q, unsigned int mode,
			     int nr_exclusive, int sync, void *key)
{
	struct list_head *tmp, *next;

	list_for_each_safe(tmp, next, &q->task_list) {
		wait_queue_t *curr = list_entry(tmp, wait_queue_t, task_list);
		unsigned flags = curr->flags;

		if (curr->func(curr, mode, sync, key) &&
				(flags & WQ_FLAG_EXCLUSIVE) && !--nr_exclusive)
			break;
	}
}

/**
 * __wake_up - wake up threads blocked on a waitqueue.
 * @q: the waitqueue
 * @mode: which threads
 * @nr_exclusive: how many wake-one or wake-many threads to wake up
 * @key: is directly passed to the wakeup function
 */
void fastcall __wake_up(wait_queue_head_t *q, unsigned int mode,
			int nr_exclusive, void *key)
{
	mutex_lock(&q->mutx);
	__wake_up_common(q, mode, nr_exclusive, 0, key);
	mutex_unlock(&q->mutx);
}

// Same as __wake_up but called with the spinlock in wait_queue_head_t held.
void
__wake_up_locked(wait_queue_head_t * q, unsigned int mode)
{
  __wake_up_common(q, mode, 1, 0, NULL);
}
