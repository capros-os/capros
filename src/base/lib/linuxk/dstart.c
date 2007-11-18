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

#include <eros/target.h>
#include <eros/Invoke.h>
#include <domain/Runtime.h>
#include <idl/capros/GPT.h>
#include <idl/capros/Constructor.h>
#include <linuxk/linux-emul.h>
#include <linux/thread_info.h>

/* This is the first code to run in a driver process.
   It sets up the .data and .bss sections in a vcsk. */

extern NORETURN void _start(void);

/* Our stack pointer is set up in the image file.
   The following is for _start, which will reset the stack pointer. */
void * __rt_stack_pointer = (void *)(0x420000 - SIZEOF_THREAD_INFO);
uint32_t __rt_unkept = 1;

/* When called, KR_TEMP1 has a constructor builder key to the VCSK
   for the data section,
   and KR_TEMP2 has the GPT to our address space. */
void
driver_start(void)
{
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

  _start();
}
