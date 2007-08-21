/*
 * Copyright (C) 2007, Strawberry Development Group
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
#include <domain/Runtime.h>
#include <eros/ProcessKey.h>
#include <eros/ProcessState.h>
#include <idl/capros/Sleep.h>
#include <idl/capros/GPT.h>
#include <domain/domdbg.h>

#define KR_SYSTRACE 11
#define KR_KEEPER_PROCESS 18

// #define LARGE_SPACE

#ifdef LARGE_SPACE
#define ADDR1 0x7000000
#else
#define ADDR1 0x1c000
#endif

#include "meminval.h"

const uint32_t __rt_stack_pointer = 0x20000;

Message msg;

int
main(void)
{
  int blss;
  uint32_t ret;

  msg.snd_key0 = KR_VOID;
  msg.snd_key1 = KR_VOID;
  msg.snd_key2 = KR_VOID;
  msg.snd_rsmkey = KR_VOID;
  msg.snd_len = 0;

  msg.rcv_key0 = KR_VOID;
  msg.rcv_key1 = KR_VOID;
  msg.rcv_key2 = KR_VOID;
  msg.rcv_rsmkey = KR_VOID;
  msg.rcv_limit = 0;		/* no data returned */

  /* Get keeper process started. */
  process_make_fault_key(KR_KEEPER_PROCESS, KR_KEEPER_PROCESS);
  msg.snd_invKey = KR_KEEPER_PROCESS;
  msg.snd_code = RC_OK;
  SEND(&msg);

  // Void a key at each level of the memory tree.

  int toNode = KR_SEG17;	// and consecutive key registers
  int slot = 31;	// slot containing stack page
  for (blss = 1; blss <= 5; blss++, toNode++) {
    shInfP->blss = blss;
    // Clear info in shared page.
    shInfP->faultCode = 0;
    shInfP->faultInfo = 0;

    kprintf(KR_OSTREAM, "Voiding blss %d\n", blss);

    if (blss < 5) {
      ret = capros_GPT_setSlot(toNode, slot, KR_VOID);
      assert(ret == RC_OK);
    } else {	// blss == 5
      ret = process_swap(KR_PROC1_PROCESS, ProcAddrSpace, KR_VOID, KR_VOID);
      assert(ret == RC_OK);
    }

    // Should have faulted. Keeper sets info in shared page.
    assert(shInfP->faultCode == capros_Process_FC_InvalidAddr);
    if (slot == 0)
      assert((shInfP->faultInfo & 0xfffe0000) == 0);
    else
      assert((shInfP->faultInfo & 0xfffff000) == 0x1f000);

    // set up for next iteration
    slot = 0;
  }

  // Make a key read-only at each level of the memory tree.

  toNode = KR_SEG17;	// and consecutive key registers
  slot = 31;	// slot containing stack page
  for (blss = 1; blss <= 5; blss++, toNode++) {
    shInfP->blss = blss;
    // Clear info in shared page.
    shInfP->faultCode = 0;
    shInfP->faultInfo = 0;

    kprintf(KR_OSTREAM, "Making blss %d readonly\n", blss);

    // Make the read-only key.
    ret = capros_Memory_reduce(toNode -1,
               capros_Memory_readOnly | capros_Memory_opaque, KR_TEMP);
    assert(ret == RC_OK);

    if (blss < 5) {
      ret = capros_GPT_setSlot(toNode, slot, KR_TEMP);
      assert(ret == RC_OK);
    } else {	// blss == 5
      ret = process_swap(KR_PROC1_PROCESS, ProcAddrSpace, KR_TEMP, KR_VOID);
      assert(ret == RC_OK);
    }

    // Should have faulted. Keeper sets info in shared page.
    assert(shInfP->faultCode == capros_Process_FC_AccessViolation);
    if (slot == 0)
      assert((shInfP->faultInfo & 0xfffe0000) == 0);
    else
      assert((shInfP->faultInfo & 0xfffff000) == 0x1f000);

    // set up for next iteration
    slot = 0;
  }

  kprintf(KR_OSTREAM, "Done\n");

  return 0;
}
