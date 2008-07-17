/*
 * Copyright (C) 2007, 2008, Strawberry Development Group
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
#include <idl/capros/Process.h>
#include <idl/capros/Sleep.h>
#include <domain/domdbg.h>
#include <idl/capros/arch/arm/SysTrace.h>

#define KR_VOID 0
#define KR_OSTREAM 10
#define KR_SYSTRACE 11
#define KR_KEEPER_PROCESS 12

/* It is intended that this should be a small space domain */
const uint32_t __rt_stack_pages = 0;
const uint32_t __rt_stack_pointer = 0x21000;
const uint32_t __rt_unkept = 1;	/* do not mess with keeper */

Message msg;

int
main(void)
{

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
  capros_Process_makeResumeKey(KR_KEEPER_PROCESS, KR_KEEPER_PROCESS);
  msg.snd_invKey = KR_KEEPER_PROCESS;
  msg.snd_code = RC_OK;
  SEND(&msg);

  kprintf(KR_OSTREAM, "About to fault\n");

  int volatile * x = (int *)0xbadbad00;
  *x;

  kprintf(KR_OSTREAM, "After fault\nDone.\n");

  return 0;
}
