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

/* This process is a alarm timer. The NetSys uses it to for 
 * sleeping on its behalf */
#include <stddef.h>
#include <eros/target.h>
#include <eros/NodeKey.h>
#include <eros/ProcessKey.h>
#include <eros/cap-instr.h>
#include <eros/Invoke.h>

#include <idl/capros/key.h>
#include <idl/capros/Sleep.h>
#include <idl/capros/net/shared_ipv4/netsys.h>

#include <domain/domdbg.h>
#include <domain/Runtime.h>

#include <string.h>
#include "constituents.h"

#define KR_OSTREAM  KR_APP(0)
#define KR_NETSYS   KR_APP(1)
#define KR_SLEEP    KR_APP(2)

#define DEBUG_TIMEOUTAGENT if(0)
#define TIME_GRANULARITY   100 /* ms */

/* The job of this alarm process is to keep calling the netsys 
 * intermittently & sleeping in a loop */
int
main(void)
{
  Message msg;
  
  node_extended_copy(KR_CONSTIT,KC_OSTREAM,KR_OSTREAM);
  node_extended_copy(KR_CONSTIT,KC_SLEEP,KR_SLEEP);
  
  DEBUG_TIMEOUTAGENT kprintf(KR_OSTREAM,"starting netsys alarm ... [SUCCESS]");

  /* Zero out the message structure */
  memset((void *)&msg,0,sizeof(Message));
  
  /* Now fill in the required values & return to void 
   * The netsys will call us when we need to start working */
  msg.snd_invKey = KR_VOID;
  msg.snd_rsmkey = KR_RETURN;
  msg.rcv_rsmkey = KR_RETURN;
  msg.rcv_key0 = KR_NETSYS;
  RETURN(&msg);
  
  msg.snd_invKey = KR_RETURN;
  msg.rcv_key0 = KR_VOID;
  /* Send back to get the netsys process running again */
  SEND(&msg);
  
  msg.snd_invKey = KR_NETSYS;
  
  /* Loop - Sleep & Call */
  for(;;) {
    capros_Sleep_sleep(KR_SLEEP,TIME_GRANULARITY);
    //eros_domain_net_ipv4_netsys_timeout_call(KR_NETSYS);
    CALL(&msg);
  }
  
  return 0;
}
