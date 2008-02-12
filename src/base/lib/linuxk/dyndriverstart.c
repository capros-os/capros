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

#include <stddef.h>
#include <eros/target.h>
#include <eros/Invoke.h>
#include <domain/Runtime.h>
#include <idl/capros/GPT.h>
#include <idl/capros/Constructor.h>
#include <idl/capros/SuperNode.h>
#include <idl/capros/Process.h>
#include <linuxk/linux-emul.h>
#include <linuxk/lsync.h>
#include <linux/thread_info.h>
#include <linux/preempt.h>
#include <domain/assert.h>

/* This is the first driver code to run in a usb driver process.
   It sets up the .data and .bss sections in a vcsk. */

const uint32_t __rt_stack_pointer
   = LK_STACK_BASE + (1UL << LK_LGSTACK_AREA) - SIZEOF_THREAD_INFO;
const uint32_t __rt_unkept = 1;

extern void driver_main(void);

/* We run after dyndriverprotospace.
   which leaves KR_TEMP2 as the GPT to our address space,
   and KR_TEMP1 as the GPT to our stacks space.
   The caller of the constructor passed a USBInterface cap
   which is now in KR_ARG(0), and a resume key in KR_RETURN. */
int
main(void)
{
  result_t result;

  // Unpack some caps.
  result = capros_Node_getSlotExtended(KR_CONSTIT, KC_OSTREAM, KR_OSTREAM);
  if (result != RC_OK) {
    *((int *)0) = 0xbadbad77;	// FIXME
  }
  result = capros_Node_getSlotExtended(KR_CONSTIT, KC_LINUX_EMUL,
    KR_LINUX_EMUL);
  assert(result == RC_OK);	// FIXME
  result = capros_Node_getSlotExtended(KR_CONSTIT, KC_SLEEP, KR_SLEEP);
  assert(result == RC_OK);	// FIXME
  result = capros_Node_getSlotExtended(KR_CONSTIT, KC_DEVPRIVS, KR_DEVPRIVS);
  assert(result == RC_OK);	// FIXME
  /* Set I/O privileges. */
  result = capros_Process_setIOSpace(KR_SELF, KR_DEVPRIVS);
  assert(result == RC_OK);

  // Create the KEYSTORE supernode.
  result = capros_Node_getSlotExtended(KR_CONSTIT, KC_SNODECONSTR, KR_KEYSTORE);
  assert(result == RC_OK);	// FIXME
  result = capros_Constructor_request(KR_KEYSTORE, KR_BANK, KR_SCHED, KR_VOID,
                             KR_KEYSTORE);
  assert(result == RC_OK);	// FIXME
  result = capros_SuperNode_allocateRange(KR_KEYSTORE, LKSN_THREAD_PROCESS_KEYS,
                      LKSN_APP - 1);
  assert(result == RC_OK);	// FIXME
  // Populate it.
  capros_Node_swapSlotExtended(KR_KEYSTORE, LKSN_THREAD_PROCESS_KEYS+0,
                               KR_SELF, KR_VOID);
  capros_Node_swapSlotExtended(KR_KEYSTORE, LKSN_STACKS_GPT,
                               KR_TEMP1, KR_VOID);

  // Create the GPT for maps. 
  result = capros_SpaceBank_alloc1(KR_BANK, capros_Range_otGPT, KR_MAPS_GPT);
  assert(result == RC_OK);	// FIXME
  result = capros_GPT_setL2v(KR_MAPS_GPT, 17);
  assert(result == RC_OK);	// FIXME
  // Map it.
  result = capros_GPT_setSlot(KR_TEMP2, LK_MAPS_BASE / 0x400000, KR_MAPS_GPT);
  assert(result == RC_OK);	// FIXME

  preempt_count() = 0;

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

  maps_init();

  driver_main();
  assert(false);	// driver_main should not return!
  return 0;
}
