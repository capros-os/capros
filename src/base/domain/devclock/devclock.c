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

/* DevClock -- Controls hardware clocks.
 */

#include <eros/target.h>
#include <eros/Invoke.h>

#include <idl/capros/key.h>
#include <idl/capros/Node.h>
#include <idl/capros/Number.h>
#include <idl/capros/DevClock.h>

#include <domain/domdbg.h>
#include <domain/Runtime.h>

#define __raw_readl(a) (*(volatile unsigned long *)(a))
#define __raw_writel(v,a) (*(volatile unsigned long *)(a) = (v))

/* Memory:
  0: nothing
  0x1000: code
  0x20000: the page containing the device register
  0x21000: nothing (to guard against stack overflow)
  0x22000: stack */

const uint32_t devRegPageAddr = 0x20000;
const uint32_t __rt_stack_pointer = 0x23000;

uint32_t __rt_unkept = 1;

#define KR_PARAMS KR_APP(0)
#define KR_OSTREAM KR_APP(1)

int
main(void)
{
  int clients = 0;	/* Number of enables minus number of disables */
  unsigned long rate;	// clock rate in Hz
  uint32_t enable_reg;
  uint32_t enable_mask;

  Message msg;
  
  capros_Number_get(KR_PARAMS, &rate, &enable_reg, &enable_mask);

  bool hasEnableReg = enable_reg != 0;
  enable_reg &= EROS_PAGE_MASK;
  enable_reg |= devRegPageAddr;	// local address of enable register

  msg.snd_invKey = KR_VOID;
  msg.snd_key0 = KR_VOID;
  msg.snd_key1 = KR_VOID;
  msg.snd_key2 = KR_VOID;
  msg.snd_rsmkey = KR_VOID;
  msg.snd_len = 0;
  msg.snd_code = 0;
  msg.snd_w1 = 0;
  msg.snd_w2 = 0;
  msg.snd_w3 = 0;

  msg.rcv_key0 = KR_ARG(0);
  msg.rcv_key1 = KR_ARG(1);
  msg.rcv_key2 = KR_ARG(2);
  msg.rcv_rsmkey = KR_RETURN;
  msg.rcv_limit = 0;
  
  // kdprintf(KR_OSTREAM, "devclock: accepting requests\n");

  for(;;) {
    RETURN(&msg);

    // Defaults for reply:
    msg.snd_invKey = KR_RETURN;
    msg.snd_code = RC_OK;
    msg.snd_w1 = 0;
    msg.snd_w2 = 0;
    msg.snd_w3 = 0;

    switch (msg.rcv_code) {
    case OC_capros_DevClock_enable:
      if (!clients++ && hasEnableReg) {
        uint32_t oldval = __raw_readl(enable_reg);
        __raw_writel(oldval | enable_mask, enable_reg);
      }
      break;

    case OC_capros_DevClock_disable:
      if (!--clients && hasEnableReg) {
        uint32_t oldval = __raw_readl(enable_reg);
        __raw_writel(oldval & ~enable_mask, enable_reg);
      }
      break;

    case OC_capros_DevClock_getRate:
      msg.snd_w1 = rate;
      break;

    case OC_capros_key_destroy:
      // This process is permanent. 
      break;

    case OC_capros_key_getType:
      msg.snd_w1 = IKT_capros_DevClock;
      break;
    }
  }
}
