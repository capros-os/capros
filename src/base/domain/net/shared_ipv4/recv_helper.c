/*
 * Copyright (C) 2002, Jonathan S. Shapiro.
 *
 * This file is part of the EROS Operating System distribution.
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

/* This process is a recv helper to netsys. Its function is to call enet 
 * repeatedly and when enet "releases" the call, it turns around and 
 * calls the netsys process 
 * indicating the netsys has data to receive. Its a mirror image of the 
 * xmit_helper */
#include <stddef.h>
#include <eros/target.h>
#include <eros/NodeKey.h>
#include <eros/ProcessKey.h>
#include <eros/cap-instr.h>
#include <eros/Invoke.h>

#include <idl/capros/key.h>
#include <idl/capros/net/shared_ipv4/netsys.h>
#include <idl/capros/net/enet/enet.h>

#include <domain/domdbg.h>
#include <domain/Runtime.h>

#include <string.h>
#include "constituents.h"

#define KR_OSTREAM  KR_APP(0)
#define KR_NETSYS   KR_APP(1)
#define KR_ENET     KR_APP(2)

#define DEBUG_RECVHELPER  if(0)

const uint32_t __rt_stack_pointer = 0x20000u;
const uint32_t __rt_stack_pages   = 1u;

int
main(void)
{
  Message msg;
  
  node_extended_copy(KR_CONSTIT,KC_OSTREAM,KR_OSTREAM);
  
  DEBUG_RECVHELPER kprintf(KR_OSTREAM,"starting netsys alarm ... [SUCCESS]");

  /* Zero out the message structure */
  memset((void *)&msg,0,sizeof(Message));
  
  /* Now fill in the required values */
  msg.snd_invKey = KR_VOID;
  msg.snd_rsmkey = KR_RETURN;
  msg.rcv_rsmkey = KR_RETURN;
  msg.rcv_key0 = KR_NETSYS;
  msg.rcv_key1 = KR_ENET;
  
  /* Return back to get the netsys process running again */
  RETURN(&msg);
  
  /* Now we should have received the netsys key in KR_NETSYS */
  msg.rcv_key0 = KR_VOID;
  msg.rcv_key1 = KR_VOID;
  msg.snd_invKey = KR_RETURN;
  /* Send back to Netsys to get it running too*/
  SEND(&msg);

  DEBUG_RECVHELPER kprintf(KR_OSTREAM,"recv_helper: We go our own ways");
  
  /* Loop - Call,block ,turnaround & call */
  msg.snd_code = OC_eros_domain_net_enet_enet_recv_queue;
  for(;;) {
    msg.snd_invKey = KR_ENET;
    CALL(&msg); /* Call enet */
        
    msg.snd_invKey = KR_NETSYS; /* Call Netsys to indicate reception */
    CALL(&msg);
  }
  
  return 0;
}
