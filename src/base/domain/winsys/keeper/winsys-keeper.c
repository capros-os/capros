/*
 * Copyright (C) 2003, Jonathan S. Shapiro.
 * Copyright (C) 2007, 2009, Strawberry Development Group.
 *
 * This file is part of the CapROS Operating System,
 * and is derived from the EROS Operating System.
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


/* winsys-keeper. This is a specialized keeper that acts in cahoots
 * with the window system to provide recovery from frame buffer
 * faults.
 */

#include <stddef.h>
#include <eros/target.h>
#include <eros/Invoke.h>
#include <eros/NodeKey.h>

#include <idl/capros/key.h>
#include <idl/capros/Process.h>
#include <idl/capros/ProcessKeeper.h>

#include <domain/domdbg.h>
#include <domain/Runtime.h>

#include "winsys-keeper.h"
#include "constituents.h"

#define KR_PROCESS     KR_ARG(0)

#define KR_OSTREAM     KR_APP(0)
#define KR_START       KR_APP(1)

static unsigned long recoveryAddr;

/* FIX: Need an opcode to set these values.  For now, hardcode them to
   known max acceptable range for winsys clients. */
#define _128MB_    (0x08000000u)
#define BOUNDS_MIN (18 * _128MB_)
#define BOUNDS_MAX (23 * _128MB_)

static bool
bounds_check(uint32_t fault_addr)
{
  return (fault_addr >= BOUNDS_MIN) && (fault_addr <= BOUNDS_MAX);
}

int
ProcessRequest(Message *msg)
{
  /* This is not really much of a keeper. */

  kprintf(KR_OSTREAM, "winsys-keeper INVOKED! with opcode = %u (0x%08x)\n",  
	  msg->rcv_code, msg->rcv_code);

  switch(msg->rcv_code) {
  case OC_capros_ProcessKeeper_fault:
    {
      struct capros_Process_CommonRegisters32 * regs
        = (struct capros_Process_CommonRegisters32 *) msg->rcv_data;

      kprintf(KR_OSTREAM, "**** winsys-keeper: PROCFAULT! Regs:\n");

      kprintf(KR_OSTREAM, "\tarch      = 0x%08x\n\tlen       = 0x%08x\n"
	   "\tpc        = 0x%08x\n\tsp        = 0x%08x\n\tfaultCode = 0x%08x\n"
	   "\tfaultInfo = 0x%08x\n\tdomFlags  = 0x%08x\n",
	      regs->arch, regs->len, regs->pc, regs->sp, regs->faultCode,
	      regs->faultInfo, regs->procFlags);

      /* Perform bounds check on the fault address. If outside the
      acceptable range, return to KR_VOID (so at least winsys won't
      get caught in an infinite loop!) */
      if (!bounds_check(regs->faultInfo)) {
	kprintf(KR_OSTREAM, "  #### fault address is out of bounds!\n");
	msg->snd_invKey = KR_VOID;
	return 1;
      }

      /* Whack the winsys PC to point to the recovery trampoline: */
      regs->pc = recoveryAddr;
      regs->faultCode = capros_Process_FC_NoFault;
      capros_Process_setRegisters32(KR_PROCESS, *regs);

      msg->snd_invKey = KR_RETURN;
      msg->snd_w1 = 0;		/* resume the victim */
      msg->snd_code = RC_OK;
      break;
    }
  case OC_WINSYS_KEEPER_SETUP:
    {

      kprintf(KR_OSTREAM, "winsys-keeper doing KEEPER_SETUP.\n");

      recoveryAddr = msg->rcv_w1;
      msg->snd_invKey = KR_RETURN;
      msg->snd_code = RC_OK;
      break;
    }
  default:
    kprintf(KR_OSTREAM, "winsys-keeper: unknown request!\n");
    msg->snd_code = RC_capros_key_UnknownRequest;
    break;
  }

  /* Return 1 so that we will continue processing further requests: */
  return 1;
}

int
main ()
{
  Message msg;

  struct Registers rcvData;

  node_extended_copy(KR_CONSTIT, KC_OSTREAM, KR_OSTREAM);

  kprintf(KR_OSTREAM, "winsys-keeper says HI!\n");

  /* Make a start key to return to constructor */
  capros_Process_makeStartKey(KR_SELF, 0, KR_START);

  msg.snd_invKey = KR_RETURN;
  msg.snd_key0 = KR_START;
  msg.snd_key1 = KR_VOID;
  msg.snd_key2 = KR_VOID;
  msg.snd_data = 0;
  msg.snd_len = 0;
  msg.snd_code = 0;
  msg.snd_w1 = 0;
  msg.snd_w2 = 0;
  msg.snd_w3 = 0;
     
  msg.rcv_key0 = KR_PROCESS;
  msg.rcv_key1 = KR_VOID;
  msg.rcv_key2 = KR_VOID;
  msg.rcv_data = &rcvData;
  msg.rcv_code = 0;
  msg.rcv_w1 = 0;
  msg.rcv_w2 = 0;
  msg.rcv_w3 = 0;
     
  do {
    msg.rcv_rsmkey = KR_RETURN;
    msg.rcv_limit = sizeof (struct Registers);
    RETURN(&msg);

    /* If the ProcessRequest routine is actually able to correct the
     * fault, it should override msg.snd_invKey with KR_RETURN. If we
     * do not return to the faulter, the faulter will remain "stuck"
     * in the waiting state, showing in its fault code and fault info
     * pseudo-registers the fault code and fault info that the kernel
     * just delivered to us (on its behalf).
     *
     * Note that we don't have the authority to kill the process in
     * the absence of prior knowledge about its runtime environment.
     */
    msg.snd_invKey = KR_VOID;
  } while ( ProcessRequest(&msg) );

  return 0;
}
