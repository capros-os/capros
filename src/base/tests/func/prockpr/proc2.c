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

/*
 * Proc2 -- this process is the keeper of proc1.
 */

#include <string.h>
#include <eros/target.h>
#include <eros/Invoke.h>
#include <domain/Runtime.h>
#include <idl/capros/Process.h>
#include <domain/domdbg.h>

/* It is intended that this should be a small space domain */
const uint32_t __rt_stack_pages = 0;
const uint32_t __rt_stack_pointer = 0x21000;
const uint32_t __rt_unkept = 1;	/* do not mess with keeper */

#define KR_OSTREAM 10

static uint8_t rcvData[EROS_PAGE_SIZE];

int
main()
{
  Message msg;
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

  msg.rcv_key0 = KR_ARG(0);
  msg.rcv_key1 = KR_ARG(1);
  msg.rcv_key2 = KR_ARG(2);
  msg.rcv_rsmkey = KR_RETURN;
  msg.rcv_data = &rcvData;
  msg.rcv_limit = sizeof(rcvData);;

  msg.snd_invKey = KR_VOID;
  for (;;) {
    memset(&rcvData, 0, sizeof(rcvData));
    RETURN(&msg);

    kprintf(KR_OSTREAM, "Keeper called.\n");
    kprintf(KR_OSTREAM, "w0=%d(0x%x), w1=%d(0x%x), w2=%d(0x%x), w3=%d(0x%x)\n",
            msg.rcv_code, msg.rcv_code, msg.rcv_w1, msg.rcv_w1,
            msg.rcv_w2, msg.rcv_w2, msg.rcv_w3, msg.rcv_w3);
    kprintf(KR_OSTREAM, "keyData=%d, %d bytes sent\n",
            msg.rcv_keyInfo, msg.rcv_sent);
    int i;
    for (i = 0; i < msg.rcv_sent; i += 16) {
      kprintf(KR_OSTREAM, "0x%08x 0x%08x 0x%08x 0x%08x\n",
              *(uint32_t *)&rcvData[i],
              *(uint32_t *)&rcvData[i+4],
              *(uint32_t *)&rcvData[i+8],
              *(uint32_t *)&rcvData[i+12] );
    }

    // Skip over the offending instruction:
    struct capros_Process_CommonRegisters32 * regs
      = (struct capros_Process_CommonRegisters32 *)&rcvData;
    int increment;
    switch (regs->arch) {
    case capros_Process_ARCH_ARMProcess: increment = 4; break;
    case capros_Process_ARCH_I386:       increment = 5; break;
    default: kprintf(KR_OSTREAM, "Unknown architecture.\n");
      increment = 0;
    }
    regs->pc += increment;
    capros_Process_setRegisters32(KR_ARG(2), *regs);

    kprintf(KR_OSTREAM, "Keeper returning.\n");

    msg.snd_invKey = KR_RETURN;
  }

  return 0;
}
