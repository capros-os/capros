/*
 * Copyright (C) 2003, Jonathan S. Shapiro.
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

/* Support code for recovery from a frame buffer fault. */
#include <eros/target.h>

#include <setjmp.h>
#include <string.h>
#include <eros/KeyConst.h>
#include <eros/Invoke.h>
#include <domain/Runtime.h>
#include <domain/ConstructorKey.h>
#include "../keeper/winsys-keeper.h"
#include <domain/domdbg.h>
#include <idl/capros/Process.h>

jmp_buf fbfault_jmp_buf;

extern void fbfault_trampoline();

#include "../winsyskeys.h"

unsigned 
fbfault_init(cap_t kr_keeper, cap_t kr_bank, cap_t kr_sched)
{
  Message msg;
  uint32_t result;

  /* Replace this with:
   *
   * Call the constructor, insert resulting start 
   * key in our keeper slot 
   */

  kprintf(KR_OSTREAM, "fbfault_init() calling constructor (slot=%u)...\n",
	  kr_keeper);
  result = constructor_request(kr_keeper, kr_bank, kr_sched, KR_VOID, 
			       kr_keeper);
  if (result != RC_OK)
    return result;

#if 1
  result = capros_Process_swapKeeper(KR_SELF, kr_keeper, KR_VOID);
  if (result != RC_OK)
    return result;
#endif

  /* IDEA: You build a specialized keeper that resets our PC to
     the address supplied here whenever it is informed of a memory 
     fault. */
  memset(&msg, 0, sizeof(msg));
  msg.snd_invKey = kr_keeper;
  msg.snd_code = OC_WINSYS_KEEPER_SETUP;
  msg.snd_w1 = (unsigned long) &fbfault_trampoline;

  kprintf(KR_OSTREAM, "fbfault_init() invoking pkeeper...\n");
  return CALL(&msg);
}

unsigned 
fbfault_received()
{
  return setjmp(fbfault_jmp_buf);
}
