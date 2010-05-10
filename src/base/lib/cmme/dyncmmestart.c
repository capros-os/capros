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

#include <stddef.h>
#include <eros/target.h>
#include <eros/Invoke.h>
#include <eros/machine/cap-instr.h>
#include <domain/Runtime.h>
#include <idl/capros/GPT.h>
#include <idl/capros/Node.h>
#include <idl/capros/SpaceBank.h>
#include <idl/capros/Process.h>
#include <domain/assert.h>
#include <domain/CMME.h>
#include <domain/InterpreterDestroy.h>

#define dbg_alloc 0x01

/* Following should be an OR of some of the above */
#define dbg_flags   ( 0u )

#define DEBUG(x) if (dbg_##x & dbg_flags)

/* This is the first code to run in a CMTE process built using a constructor.
   It sets up the .data and .bss sections in a vcsk. */

// Leave room at the top of the stack for a void *,
// used by CMTE.
const uint32_t __rt_stack_pointer
  = (LK_STACK_BASE + LK_STACK_AREA- sizeof(void *));

extern result_t cmme_main(void);

/* We run after dyncmteprotospace.
   which leaves KR_TEMP3 as the GPT to our address space,
   and KR_TEMP2 as the GPT to our stacks space.
   The caller of the constructor passed a cap
   which is now in KR_ARG(0), and a resume key in KR_RETURN. */
int
main(void)
{
  result_t result;
  result_t finalResult;

  result = capros_Node_getSlotExtended(KR_CONSTIT, KC_OSTREAM, KR_OSTREAM);
  if (result != RC_OK) {
    /* This should not happen, but until KR_OSTREAM is there,
    we can't use assert. */
    *((int *)0) = 0xbadbad77;
  }

  // At this point we have a single-page-sized stack.
  // Allocate any additional stack.
  extern unsigned long __rt_stack_size;
  if (__rt_stack_size > EROS_PAGE_SIZE) {
    unsigned long stackSize = EROS_PAGE_SIZE;
    if (__rt_stack_size > LK_STACK_AREA)
      __rt_stack_size = LK_STACK_AREA;	// biggest we support

    // Get node with l2v==17.
    // (It should already be in KR_TEMP2, but this is cheap insurance.)
    result = capros_GPT_getSlot(KR_TEMP3, LK_STACK_BASE / 0x400000, KR_TEMP2);
    assert(result == RC_OK);
    // Get existing stack page. (Careful - we are running on this stack.)
    result = capros_GPT_getSlot(KR_TEMP2, 0, KR_TEMP0);
    assert(result == RC_OK);
    result = capros_Memory_makeGuarded(KR_TEMP0, 0, KR_TEMP0);
    assert(result == RC_OK);
    // Insert a node to hold the page keys:
    result = capros_SpaceBank_alloc1(KR_BANK, capros_Range_otGPT, KR_TEMP1);
    assert(result == RC_OK);	// TODO handle
    result = capros_GPT_setL2v(KR_TEMP1, EROS_PAGE_LGSIZE);
    assert(result == RC_OK);
    result = capros_GPT_setSlot(KR_TEMP1,
               (LK_STACK_AREA - EROS_PAGE_SIZE) / EROS_PAGE_SIZE, KR_TEMP0);
    assert(result == RC_OK);
    result = capros_GPT_setSlot(KR_TEMP2, 0, KR_TEMP1);
    assert(result == RC_OK);

    while (stackSize < __rt_stack_size) {
      result = capros_SpaceBank_alloc1(KR_BANK, capros_Range_otPage, KR_TEMP0);
      assert(result == RC_OK);	// TODO handle
      stackSize += EROS_PAGE_SIZE;
      result = capros_GPT_setSlot(KR_TEMP1,
                 (LK_STACK_AREA - stackSize) / EROS_PAGE_SIZE, KR_TEMP0);
      assert(result == RC_OK);
    }
  }

  finalResult = cmme_main();

  /* Tear down everything. */

  result = capros_Process_getAddrSpace(KR_SELF, KR_TEMP3);
  assert(result == RC_OK);
  result = capros_GPT_getSlot(KR_TEMP3, LK_STACK_BASE / 0x400000, KR_TEMP2);
  assert(result == RC_OK);
  result = capros_GPT_getSlot(KR_TEMP2, 0, KR_TEMP1);
  assert(result == RC_OK);
  // Deallocate any stack we allocated.
  if (__rt_stack_size > EROS_PAGE_SIZE) {
    unsigned long stackSize = __rt_stack_size;
    while (stackSize > EROS_PAGE_SIZE) {
      result = capros_GPT_getSlot(KR_TEMP1,
                 (LK_STACK_AREA - stackSize) / EROS_PAGE_SIZE, KR_TEMP0);
      assert(result == RC_OK);
      result = capros_SpaceBank_free1(KR_BANK, KR_TEMP0);
      assert(result == RC_OK);
      stackSize -= EROS_PAGE_SIZE;
    }
    result = capros_GPT_getSlot(KR_TEMP1,
               (LK_STACK_AREA - EROS_PAGE_SIZE) / EROS_PAGE_SIZE, KR_TEMP0);
    assert(result == RC_OK);
    result = capros_Memory_makeGuarded(KR_TEMP0,
               LK_STACK_AREA - EROS_PAGE_SIZE, KR_TEMP0);
    assert(result == RC_OK);
    result = capros_GPT_setSlot(KR_TEMP2, 0, KR_TEMP0);
    assert(result == RC_OK);
    result = capros_SpaceBank_free1(KR_BANK, KR_TEMP1);
    assert(result == RC_OK);
    COPY_KEYREG(KR_TEMP0, KR_TEMP1);	// move page to KR_TEMP1
  }

  result = capros_GPT_getSlot(KR_TEMP3, LK_DATA_BASE / 0x400000, KR_TEMP0);
  assert(result == RC_OK);
  // KR_TEMP0, KR_TEMP1, KR_TEMP2, and KR_TEMP3 are now set up for destruction.
  result = capros_Node_getSlotExtended(KR_CONSTIT, KC_INTERPRETERSPACE,
             KR_ARG(0));
  assert(result == RC_OK);
  InterpreterDestroy(KR_ARG(0), KR_TEMP3, finalResult);
  assert(false);	// InterpreterDestroy should not return!
  return 0;
}
