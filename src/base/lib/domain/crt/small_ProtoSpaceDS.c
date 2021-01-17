/*
 * Copyright (C) 2007, 2009, Strawberry Development Group.
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

#include <domain/ProtoSpaceDS.h>
#include <eros/cap-instr.h>
#include <idl/capros/GPT.h>
#include <idl/capros/SpaceBank.h>

extern char __data_start;
extern void end();

/* We need to free the stack.
   protospace_destroy_small could be called at any position in the stack,
   so we set the stack pointer to the
   base of the stack, and call protospace_destroy_small_2 with
   krProto, retCode, and __rt_stack_pages. */

void
protospace_destroy_small_2(uint32_t krProto, uint32_t retCode,
  uint32_t local_stack_pages)
{
  /* krProto could be KR_TEMP0, 1, or 2.
     Put it in KR_TEMP3 to ensure it's out of the way. */
  COPY_KEYREG(krProto, KR_TEMP3);

  /* WL - is there anything we can do if any of these invocations fail? */
  capros_Process_getAddrSpace(KR_SELF, KR_TEMP2);

  /* We are now running at the base of the stack.
     Delete all stack pages except the highest, which we are using. */
  
  while (1) {
    capros_GPT_getSlot(KR_TEMP2, capros_GPT_nSlots - local_stack_pages,
                       KR_TEMP1);
    if (local_stack_pages <= 1)
      break;
    capros_SpaceBank_free1(KR_BANK, KR_TEMP1);
    local_stack_pages--;
  }
  // The base stack page is in KR_TEMP1.

  /* We need to free all the data/bss pages.
     We can't use any global variables after this point.
     (That is why protospace_destroy_small loaded __rt_stack_pages
     and passed it to us as a parameter.)
     Even so, there is a problem.
     The first page probably also contains code, possibly this very code. 
     So, here we free all but the first page. telospace will free it. */

  uint32_t base = (uint32_t) &__data_start;
  uint32_t bound = (uint32_t) &end;
  
  /* Round the base down and the bound up to the nearest page
     boundary. */
  base &= - EROS_PAGE_SIZE;
  bound += EROS_PAGE_SIZE - 1;
  bound &= - EROS_PAGE_SIZE;

  while (1) {
    bound -= EROS_PAGE_SIZE;
    uint32_t slot = bound / EROS_PAGE_SIZE;
    
    capros_GPT_getSlot(KR_TEMP2, slot, KR_TEMP0);

    if (bound == base)
      break;

    capros_SpaceBank_free1(KR_BANK, KR_TEMP0);
  }
  /* Now we have freed all the data/bss pages except the first,
     which is in KR_TEMP0. */

  uint32_t w1_out;

  /* The following invocation replaces our own address space and
  changes our PC, therefore the code after the invocation is never executed. 
  w2_in is received in a register, and w1_out is never used. */
  capros_Process_swapAddrSpaceAndPC32Proto(KR_SELF, KR_TEMP3,
    0x400,	// well known telospace address
    retCode,	// w2_in
    &w1_out,	// won't be used
    KR_TEMP2);	// actually it's already in KR_TEMP2
  /* NOTREACHED */
}
