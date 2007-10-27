/*
 * Copyright (C) 2007, Strawberry Development Group.
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
#include <idl/capros/SuperNode.h>
#include <idl/capros/LSync.h>

#include <domain/domdbg.h>
#include <domain/assert.h>
#include <domain/ProtoSpaceDS.h>
#include <linuxk/linux-emul.h>
#include <linuxk/lsync.h>

#include <asm/semaphore.h>
#include <linux/rwsem.h>

#define dbg_init    0x1

/* Following should be an OR of some of the above */
#define dbg_flags   ( 0 )

#define DEBUG(x) if (dbg_##x & dbg_flags)


static void
Sepuku(result_t retCode)
{
  // This is primordial; it should not go away
  assert(((void)"lsync Sepuku called", false));
}

static void
wakeupWQ(wait_queue_head_t * wqh, wait_queue_t * wq)
{
  Message msg;

  __remove_wait_queue(wqh, wq);	// remove from list
  capros_Node_getSlotExtended(KR_KEYSTORE,
          LKSN_THREAD_RESUME_KEYS + wq->threadNum,
          KR_TEMP0);
  msg.snd_code = RC_OK;
  msg.snd_invKey = KR_TEMP0;
  SEND(&msg);
}

static void
checkRwSem(struct rw_semaphore * sem)
{
  // Writers have priority:
  if (! list_empty(&sem->writeWaiters.task_list)) {
    if (__rwtrylock_write(sem)) {
      // A writer got the lock.
      wait_queue_t * wq = list_first_entry(&sem->writeWaiters.task_list,
                            wait_queue_t, task_list);
      atomic_dec(&sem->contenders);
      wakeupWQ(&sem->writeWaiters, wq);
      return;		// no one else can get it
    }
  }
  
  while (! list_empty(&sem->readWaiters.task_list)) {
    if (__rwtrylock_read(sem)) {
      // A reader got the lock.
      wait_queue_t * wq = list_first_entry(&sem->readWaiters.task_list,
                            wait_queue_t, task_list);
      atomic_dec(&sem->contenders);
      wakeupWQ(&sem->readWaiters, wq);
    }
    else break;		// no readers can get it
  }
}

int
main()
{
  Message mymsg;
  Message * msg = &mymsg;	// to address it consistently

  // Build our address space:
#if 0 // no, let mkimage do it
  result = capros_SpaceBank_alloc3(KR_BANK, capros_Range_otGPT
             | (capros_Range_otGPT << 8)
             | (capros_Range_otGPT << 16),
             KR_TEMP0, KR_TEMP1, KR_TEMP2);
  if (result !=- RC_OK) {
    Sepuku(result);
  }
  ...
#endif

  msg->snd_invKey = KR_RETURN;
  msg->snd_key0 = KR_TEMP0;
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

    case OC_capros_LSync_semaWait:
    {
      struct semaphore * sem = (struct semaphore *)msg->snd_w1;
      wait_queue_t * wq = (wait_queue_t *)msg->snd_w2;
      // The caller has already decremented the count. 
      if (sem->wakeupsWaiting > 0) {
        // The wakeup already came in.
        sem->wakeupsWaiting--;	// only this thread references wakeupsWaiting
        msg->snd_code = RC_OK;
        break;		// return to the caller
      }
      __add_wait_queue_tail(&sem->wait, wq);

      // Save the resume key to the waiting thread.
      capros_Node_swapSlotExtended(KR_KEYSTORE,
        LKSN_THREAD_RESUME_KEYS + wq->threadNum,
        KR_RETURN, KR_VOID);
      msg->snd_invKey = KR_VOID;
      break;
    }

    case OC_capros_LSync_semaWakeup:
    {
      struct semaphore * sem = (struct semaphore *)msg->snd_w1;
      if (list_empty(&sem->wait.task_list)) {
        sem->wakeupsWaiting++;	// only this thread references wakeupsWaiting
      } else {
        wait_queue_t * wq = list_first_entry(&sem->wait.task_list,
                              wait_queue_t, task_list);
        wakeupWQ(&sem->wait, wq);
      }

      // Return to caller.
      msg->snd_invKey = KR_RETURN;
      msg->snd_code = RC_OK;
      break;
    }

    case OC_capros_LSync_rwsemWait:
    {
      struct rw_semaphore * sem = (struct rw_semaphore *)msg->snd_w1;
      bool write = msg->snd_w2;
      wait_queue_t * wq = (wait_queue_t *)msg->snd_w3;

      __add_wait_queue_tail(write ? &sem->writeWaiters : &sem->readWaiters,
                            wq);

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
      struct rw_semaphore * sem = (struct rw_semaphore *)msg->snd_w1;

      checkRwSem(sem);

      // Return to caller.
      msg->snd_invKey = KR_RETURN;
      msg->snd_code = RC_OK;
      break;
    }

    case OC_capros_key_destroy:
      // Implemented??
      Sepuku(RC_OK);
      /* NOTREACHED */

    default:
      msg->snd_code = RC_capros_key_UnknownRequest;
      break;
    }
  }
}
