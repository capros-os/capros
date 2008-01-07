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

#include <stddef.h>
#include <eros/target.h>
#include <eros/Invoke.h>
#include <domain/Runtime.h>
#include <idl/capros/GPT.h>
#include <idl/capros/Constructor.h>
#include <idl/capros/SuperNode.h>
#include <idl/capros/Process.h>
#include <linuxk/linux-emul.h>
#include <linux/thread_info.h>
#include <domain/assert.h>

/* This is the first code to run in a driver process.
   It sets up the .data and .bss sections in a vcsk. */

extern NORETURN void driver_main(void);

/* When called, KR_TEMP1 has a constructor builder key to the VCSK
   for the data section,
   KR_TEMP2 has the GPT to our address space,
   and KR_RETURN has the GPT to our stacks space. */
void
driver_start(void)
{
  // This needs more work to support C++ constructors.
  result_t result;

  // Create the VCSK for our .data. .bss, and heap.
  result = capros_Constructor_request(KR_TEMP1, KR_BANK, KR_SCHED, KR_VOID,
                                      KR_TEMP0);
  if (result != RC_OK) {
    *((int *)0) = 0xbadbad77;	// FIXME
  }

  result = capros_GPT_setSlot(KR_TEMP2, 3, KR_TEMP0);
  if (result != RC_OK) {
    *((int *)0) = 0xbadbad77;	// FIXME
  }

  /* Validate that SIZEOF_THREAD_INFO is correct, or at least good enough. */
  assert(SIZEOF_THREAD_INFO
         >= offsetof(struct thread_info, preempt_count)
            + sizeof(((struct thread_info *)0)->preempt_count) );

  // Create the KEYSTORE object.
  result = capros_Constructor_request(KR_KEYSTORE, KR_BANK, KR_SCHED, KR_VOID,
                             KR_KEYSTORE);
  assert(result == RC_OK);	// FIXME
  result = capros_SuperNode_allocateRange(KR_KEYSTORE, LKSN_THREAD_PROCESS_KEYS,
                      LKSN_MAPS_GPT);
  assert(result == RC_OK);	// FIXME
  // Populate it.
  capros_Node_swapSlotExtended(KR_KEYSTORE, LKSN_THREAD_PROCESS_KEYS+0,
                               KR_SELF, KR_VOID);
  capros_Node_swapSlotExtended(KR_KEYSTORE, LKSN_STACKS_GPT,
                               KR_RETURN, KR_VOID);

  // Create the lsync process.
  unsigned int lsyncThreadNum;
  result = lthread_new_thread(LSYNC_STACK_SIZE, &lsync_main, NULL,
                              &lsyncThreadNum);
  if (result != RC_OK) {
    assert(false);	// FIXME handle error
  }

  // Get lsync process key
  result = capros_Node_getSlotExtended(KR_KEYSTORE,
                              LKSN_THREAD_PROCESS_KEYS + lsyncThreadNum,
                              KR_TEMP0);
  assert(result == RC_OK);
  result = capros_Process_makeStartKey(KR_TEMP0, 0, KR_LSYNC);
  assert(result == RC_OK);
  result = capros_Process_swapKeyReg(KR_TEMP0, KR_LSYNC, KR_LSYNC, KR_VOID);
  assert(result == RC_OK);

  driver_main();
}
