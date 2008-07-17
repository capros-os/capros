/*
 * Copyright (C) 2008, Strawberry Development Group.
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

/* Test aging of objects.
*/

#include <stdint.h>
#include <string.h>
#include <eros/target.h>
#include <eros/Invoke.h>
#include <eros/fls.h>
#include <idl/capros/SpaceBank.h>
#include <idl/capros/Constructor.h>
#include <idl/capros/SuperNode.h>
#include <idl/capros/Page.h>
#include <idl/capros/Node.h>
#include <idl/capros/Number.h>
#include <idl/capros/SysTrace.h>

#include <domain/Runtime.h>
#include <domain/domdbg.h>
#include <domain/assert.h>

#define KR_OSTREAM	KR_APP(0)
#define KR_SNODEC	KR_APP(1)
#define KR_SYSTRACE	KR_APP(2)
#define KR_SNODE	KR_APP(3)
#define KR_CLONE_PAGE	KR_APP(4)

const uint32_t __rt_stack_pointer = 0x20000;
const uint32_t __rt_unkept = 1;

#define ckOK \
  if (result != RC_OK) { \
    kdprintf(KR_OSTREAM, "Line %d result is 0x%08x!\n", __LINE__, result); \
  }

capros_Number_value numVal = {
  .value = {
    [0] = 0,
    [1] = 0,
    [2] = 0
  }   
};

int
main(void)
{
  result_t result;
  uint32_t obType = capros_Range_otNode;
  unsigned int numCreated;
  unsigned int numToCreate = 13000;

  kprintf(KR_OSTREAM, "Starting.\n");

  result = capros_Constructor_request(KR_SNODEC,
                               KR_BANK, KR_SCHED, KR_VOID,
                               KR_SNODE);
  ckOK

  result = capros_SuperNode_allocateRange(KR_SNODE, 0, numToCreate -1);
  ckOK

  result = capros_SpaceBank_alloc1(KR_BANK, capros_Range_otPage,
             KR_CLONE_PAGE);
  ckOK

  for (numCreated = 0; numCreated < numToCreate; numCreated++) {
    if ((numCreated % 100) == 0) {
      kprintf(KR_OSTREAM, "Created %d\n", numCreated);
      capros_SysTrace_CheckConsistency(KR_SYSTRACE);
    }

    result = capros_SpaceBank_alloc1(KR_BANK, obType, KR_TEMP0);
    if (result == RC_capros_SpaceBank_LimitReached) {
      kprintf(KR_OSTREAM, "Reached bank limit, created %d\n", numCreated);
      break;
    } else {
      ckOK
    }

    // Dirty the object:
    switch (obType) {
    default: ;
      assert(false);

    case capros_Range_otPage:
      result = capros_Page_clone(KR_CLONE_PAGE, KR_TEMP0);
      ckOK
      break;

    case capros_Range_otNode:
      result = capros_Node_writeNumber(KR_TEMP0, 0, numVal);
      ckOK
      break;
    }

    result = capros_Node_swapSlotExtended(KR_SNODE, numCreated, KR_TEMP0,
               KR_VOID);
    if (result == RC_capros_Node_NoAddr)
      kdprintf(KR_OSTREAM, "Line %d NoAddr at 0x%08x!\n", __LINE__, numCreated);
    ckOK
  }

  kprintf(KR_OSTREAM, "Done.\n");

  return 0;
}

