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
#include <domain/Runtime.h>
#include <idl/capros/GPT.h>
#include <idl/capros/Node.h>
//#include <idl/capros/Constructor.h>
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

  // Unpack some caps.
  result = capros_Node_getSlotExtended(KR_CONSTIT, KC_OSTREAM, KR_OSTREAM);
  if (result != RC_OK) {
    /* This should not happen, but until KR_OSTREAM is there,
    we can't use assert. */
    *((int *)0) = 0xbadbad77;
  }

  finalResult = cmme_main();

  /* Tear down everything. */

  // Do we need to call the library to let it clean up?

  // Set up caps for destruction.
  result = capros_Process_getAddrSpace(KR_SELF, KR_TEMP3);
  assert(result == RC_OK);
  result = capros_GPT_getSlot(KR_TEMP3, LK_STACK_BASE / 0x400000, KR_TEMP2);
  assert(result == RC_OK);
  result = capros_GPT_getSlot(KR_TEMP3, LK_DATA_BASE / 0x400000, KR_TEMP0);
  assert(result == RC_OK);
  result = capros_GPT_getSlot(KR_TEMP2, 0, KR_TEMP1);
  assert(result == RC_OK);
  result = capros_Node_getSlotExtended(KR_CONSTIT, KC_INTERPRETERSPACE,
             KR_ARG(0));
  assert(result == RC_OK);
  InterpreterDestroy(KR_ARG(0), KR_TEMP3, finalResult);
  assert(false);	// InterpreterDestroy should not return!
  return 0;
}
