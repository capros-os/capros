/*
 * Copyright (C) 2003, Jonathan S. Shapiro.
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

#include <string.h>

#include <eros/target.h>
#include <eros/Invoke.h>

#include <idl/capros/key.h>
#include <idl/capros/Process.h>
#include <idl/capros/ProcCre.h>
#include <idl/capros/SpaceBank.h>
#include <idl/capros/GPT.h>
#include <idl/capros/SuperNode.h>

#include <domain/Runtime.h>
#include <domain/assert.h>

#include <linuxk/linux-emul.h>
#include <linuxk/lsync.h>
#include <linux/mutex.h>
#include <linux/thread_info.h>
#include <asm/bitops.h>

static DEFINE_MUTEX(threadAllocLock);

/* There is a maximum of 32 threads (limited by the address space
at LK_STACK_BASE), so a bit vector of threads fits in 32 bits.
Bit i is 1 iff thread i is allocated.
Thread 0 is the initial thread. */
uint32_t threadsAlloc = 0x1;

/* Create a thread in a Linux driver. */
/* Note: this procedure may return before the new thread has consumed
   its parameter (arg). Synchronize with the new thread before
   changing or deallocating arg. Example:

void main() {
  struct myarg {
    DECLARE_MUTEX_LOCKED(arglock);
    int otherarg;
  } args;
  lthread_new_thread(1024, threadProc, &args, &newThreadNum);
  down(&args->arglock);
  up(&args->arglock);	// for cleanliness
}

void threadProc(struct myarg * args) {
  consume(args->otherarg);
  up(&args->arglock);
}

In effect, arglock starts out locked by the new thread.
*/
result_t
lthread_new_thread(uint32_t stackSize,
		   void * (* start_routine)(void *), void * arg,
		   /* out */ unsigned int * newThreadNum)
{
#define KR_NEWTHREAD KR_TEMP2
  // Allocate a thread number.
  mutex_lock(&threadAllocLock);
  if (threadsAlloc == 0xffffffff) {
    mutex_unlock(&threadAllocLock);
    return -1;
  }
  unsigned int threadNum = ffz(threadsAlloc);
  threadsAlloc |= (uint32_t)1 << threadNum;
  mutex_unlock(&threadAllocLock);

  uint32_t kr;
  result_t result;
  Message msg;
  
  // Create the process.
  result = capros_ProcCre_createProcess(KR_CREATOR, KR_BANK, KR_NEWTHREAD);
  if (result != RC_OK)
    return result;

  // Create its stack.
  uint32_t stackPages = (stackSize + EROS_PAGE_SIZE - 1) >> EROS_PAGE_LGSIZE;
  if (stackPages >= capros_GPT_nSlots)
    /* Disallow =, because we want an empty slot to guard against overflow. */
    return -1;

  if (stackPages == 1) {
    result = capros_SpaceBank_alloc1(KR_BANK, capros_Range_otPage, KR_TEMP0);
    if (result != RC_OK)
      return result;	// FIXME: clean up

    result = capros_Memory_makeGuarded(KR_TEMP0,
                 (capros_GPT_nSlots - 1) << EROS_PAGE_LGSIZE,
                 KR_TEMP0 );
    assert(result == RC_OK);
  } else {
    // Need a GPT to hold the stack pages.
    /* Don't bother to optimize by grabbing up to 3 pages at once,
     on the assumption that the stack is seldom more than 1 page. */
    // Stack space is conveniently arranged in a single node.
    result = capros_SpaceBank_alloc1(KR_BANK, capros_Range_otGPT, KR_TEMP0);
    if (result != RC_OK)
      return result;	// FIXME: clean up

    capros_GPT_setL2v(KR_TEMP0, EROS_PAGE_LGSIZE);

    int i;
    for (i = 0; i < stackPages; i++) {
      result = capros_SpaceBank_alloc1(KR_BANK, capros_Range_otPage, KR_TEMP1);
      if (result != RC_OK)
        return result;	// FIXME: clean up

      capros_GPT_setSlot(KR_TEMP0, capros_GPT_nSlots - 1 - i,
                          KR_TEMP1);
    }
  }
  // KR_TEMP0 has the memory key for the stack.
  result = capros_Node_getSlotExtended(KR_KEYSTORE, LKSN_STACKS_GPT, KR_TEMP1);
  assert(result == RC_OK);
  result = capros_GPT_setSlot(KR_TEMP1, threadNum, KR_TEMP0);
  assert(result == RC_OK);

  /* The process's stack will have:
  0x4xxffc: struct thread_info.preempt_count
  0x4xxff8: struct thread_info
  0x4xxff4: the number of pages in the stack
  0x4xxff0: start_routine, temporarily
  0x4xxfec: arg, temporarily
  */

  // Store the starting procedure and arg on the stack:
  void * * sp = (void * *)(LK_STACK_BASE
                           + (LK_STACK_AREA * (threadNum + 1))
                           - SIZEOF_THREAD_INFO );
    // + 1 above is to get to the high end of the stack
  *(uint32_t *)(--sp) = stackPages;
  *(--sp) = start_routine;
  *(--sp) = arg;

  /* Copy the address space to the new thread */
  result = capros_Process_getAddrSpace(KR_SELF, KR_TEMP0);
  assert(result == RC_OK);

  result = capros_Process_swapAddrSpace(KR_NEWTHREAD, KR_TEMP0, KR_VOID);
  assert(result == RC_OK);
  
  /* Copy schedule key */
  result = capros_Process_getSchedule(KR_SELF, KR_TEMP0);
  assert(result == RC_OK);

  result = capros_Process_swapSchedule(KR_NEWTHREAD, KR_TEMP0, KR_VOID);
  assert(result == RC_OK);

  /* Now just copy all key registers */
  // This is slow; is it necessary?
  for (kr = 1; kr < EROS_NODE_SIZE; kr++) {
    result = capros_Process_getKeyReg(KR_SELF, kr, KR_TEMP0);
    assert(result == RC_OK);

    result = capros_Process_swapKeyReg(KR_NEWTHREAD, kr, KR_TEMP0, KR_VOID);
    assert(result == RC_OK);
  }

  // Set its KR_SELF.
  result = capros_Process_swapKeyReg(KR_NEWTHREAD, KR_SELF,
             KR_NEWTHREAD, KR_VOID);
  assert(result == RC_OK);

  // Set the PC and SP.
  void lk_thread_start(void *);
  struct capros_Process_CommonRegisters32 regs = {
    .procFlags = 0,
    .faultCode = 0,
    .faultInfo = 0,
    .pc = (uint32_t)&lk_thread_start,
    .sp = (uint32_t)sp
  };

  result = capros_Process_setRegisters32(KR_NEWTHREAD, regs);
  assert(result == RC_OK);

  result = capros_Node_swapSlotExtended(KR_KEYSTORE,
             LKSN_THREAD_PROCESS_KEYS + threadNum, KR_NEWTHREAD, KR_VOID);
  assert(result == RC_OK);
  
  result = capros_Process_makeResumeKey(KR_NEWTHREAD, KR_TEMP0);
  assert(result == RC_OK);
  
  /* Invoke the resume key to start the thread */
  memset(&msg, 0, sizeof(Message));
  msg.snd_invKey = KR_TEMP0;
  SEND(&msg);

  if (newThreadNum)
    *newThreadNum = threadNum;
  return RC_OK;
#undef KR_NEWTHREAD
}

// Stop the current thread.
void
lthread_exit(void)
{
  unsigned int threadNum = lk_getCurrentThreadNum();
  uint32_t stackPages = *((uint32_t *)current_thread_info() - 1);

  mutex_lock(&threadAllocLock);

  capros_LSync_threadDestroy(KR_LSYNC, threadNum, stackPages);
  assert(false);	// shouldn't get here
}

// This must be called with the threadAllocLock held.
// It deallocates the thread number and releases the lock.
void
lthreadDeallocateNum(unsigned int threadNum)
{
  threadsAlloc &= ~((uint32_t)1 << threadNum);
  mutex_unlock(&threadAllocLock);
}
