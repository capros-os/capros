/*
 * Copyright (C) 2007, Strawberry Development Group.
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
#include <linuxk/lsync.h>
#include <domain/assert.h>
#include <linux/timer.h>
#include <linux/completion.h>

void complete(struct completion * x)
{
  if (atomic_add_return(compl_completed, &x->done) == compl_completed)
    // If we were the first to make done nonzero, wake up the waiter:
    up(&x->sem);
}

void
compl_timer_fn(unsigned long data)
{
  struct completion * x = (struct completion *)data;

  if (atomic_add_return(compl_timedout, &x->done) == compl_timedout)
    // If we were the first to make done nonzero, wake up the waiter:
    up(&x->sem);
}

void
wait_for_completion(struct completion * x)
{
  down(&x->sem);
  atomic_set(&x->done, 0);	// reinitialize
}

unsigned long
wait_for_completion_timeout(struct completion * x, unsigned long timeout)
{
  // Quick test if already done:
  if (atomic_read(&x->done)) {
    wait_for_completion(x);
    return timeout;
  }

  DEFINE_TIMER(compl_timer, &compl_timer_fn, timeout, (unsigned long)x);
  add_timer(&compl_timer);

  down(&x->sem);	// wait for completion or timeout

  unsigned long remaining = timer_remaining_time(&compl_timer);
  int wasActive = del_timer(&compl_timer);
  /* FIXME: there is a race here; the timer thread could still be
  calling the timer function, even after this procedure has returned
  (which is somewhat harmful) and after x has been deallocated
  (which would be bad). */

  int oldDone = atomic_xchg(&x->done, 0);	// reset done to 0

  /* If wasActive, del_timer cancelled the timer and
  compl_timer_fn will not be called. */
  if (! wasActive) {
    // The timer has fired or is about to fire.
    if (! (oldDone & compl_timedout)) {
      /* The timer hasn't fired.
      Wait for it to fire, to ensure it is done referencing x. */
      down(&x->sem);
      atomic_set(&x->done, 0);	// reinitialize for next time
    }
  }

  if (oldDone & compl_completed) {
    // It was completed.
    if (remaining)
      return remaining;
    else return 1;	// don't return zero if completed
  } else {
    return 0;
  }
}
