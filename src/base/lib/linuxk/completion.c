/*
 * Copyright (C) 2007, 2008, Strawberry Development Group.
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
  if (atomic_sub_return(compl_completed, &x->done) != 0)
    up(&x->sem);	// get ready for the next wait
}

unsigned long
wait_for_completion_timeout(struct completion * x, unsigned long timeout)
{
  // Quick test if already done:
  if (atomic_read(&x->done)) {
    wait_for_completion(x);
    return timeout;
  }

  DEFINE_TIMER(compl_timer, &compl_timer_fn,
               jiffies + timeout, (unsigned long)x);
  add_timer(&compl_timer);

  down(&x->sem);	// wait for completion or timeout

  unsigned long remaining = timer_remaining_time(&compl_timer);
  int wasActive = del_timer(&compl_timer);

  /* Atomically update done:
       Clear compl_timedout.
       If there is a completion count, decrement it. */
  int oldDone, nextDone;
  int curDone = atomic_read(&x->done);
  do {
    oldDone = curDone;
    nextDone = oldDone & ~ compl_timedout;
    if (nextDone)
      nextDone -= compl_completed;
    curDone = atomic_cmpxchg(&x->done, curDone, nextDone);
  } while (curDone != oldDone);

  if (nextDone != 0)
    up(&x->sem);	// get ready for the next wait

  /* If wasActive, del_timer cancelled the timer and
  compl_timer_fn will not be called. */
  if (! wasActive) {
    // The timer has fired or is about to fire.
    if (! (oldDone & compl_timedout)) {
      /* The timer hasn't fired.
      Wait for it to fire, to ensure it is done referencing x. */
      // FIXME: this doesn't work with multiple completions.
      down(&x->sem);
      atomic_set(&x->done, 0);	// reinitialize for next time
    }
  }

  if (oldDone & ~ compl_timedout) {	// there was a completion
    // It was completed.
    if (remaining)
      return remaining;
    else return 1;	// don't return zero if completed
  } else {
    return 0;
  }
}
