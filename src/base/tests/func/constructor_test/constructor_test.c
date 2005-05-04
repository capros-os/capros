/*
 * Copyright (C) 2001, The EROS Group, LLC.
 *
 * This file is part of the EROS Operating System.
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

#include <eros/target.h>
#include <eros/Invoke.h>
#include <eros/NodeKey.h>
#include <domain/ConstructorKey.h>
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

int
main()
{
  uint32_t result;
  uint32_t isDiscrete;
  
  node_copy(KR_CONSTIT, KC_OSTREAM, KR_OSTREAM);
  node_copy(KR_CONSTIT, KC_METACON, KR_METACON);
  node_copy(KR_CONSTIT, KC_HELLO_SEG, KR_HELLO_SEG);
  node_copy(KR_CONSTIT, KC_HELLO_PC, KR_HELLO_PC);
  
  result = constructor_is_discreet(KR_METACON, &isDiscrete);
  
  if ( result == RC_OK && isDiscrete )
    kprintf(KR_OSTREAM, "Metacon alleges discretion\n");
  
  result = constructor_request(KR_METACON, KR_BANK, KR_SCHED, KR_VOID,
			 KR_NEWCON);

  if (result != RC_OK)
    kdprintf(KR_OSTREAM, "Failed to get constructor, rc=%d\n", result);

  kprintf(KR_OSTREAM, "Populate new constructor\n");

  constructor_insert_addrspace(KR_NEWCON, KR_HELLO_SEG);
  constructor_insert_pc(KR_NEWCON, KR_HELLO_PC);
  /* The hello address space wants it's output stream as the zeroth
     constituent. The fact that we need to hard-code this relationship
     is a good indication of a problem somewhere in here, but for
     now... */
  constructor_insert_constituent(KR_NEWCON, 0, KR_OSTREAM);

  constructor_seal(KR_NEWCON, KR_CONREQ);

  kprintf(KR_OSTREAM, "Request product...\n");

  result = constructor_request(KR_CONREQ, KR_BANK, KR_SCHED, KR_VOID,
	   	               KR_NEW_HELLO);

  if (result != RC_OK)
    kdprintf(KR_OSTREAM, "Failed to get product, rc=%d\n", result);

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

  key_destroy(KR_NEWCON);

  return 0;
}
