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
#include <eros/ProcessKey.h>
#include <eros/NodeKey.h>
#include <eros/machine/Registers.h>
#include <domain/domdbg.h>

#define ADDR1 0x40000

#include "meminval.h"

const uint32_t __rt_stack_pointer = 0x21000;

static uint8_t rcvData[EROS_PAGE_SIZE];
      
void
nodeStore(uint32_t krNode, uint32_t slot, uint32_t krFrom)
{
  uint32_t ret = node_swap(krNode, slot, krFrom, KR_VOID);
  assert(ret == RC_OK);
}

void
makeSeg(uint32_t krNode, uint16_t keyData)
{
  uint32_t ret = node_make_segment_key(krNode, keyData, 0/*perms*/, KR_TEMP);
  assert(ret == RC_OK);
}

int
main()
{
  int blss;
  uint32_t ret;
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

#if 0
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
#endif

    // Copy fault code and info to shared page:
    struct Registers * regs = (struct Registers *)&rcvData;
    shInfP->faultCode = regs->faultCode;
    shInfP->faultInfo = regs->faultInfo;

    // Repair proc1's address space.
    /* Don't repair any more than necessary, so we avoid triggering
    extra Depend entry invalidations. */
    blss = shInfP->blss;
    switch (blss) {
    case 1:
      nodeStore(KR_SEG17, 31, KR_PAGE);
      break;

    case 2:
    case 3:
    case 4:
      makeSeg(KR_SEG17 - 2 + blss, blss-1);
      nodeStore(KR_SEG17 - 1 + blss, 0, KR_TEMP);
      break;

    case 5:
      makeSeg(KR_SEG17 - 2 + blss, blss-1);
      ret = process_swap(KR_PROC1_PROCESS, ProcAddrSpace, KR_TEMP, KR_VOID);
      assert(ret == RC_OK);
      break;

    default:
      assert(false);
    }
#if 0
    int fromKey = KR_PAGE;
    int toNode = KR_SEG17;	// and consecutive key registers
    int slot = 31;	// slot containing stack page
    for (blss = 1; blss <= 4; blss++, toNode++) {
      ret = node_swap(toNode, slot, fromKey, KR_VOID);
      assert(ret == RC_OK);
      
      // set up for next iteration
      ret = node_make_segment_key(toNode, blss, 0/*perms*/, KR_TEMP);
      assert(ret == RC_OK);
      slot = 0;
      fromKey = KR_TEMP;
    }
    ret = process_swap(KR_PROC1_PROCESS, ProcAddrSpace, fromKey, KR_VOID);
    assert(ret == RC_OK);
#endif

    kprintf(KR_OSTREAM, "Keeper returning.\n");

    msg.snd_invKey = KR_RETURN;
  }

  return 0;
}
