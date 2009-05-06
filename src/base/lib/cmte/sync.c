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
 * Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */
/* This material is based upon work supported by the US Defense Advanced
Research Projects Agency under Contract No. W31P4Q-07-C-0070.
Approved for public release, distribution unlimited. */

/* Synchronization process for CapROS Multi-Threaded Environment. */

#include <eros/target.h>
#include <eros/container_of.h>
#include <eros/Invoke.h>
#include <eros/ffs.h>
#include <idl/capros/SpaceBank.h>
#include <idl/capros/GPT.h>
#include <idl/capros/ProcCre.h>
#include <idl/capros/SuperNode.h>
#include <idl/capros/Sleep.h>
#include <idl/capros/LSync.h>

#include <domain/domdbg.h>
#include <domain/assert.h>
#include <domain/ProtoSpaceDS.h>
#include <domain/cmtesync.h>
#include <domain/CMTESemaphore.h>
#include <domain/CMTERWSemaphore.h>

// Private interface to semaphore.c:
bool CMTESemaphore_tryUp(CMTESemaphore * sem);

// Private interfaces to lthread.c:
void lthread_destroy_internal(unsigned int threadNum);
extern uint32_t threadsAlloc;
extern CMTESemaphore threadAllocLock;

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
wakeupWQ(CMTEWaitQueue * wq)
{
  Message msg = {
    .snd_code = RC_OK,
    .snd_invKey = KR_TEMP0,
    .snd_key0 = KR_VOID,
    .snd_key1 = KR_VOID,
    .snd_key2 = KR_VOID,
    .snd_rsmkey = KR_VOID
  };

  link_UnlinkUnsafe(&wq->link);	// remove from list
  capros_Node_getSlotExtended(KR_KEYSTORE,
          LKSN_THREAD_RESUME_KEYS + wq->threadNum,
          KR_TEMP0);
  SEND(&msg);
}

// Get the first CMTEWaitQueue on a non-empty list.
INLINE CMTEWaitQueue *
link_first_wq(Link * list)
{
  return container_of(list->next, CMTEWaitQueue, link);
}

static void
doSemaWakeup(CMTESemaphore * sem)
{
  if (link_isSingleton(&sem->task_list)) {
    sem->wakeupsWaiting++;	// only this thread references wakeupsWaiting
  } else {
    CMTEWaitQueue * wq = link_first_wq(&sem->task_list);
    wakeupWQ(wq);
  }
}

static void
lsync_mutex_unlock(CMTESemaphore * sem)
{
  /* We can't just use CMTEMutex_unlock(), because that may call lsync,
  which deadlocks. */
  
  if (CMTESemaphore_tryUp(sem)) {
    doSemaWakeup(sem);
  }
}

static void
checkRwSem(CMTERWSemaphore * sem)
{
  // Writers have priority:
  if (! link_isSingleton(&sem->writeWaiters)) {
    if (CMTERWSemaphore_tryDownWrite(sem)) {
      // A writer got the lock.
      CMTEWaitQueue * wq = link_first_wq(&sem->writeWaiters);
      capros_atomic32_add_return(&sem->contenders, -1);
      wakeupWQ(wq);
      return;		// no one else can get it
    }
  }
  
  while (! link_isSingleton(&sem->readWaiters)) {
    if (CMTERWSemaphore_tryDownRead(sem)) {
      // A reader got the lock.
      CMTEWaitQueue * wq = link_first_wq(&sem->readWaiters);
      capros_atomic32_add_return(&sem->contenders, -1);
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
      CMTESemaphore * sem = (CMTESemaphore *)msg->rcv_w1;
      CMTEWaitQueue * wq = (CMTEWaitQueue *)msg->rcv_w2;
      // The caller has already decremented the count. 
      if (sem->wakeupsWaiting > 0) {
        // The wakeup already came in.
        sem->wakeupsWaiting--;	// only this thread references wakeupsWaiting
        msg->snd_code = RC_OK;
        break;		// return to the caller
      }
      link_insertBefore(&sem->task_list, &wq->link);

      // Save the resume key to the waiting thread.
      capros_Node_swapSlotExtended(KR_KEYSTORE,
        LKSN_THREAD_RESUME_KEYS + wq->threadNum,
        KR_RETURN, KR_VOID);
      msg->snd_invKey = KR_VOID;
      break;
    }

    case OC_capros_LSync_semaWakeup:
    {
      doSemaWakeup((CMTESemaphore *)msg->rcv_w1);

      // Return to caller.
      msg->snd_invKey = KR_RETURN;
      msg->snd_code = RC_OK;
      break;
    }

    case OC_capros_LSync_rwsemWait:
    {
      CMTERWSemaphore * sem = (CMTERWSemaphore *)msg->rcv_w1;
      bool write = msg->rcv_w2;
      CMTEWaitQueue * wq = (CMTEWaitQueue *)msg->rcv_w3;

      link_insertBefore(write ? &sem->writeWaiters : &sem->readWaiters,
                        &wq->link);

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
      CMTERWSemaphore * sem = (CMTERWSemaphore *)msg->rcv_w1;

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
        unsigned int threadNum = ffs32(ta) - 1;
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
