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

#include <eros/target.h>
#include <eros/Invoke.h>
#include <idl/capros/Number.h>
#include <idl/capros/SpaceBank.h>
#include <idl/capros/SuperNode.h>

#include <idl/capros/Constructor.h>
#include <domain/Runtime.h>

#include <domain/domdbg.h>

#define KR_SNODEC   KR_APP(0)
#define KR_OSTREAM  KR_APP(1)

#define KR_SNODE    KR_APP(2)

#define KR_NODE     KR_APP(3)	// node for creating number keys

const uint32_t __rt_stack_pointer = 0x20000;
const uint32_t __rt_unkept = 1;

#define ckOK \
  if (result != RC_OK) { \
    kdprintf(KR_OSTREAM, "Line %d result is 0x%08x!\n", __LINE__, result); \
  }

const unsigned int l2nSlots = capros_Node_l2nSlots;
#define sn(x,n) ((x) << ((n)*l2nSlots))

void
createNumberKey(uint32_t number, cap_t krOut)
{
  result_t result;
  const capros_Number_value val = {{0,0,number}};

  result = capros_Node_writeNumber(KR_NODE, 0, val);
  ckOK

  result = capros_Node_getSlot(KR_NODE, 0, krOut);
  ckOK
}

capros_Node_extAddr_t
writeslot(capros_Node_extAddr_t addr)
{
  result_t result;

  createNumberKey(addr, KR_TEMP0);
  result = capros_Node_swapSlotExtended(KR_SNODE, addr, KR_TEMP0, KR_VOID);
  if (result == RC_capros_Node_NoAddr)
    kdprintf(KR_OSTREAM, "Line %d NoAddr at 0x%08x!\n", __LINE__, addr);
  ckOK
  return addr;
}

void
testslot(capros_Node_extAddr_t addr)
{
  result_t result;
  unsigned long w0, w1, w2;

  result = capros_Node_getSlotExtended(KR_SNODE, addr, KR_TEMP0);
  if (result == RC_capros_Node_NoAddr)
    kdprintf(KR_OSTREAM, "Line %d NoAddr at 0x%08x!\n", __LINE__, addr);
  ckOK
  result = capros_Number_get(KR_TEMP0, &w0, &w1, &w2);
  ckOK
  if (w2 != addr)
    kdprintf(KR_OSTREAM, "Line %d expect 0x%x got 0x%x!\n",
             __LINE__, addr, w2);
}

void
failslot(capros_Node_extAddr_t addr)
{
  result_t result;
  result = capros_Node_getSlotExtended(KR_SNODE, addr, KR_TEMP0);
  if (result != RC_capros_Node_NoAddr)
    kdprintf(KR_OSTREAM, "Get slot 0x%x returned 0x%x, expecting failure!\n",
      addr, result);
}

int
main(void)
{
  result_t result;
  uint32_t isDiscrete;
  capros_Node_extAddr_t first, last;
  capros_Node_extAddr_t a1,a2,a3,a4,a5;

  result = capros_Constructor_isDiscreet(KR_SNODEC, &isDiscrete);
  ckOK
  if (isDiscrete) {
    kprintf(KR_OSTREAM,
	    "Constructor alleges discretion.\n");
  } else {
    kdprintf(KR_OSTREAM,
	     "Constructor is not discreet.\n");
  }

  kprintf(KR_OSTREAM, "Creating supernode\n");

  result = capros_SpaceBank_alloc1(KR_BANK, capros_Range_otNode, KR_NODE);
  ckOK

  /* we've now got all the pieces -- let the test begin! */

  result = capros_Constructor_request(KR_SNODEC,
			       KR_BANK, KR_SCHED, KR_VOID,
			       KR_SNODE);
  ckOK

  first = sn(1,1) + sn(5,0);
  last = first;
  result = capros_SuperNode_allocateRange(KR_SNODE, first, last);
  ckOK
  a1 = writeslot(first);

  // Slots 0-31 should now be inaccessible.
  failslot(7);

  first = sn(1,4) + sn(1,3) + sn(3,2) + sn(16,1) + sn(0,0);
  last = first;
  result = capros_SuperNode_allocateRange(KR_SNODE, first, last);
  ckOK
  a2 = writeslot(first);

  // Test inserting an intermediate node.
  first = sn(1,4) + sn(1,3) + sn(5,2) + sn(16,1) + sn(31,0);
  last = first +1;
  result = capros_SuperNode_allocateRange(KR_SNODE, first, last);
  ckOK
  a3 = writeslot(first);
  a4 = writeslot(last);

  first = sn(3,6);
  last = first;
  result = capros_SuperNode_allocateRange(KR_SNODE, first, last);
  ckOK
  a5 = writeslot(first);

  testslot(a1);
  testslot(a2);
  testslot(a3);
  testslot(a4);
  testslot(a5);

  first = a5;
  last = first + capros_Node_nSlots - 1;
  result = capros_SuperNode_deallocateRange(KR_SNODE, first, last);
  ckOK

  testslot(a1);
  testslot(a2);
  testslot(a3);
  testslot(a4);
  failslot(a5);

  first = (a3 | (capros_Node_nSlots - 1)) - (capros_Node_nSlots - 1);
  last = first + capros_Node_nSlots - 1;
  result = capros_SuperNode_deallocateRange(KR_SNODE, first, last);
  ckOK

  testslot(a1);
  testslot(a2);
  failslot(a3);
  testslot(a4);
  failslot(a5);

  result = capros_key_destroy(KR_SNODE);
  ckOK

  kprintf(KR_OSTREAM, "Done.\n");

  return 0;
}

