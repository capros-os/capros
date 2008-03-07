/*
 * Copyright (C) 2007, 2008, Strawberry Development Group.
 *
 * This file is part of the CapROS Operating System.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2,
 * or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */
/* This material is based upon work supported by the US Defense Advanced
Research Projects Agency under Contract No. W31P4Q-07-C-0070.
Approved for public release, distribution unlimited. */

/* LSync -- Synchronization process for Linux kernel emulation.
*/

#include <eros/target.h>
#include <eros/Invoke.h>
#include <idl/capros/SpaceBank.h>
#include <idl/capros/GPT.h>
#include <idl/capros/ProcCre.h>
#include <idl/capros/SuperNode.h>
#include <idl/capros/Sleep.h>
#include <idl/capros/LSync.h>

#include <domain/domdbg.h>
#include <domain/assert.h>
#include <domain/ProtoSpaceDS.h>
#include <linuxk/linux-emul.h>
#include <linuxk/lsync.h>

#include <asm/semaphore.h>
#include <linux/rwsem.h>
#include <linux/wait.h>

// Private interface to semaphore.c:
bool tryUp(struct semaphore * sem);

// Private interfaces to lthread.c:
void lthread_destroy_internal(unsigned int threadNum);
extern uint32_t threadsAlloc;
extern struct mutex threadAllocLock;

#define dbg_init    0x1

/* Following should be an OR of some of the above */
#define dbg_flags   ( 0 )

#define DEBUG(x) if (dbg_##x & dbg_flags)


// Initialize delayCalibrationConstant to an invalid value (too big),
// until it gets properly initialized.
uint32_t delayCalibrationConstant = (1UL << 31);

static void
Sepuku(result_t retCode)
{
  // This is primordial; it should not go away
  assert(((void)"lsync Sepuku called", false));
}

static void
wakeupWQ(wait_queue_t * wq)
{
  Message msg = {
    .snd_code = RC_OK,
    .snd_invKey = KR_TEMP0,
    .snd_key0 = KR_VOID,
    .snd_key1 = KR_VOID,
    .snd_key2 = KR_VOID,
    .snd_rsmkey = KR_VOID
  };

  list_del(&wq->task_list);	// remove from list
  capros_Node_getSlotExtended(KR_KEYSTORE,
          LKSN_THREAD_RESUME_KEYS + wq->threadNum,
          KR_TEMP0);
  SEND(&msg);
}

static void
doSemaWakeup(struct semaphore * sem)
{
  if (list_empty(&sem->task_list)) {
    sem->wakeupsWaiting++;	// only this thread references wakeupsWaiting
  } else {
    wait_queue_t * wq = list_first_entry(&sem->task_list,
                          wait_queue_t, task_list);
    wakeupWQ(wq);
  }
}

static void
lsync_mutex_unlock(struct mutex * mut)
{
  struct semaphore * sem = &mut->sem;

  /* We can't just use mutex_unlock(), because that may call lsync,
  which deadlocks. */
  
  if (tryUp(sem)) {
    doSemaWakeup(sem);
  }
}

static void
checkRwSem(struct rw_semaphore * sem)
{
  // Writers have priority:
  if (! list_empty(&sem->writeWaiters)) {
    if (__rwtrylock_write(sem)) {
      // A writer got the lock.
      wait_queue_t * wq = list_first_entry(&sem->writeWaiters,
                            wait_queue_t, task_list);
      atomic_dec(&sem->contenders);
      wakeupWQ(wq);
      return;		// no one else can get it
    }
  }
  
  while (! list_empty(&sem->readWaiters)) {
    if (__rwtrylock_read(sem)) {
      // A reader got the lock.
      wait_queue_t * wq = list_first_entry(&sem->readWaiters,
                            wait_queue_t, task_list);
      atomic_dec(&sem->contenders);
      wakeupWQ(wq);
    }
    else break;		// no readers can get it
  }
}

void *
lsync_main(void * arg)
{
  result_t result;
  Message mymsg;
  Message * msg = &mymsg;	// to address it consistently

  // Initialize delayCalibrationConstant.
  result = capros_Sleep_getDelayCalibration(KR_SLEEP,
             &delayCalibrationConstant);
  assert(result == RC_OK);

  msg->snd_invKey = KR_VOID;
  msg->snd_key0 = KR_VOID;
  msg->snd_key1 = KR_VOID;
  msg->snd_key2 = KR_VOID;
  msg->snd_rsmkey = KR_VOID;
  msg->snd_len = 0;
  msg->snd_code = 0;
  msg->snd_w1 = 0;
  msg->snd_w2 = 0;
  msg->snd_w3 = 0;

  msg->rcv_key0 = KR_ARG(0);
  msg->rcv_key1 = KR_VOID;
  msg->rcv_key2 = KR_VOID;
  msg->rcv_rsmkey = KR_RETURN;
  msg->rcv_limit = 0;

  for(;;) {
    RETURN(msg);

    // Set default return values:
    msg->snd_invKey = KR_RETURN;
    msg->snd_key0 = KR_VOID;
    msg->snd_key1 = KR_VOID;
    msg->snd_key2 = KR_VOID;
    msg->snd_rsmkey = KR_VOID;
    msg->snd_len = 0;
    msg->snd_w1 = 0;
    msg->snd_w2 = 0;
    msg->snd_w3 = 0;
    // msg->snd_code has no default.

    switch (msg->rcv_code) {

    default:
      msg->snd_code = RC_capros_key_UnknownRequest;
      break;

    case OC_capros_key_getType:
      msg->snd_w1 = IKT_capros_LSync;
      break;

    case OC_capros_LSync_semaWait:
    {
      struct semaphore * sem = (struct semaphore *)msg->rcv_w1;
      wait_queue_t * wq = (wait_queue_t *)msg->rcv_w2;
      // The caller has already decremented the count. 
      if (sem->wakeupsWaiting > 0) {
        // The wakeup already came in.
        sem->wakeupsWaiting--;	// only this thread references wakeupsWaiting
        msg->snd_code = RC_OK;
        break;		// return to the caller
      }
      list_add_tail(&wq->task_list, &sem->task_list);

      // Save the resume key to the waiting thread.
      capros_Node_swapSlotExtended(KR_KEYSTORE,
        LKSN_THREAD_RESUME_KEYS + wq->threadNum,
        KR_RETURN, KR_VOID);
      msg->snd_invKey = KR_VOID;
      break;
    }

    case OC_capros_LSync_semaWakeup:
    {
      doSemaWakeup((struct semaphore *)msg->rcv_w1);

      // Return to caller.
      msg->snd_invKey = KR_RETURN;
      msg->snd_code = RC_OK;
      break;
    }

    case OC_capros_LSync_rwsemWait:
    {
      struct rw_semaphore * sem = (struct rw_semaphore *)msg->rcv_w1;
      bool write = msg->rcv_w2;
      wait_queue_t * wq = (wait_queue_t *)msg->rcv_w3;

      list_add_tail(&wq->task_list,
                    write ? &sem->writeWaiters : &sem->readWaiters);

      // Save the resume key to the waiting thread.
      capros_Node_swapSlotExtended(KR_KEYSTORE,
        LKSN_THREAD_RESUME_KEYS + wq->threadNum,
        KR_RETURN, KR_VOID);

      // To resolve a race, we need to check if we can wake him up now:
      checkRwSem(sem);

      msg->snd_invKey = KR_VOID;
      break;
    }

    case OC_capros_LSync_rwsemWakeup:
    {
      struct rw_semaphore * sem = (struct rw_semaphore *)msg->rcv_w1;

      checkRwSem(sem);

      // Return to caller.
      msg->snd_invKey = KR_RETURN;
      msg->snd_code = RC_OK;
      break;
    }

    case OC_capros_LSync_threadDestroy: ;
      lthread_destroy_internal(msg->rcv_w1);
      /* The caller locked threadAllocLock. He cannot unlock it,
      so we do it for him: */
      lsync_mutex_unlock(&threadAllocLock);
      break;

    case OC_capros_LSync_allThreadsDestroy: ;
      /* Destroy all threads except #0 (which is the caller)
      and #1 (which is this thread). */
      /* The caller holds threadAllocLock. */
      do {
        uint32_t ta = threadsAlloc & ~0x3L;
        if (ta == 0)
          break;
        unsigned int threadNum = ffs(ta) - 1;
        lthread_destroy_internal(threadNum);
      } while (true);
      break;

    case OC_capros_key_destroy:
      // Implemented??
      Sepuku(RC_OK);
      /* NOTREACHED */
    }
  }
}
