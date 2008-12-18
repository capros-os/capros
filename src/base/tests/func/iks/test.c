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
#include <idl/capros/Node.h>
#include <idl/capros/IndexedKeyStore.h>

#include <idl/capros/Constructor.h>
#include <domain/Runtime.h>

#include <domain/domdbg.h>

#define KR_IKSC   KR_APP(0)
#define KR_OSTREAM  KR_APP(1)

#define KR_IKS    KR_APP(2)

#define KR_NODE     KR_APP(3)	// node for creating number keys

/* Bypass all the usual initialization. */
const uint32_t __rt_runtime_hook = 0;

#define ckOK \
  if (result != RC_OK) { \
    kdprintf(KR_OSTREAM, "Line %d result is 0x%08x!\n", __LINE__, result); \
  }

const char * n1 = "name1";
const char * n2 = "name2";

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

const unsigned int l2nSlots = capros_Node_l2nSlots;
#define sn(x,n) ((x) << ((n)*l2nSlots))

void
writeslot(uint32_t addr)
{
  result_t result;

  createNumberKey(addr, KR_TEMP0);
  result = capros_IndexedKeyStore_put(KR_IKS, KR_TEMP0, 4, (uint8_t *)&addr);
  if (result == RC_capros_Node_NoAddr)
    kdprintf(KR_OSTREAM, "Line %d NoAddr at 0x%08x!\n", __LINE__, addr);
  ckOK
}

void
delslot(uint32_t addr)
{
  result_t result;

  result = capros_IndexedKeyStore_delete(KR_IKS, 4, (uint8_t *)&addr);
  if (result == RC_capros_Node_NoAddr)
    kdprintf(KR_OSTREAM, "Line %d NoAddr at 0x%08x!\n", __LINE__, addr);
  ckOK
}

void
testslot(uint32_t addr)
{
  result_t result;
  unsigned long w0, w1, w2;

  result = capros_IndexedKeyStore_get(KR_IKS, 4, (uint8_t *)&addr, KR_TEMP0);
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
failslot(uint32_t addr)
{
  result_t result;
  result = capros_IndexedKeyStore_get(KR_IKS, 4, (uint8_t *)&addr, KR_TEMP0);
  if (result != RC_capros_IndexedKeyStore_NotFound)
    kdprintf(KR_OSTREAM, "Get slot 0x%x returned 0x%x, expecting failure!\n",
      addr, result);
}

int
main(void)
{
  result_t result;
  uint32_t isDiscrete;
  uint32_t a1 = 1234;
  uint32_t a2 = 2345;

  kprintf(KR_OSTREAM, "Start test.\n");

  result = capros_Constructor_isDiscreet(KR_IKSC, &isDiscrete);
  ckOK
  if (isDiscrete) {
    kprintf(KR_OSTREAM,
	    "Constructor alleges discretion.\n");
  } else {
    kdprintf(KR_OSTREAM,
	     "Constructor is not discreet.\n");
  }

  kprintf(KR_OSTREAM, "Creating iks\n");

  result = capros_SpaceBank_alloc1(KR_BANK, capros_Range_otNode, KR_NODE);
  ckOK

  /* we've now got all the pieces -- let the test begin! */

  result = capros_Constructor_request(KR_IKSC,
			       KR_BANK, KR_SCHED, KR_VOID,
			       KR_IKS);
  ckOK

  writeslot(a1);

  testslot(a1);
  failslot(a2);

  writeslot(a2);

  testslot(a1);
  testslot(a2);

  delslot(a1);

  failslot(a1);
  testslot(a2);

#if 0
  kprintf(KR_OSTREAM, "Destroying iks\n");

  result = capros_key_destroy(KR_IKS);
  ckOK
#endif

  kdprintf(KR_OSTREAM, "Done.\n");

  return 0;
}

