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

#include <eros/Invoke.h>
#include <domain/assert.h>
#include <linuxk/linux-emul.h>
#include <linuxk/lsync.h>
#include <linux/kthread.h>
#include <domain/cmtesync.h>
#include <domain/CMTEThread.h>

#define kthreadStackSize 2048	// just a guess

struct kthread_create_info {
  struct task_struct ts;
  int (*threadfn)(void *);
  void * data;

  bool should_stop;
  int retval;
  struct semaphore stop_sem;
};

// kthread_should_stop needs this to find itself.
struct kthread_create_info * kciArray[LK_MAX_THREADS];

void *
kthread_proc(void * arg)
{
  struct kthread_create_info * kci = arg;
  unsigned int threadNum = lk_getCurrentThreadNum();
  kciArray[threadNum] = kci;

  int ret = (*kci->threadfn)(kci->data);

  // kthread function exited, presumably as a result of kthread_stop().
  if (kci->should_stop) {
    // Exit was a result of kthread_stop().
    kci->retval = ret;
    up(&kci->stop_sem);
    // kthread_stop() will free kci.
  } else {
    kfree(kci);
  }

  return NULL;
}

struct task_struct *
kthread_run(int (*threadfn)(void *data),
            void *data,
            const char namefmt[], ...)
{
  result_t result;
  struct kthread_create_info * kci = kzalloc(sizeof(*kci), GFP_KERNEL);
  if (!kci)
    return ERR_PTR(-ENOMEM);

  sema_init(&kci->stop_sem, 0);
  kci->threadfn = threadfn;
  kci->data = data;

  unsigned int newThreadNum;
  result = CMTEThread_create(kthreadStackSize,
             &kthread_proc, kci,
             &newThreadNum);
  if (result != RC_OK) {
    kfree(kci);
    return ERR_PTR(-ENOMEM);
  }

  kci->ts.pid = newThreadNum;
  return &kci->ts;
}

int
kthread_should_stop(void)
{
  unsigned int threadNum = lk_getCurrentThreadNum();
  struct kthread_create_info * kci = kciArray[threadNum];
  return kci->should_stop;
}

int
kthread_stop(struct task_struct * k)
{
  struct kthread_create_info * kci
    = container_of(k, struct kthread_create_info, ts);
  kci->should_stop = true;
  down(&kci->stop_sem);		// wait for it to stop
  int retval = kci->retval;
  kfree(kci);
  return retval;
}
