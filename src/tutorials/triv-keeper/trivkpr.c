/*
 * Copyright (C) 1998, 1999, Jonathan S. Shapiro.
 * Copyright (C) 2009, Strawberry Development Group.
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


/*
 * TrivKpr -- a trivial domain keeper, just (about) smart enough to
 * dump the information it was passed by the kernel onto the
 * console. It makes no attempt to fix the busted process.
 *
 * The keeper receives (from the kernel) a DomCtlInfo structure, plus
 * a fault key to the process and also a process key to the process.
 */

#include <stddef.h>
#include <eros/target.h>
#include <eros/Invoke.h>
#include <idl/capros/Node.h>
#include <domain/domdbg.h>
#include <domain/Runtime.h>
/* #include <eros/DomCtlInfo.h> */
#ifdef __REGISTERS_I486_H__
#error "registers anti-macro defined"
#endif
#include <idl/capros/Process.h>
#include "constituents.h"

#define KR_OSTREAM     KR_APP(0)

#define KR_RK0         KR_ARG(0)
#define KR_RK1         KR_ARG(1)
#define KR_PROCESS     KR_ARG(2)

typedef struct DomCtlInfo32_s DomCtlInfo;

int
ProcessRequest(Message *msg)
{
  /* We don't bother with the usual request dispatch logic here,
   * because we are only ever going to receive one request: the
   * "somebody faulted" request which is synthesized by the kernel.
   *
   * Also, this keeper never exits!
   */

  struct capros_Process_CommonRegisters32 * regs
           = (struct capros_Process_CommonRegisters32 *) msg->rcv_data;

  kprintf(KR_OSTREAM, "faultCode: 0x%08x\n", regs->faultCode);
  kprintf(KR_OSTREAM, "faultInfo: 0x%08x\n", regs->faultInfo);
  kprintf(KR_OSTREAM, "fault PC:  0x%08x\n", regs->pc);
  kprintf(KR_OSTREAM, "fault SP:  0x%08x\n", regs->sp);

  /* If (hypothetically), we had "fixed" the fault condition, then we
   * should:
   *
   *   msg.snd_invKey = KR_RETURN;
   *   msg.snd_code   = RC_OK;
   *
   * The first causes the process to be restarted. The second resets
   * the fault code within the process.
   */

  /* Return 1 so that we will continue processing further requests: */
  return 1;
}

struct capros_Process_CommonRegisters32 rcvData;


int
main ()
{
  Message msg;

  capros_Node_getSlot(KR_CONSTIT, KC_OSTREAM, KR_OSTREAM);

  msg.snd_invKey = KR_VOID;
  msg.snd_key0 = KR_VOID;
  msg.snd_key1 = KR_VOID;
  msg.snd_key2 = KR_VOID;
  msg.snd_rsmkey = KR_VOID;
  msg.snd_data = 0;
  msg.snd_len = 0;
  msg.snd_code = 0;
  msg.snd_w1 = 0;
  msg.snd_w2 = 0;
  msg.snd_w3 = 0;
     
  msg.rcv_key0 = KR_RK0;
  msg.rcv_key1 = KR_RK1;
  msg.rcv_key2 = KR_PROCESS;
  msg.rcv_rsmkey = KR_RETURN;
  msg.rcv_data = &rcvData;
  msg.rcv_code = 0;
  msg.rcv_w1 = 0;
  msg.rcv_w2 = 0;
  msg.rcv_w3 = 0;
     
  kprintf(KR_OSTREAM, "Keeper is initialized\n");

  do {
    msg.rcv_limit = sizeof(rcvData);
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
