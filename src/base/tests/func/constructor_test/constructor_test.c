/*
 * Copyright (C) 2001, The EROS Group, LLC.
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

#include <eros/target.h>
#include <eros/Invoke.h>
#include <idl/capros/key.h>
#include <idl/capros/Constructor.h>
#include <idl/capros/Node.h>
#include <domain/Runtime.h>
#include <domain/domdbg.h>
#include "constituents.h"

#define KR_OSTREAM KR_APP(0)
#define KR_NEWCON  KR_APP(1)
#define KR_METACON KR_APP(2)
#define KR_NEW_HELLO KR_APP(3)
#define KR_HELLO_SEG KR_APP(4)
#define KR_HELLO_PC  KR_APP(5)
#define KR_CONREQ    KR_APP(6)

const uint32_t __rt_stack_pages = 1;
const uint32_t __rt_stack_pointer = 0x20000;

#define ckOK \
  if (result != RC_OK) { \
    kdprintf(KR_OSTREAM, "Line %d result is 0x%08x!\n", __LINE__, result); \
  }

int
main()
{
  uint32_t result;
  uint32_t isDiscreet;
  
  capros_Node_getSlot(KR_CONSTIT, KC_OSTREAM, KR_OSTREAM);
  capros_Node_getSlot(KR_CONSTIT, KC_METACON, KR_METACON);
  capros_Node_getSlot(KR_CONSTIT, KC_HELLO_SEG, KR_HELLO_SEG);
  capros_Node_getSlot(KR_CONSTIT, KC_HELLO_PC, KR_HELLO_PC);
  
  result = capros_Constructor_isDiscreet(KR_METACON, &isDiscreet);
  ckOK
  if (isDiscreet)
    kprintf(KR_OSTREAM, "Metacon alleges discretion\n");
  
  result = capros_Constructor_request(KR_METACON, KR_BANK, KR_SCHED, KR_VOID,
			 KR_NEWCON);
  ckOK

  kprintf(KR_OSTREAM, "Insert addr space\n");

  uint32_t pc;
  result = capros_Number_get32(KR_HELLO_PC, &pc);
  ckOK
  result = capros_Constructor_insertAddrSpace32(KR_NEWCON, KR_HELLO_SEG, pc);
  ckOK

  kprintf(KR_OSTREAM, "Insert constituent\n");

  /* The hello address space wants its output stream as the zeroth
     constituent. */
  result = capros_Constructor_insertConstituent(KR_NEWCON, 0, KR_OSTREAM);
  ckOK

  result = capros_Constructor_seal(KR_NEWCON, KR_CONREQ);
  ckOK

  kprintf(KR_OSTREAM, "Request product...\n");

  result = capros_Constructor_request(KR_CONREQ, KR_BANK, KR_SCHED, KR_VOID,
	   	               KR_NEW_HELLO);
  ckOK

  kprintf(KR_OSTREAM, "Got product!\n");

  {
    Message msg;
    msg.snd_key0 = KR_VOID;
    msg.snd_key1 = KR_VOID;
    msg.snd_key2 = KR_VOID;
    msg.snd_rsmkey = KR_VOID;
    msg.snd_len = 0;
    msg.snd_code = 0;
    msg.snd_invKey = KR_NEW_HELLO;
    
    msg.rcv_key0 = KR_VOID;
    msg.rcv_key1 = KR_VOID;
    msg.rcv_key2 = KR_VOID;
    msg.rcv_rsmkey = KR_VOID;
    msg.snd_len = 0;

    CALL(&msg);
  }
  
  kprintf(KR_OSTREAM, "Destroying the constructor!\n");

  capros_key_destroy(KR_NEWCON);
  
  kprintf(KR_OSTREAM, "\nDONE!\n");

  return 0;
}
