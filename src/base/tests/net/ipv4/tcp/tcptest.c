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
Research Projects Agency under Contract Nos. W31P4Q-06-C-0040 and
W31P4Q-07-C-0070.  Approved for public release, distribution unlimited. */

#include <stddef.h>
#include <stddef.h>
#include <eros/target.h>
#include <eros/NodeKey.h>
#include <eros/Invoke.h>
#include <eros/ProcessKey.h>
#include <eros/endian.h>

#include <idl/eros/Sleep.h>

#include <domain/ConstructorKey.h>
#include <domain/NetSysKey.h>
#include <domain/domdbg.h>
#include <domain/Runtime.h>

#include <idl/eros/domain/net/ipv4/netsys.h>

#include "constituents.h"

#include <ctype.h>
#include <string.h>

#include "constituents.h"

#define KR_OSTREAM      KR_APP(0)
#define KR_NETSYS_C     KR_APP(1)
#define KR_NETSYS_S     KR_APP(2)
#define KR_NETSYS       KR_APP(3)
#define KR_SLEEP        KR_APP(4)


/* Globals */
#define MAX_SIZE   200
char  buf[MAX_SIZE];  
//char *req_str = "GET http://www.cs.jhu.edu/index.html HTTP/1.0\n"; //www.cs.jhu.edu
char *req_str = "GET http://srl.cs.jhu.edu/~shap/index.html HTTP/1.0\n\n"; //srl.cs.jhu.edu/~anshumal
//char *req_str = "GET head.ply 0 400";


int 
main(void)
{
  uint32_t result;
  Message msg;
  node_extended_copy(KR_CONSTIT, KC_OSTREAM,KR_OSTREAM);
  node_extended_copy(KR_CONSTIT, KC_NETSYS_C,KR_NETSYS_C);
  node_extended_copy(KR_CONSTIT, KC_SLEEP,KR_SLEEP);
  
  /* Construct the netsys domain */
  result = constructor_request(KR_NETSYS_C,KR_BANK,KR_SCHED,
			       KR_VOID,KR_NETSYS_S);
  if (result != RC_OK) {
    kprintf(KR_OSTREAM, "nettest:Constructing NETSYS...[FAILED]\n");
  }
    
  /* Sleep for sometime to allow the netsys to configure itself
   * using DHCP */
  eros_Sleep_sleep(KR_SLEEP,4000);

  msg.snd_key0   = KR_VOID;
  msg.snd_key1   = KR_VOID;
  msg.snd_key2   = KR_VOID;
  msg.snd_rsmkey = KR_VOID;
  msg.snd_data = 0;
  msg.snd_len  = 0;
  msg.snd_code = 0;
  msg.snd_w1 = 0;
  msg.snd_w2 = 0;
  msg.snd_w3 = 0;
  
  msg.rcv_key0 = KR_NETSYS;
  msg.rcv_key1 = KR_VOID;
  msg.rcv_key2 = KR_VOID;
  msg.rcv_rsmkey = KR_VOID;
  msg.rcv_data = 0;
  msg.rcv_limit = 0;
  msg.rcv_code = 0;
  msg.rcv_w1 = 0;
  msg.rcv_w2 = 0;
  msg.rcv_w3 = 0;

  msg.snd_invKey = KR_NETSYS_S;
  msg.snd_code = OC_NetSys_GetNetConfig;
  CALL(&msg); /* Check if we are configured */
  
  msg.snd_code = OC_NetSys_GetSessionCreatorKey;
  msg.rcv_key0 = KR_NETSYS_S;
  CALL(&msg); /* Receive a session creator key */
  
  msg.snd_key0 = KR_VOID;
  msg.snd_key1 = KR_VOID;
  msg.snd_key2 = KR_VOID;
  msg.snd_rsmkey = KR_VOID;
  msg.snd_data = 0;
  msg.snd_len = 0;
  msg.snd_w1 = 0;
  msg.snd_w2 = 0;
  msg.snd_w3 = 0;
  
  msg.rcv_key0 = KR_VOID;
  msg.rcv_key1 = KR_VOID;
  msg.rcv_key2 = KR_VOID;
  msg.rcv_rsmkey = KR_VOID;
  msg.rcv_data = 0;
  msg.rcv_limit = 0;
  msg.rcv_code = 0;
  msg.rcv_w1 = 0;
  msg.rcv_w2 = 0;
  msg.rcv_w3 = 0;
  
  /* We now have netsys session creator key. 
   * So go ahead and create a session*/
  msg.snd_invKey = KR_NETSYS_S;
  msg.snd_key0 = KR_BANK;
  msg.rcv_key0 = KR_NETSYS;
  msg.snd_code = OC_NetSys_GetNewSessionKey;
  CALL(&msg);
 
  msg.rcv_key0 = KR_VOID;
  
  /* We now have a session key  */
  /* Bind a local port */
  //kprintf(KR_OSTREAM,"TCP BIND Called");  
  //result = eros_domain_net_ipv4_netsys_tcp_bind(KR_NETSYS,0,0,900);
  //kprintf(KR_OSTREAM,"Bind returned code %d",result);
  
  /* Call TCP Connect */ 
  kprintf(KR_OSTREAM,"TCP Connect Called");  
  //result = eros_domain_net_ipv4_netsys_tcp_connect(KR_NETSYS,0,0x6329EFD8,80);
  //  result = eros_domain_net_ipv4_netsys_tcp_connect(KR_NETSYS,0,0x650DDC80,80); //CS.JHU>EDU
  result = eros_domain_net_ipv4_netsys_tcp_connect(KR_NETSYS,0,0xf6DFDC80,80); //SRL.CS.JHU.EDU
  //result = eros_domain_net_ipv4_netsys_tcp_connect(KR_NETSYS,0,0x6329EFD8,80);
  //result = eros_domain_net_ipv4_netsys_tcp_connect(KR_NETSYS,0,
  //					    0x01C9A8C0,1800);
  //msg.snd_w2 = 0x650DDC80/*0x019610AC*//*0xB27BA8C0*/;
  kprintf(KR_OSTREAM,"Connect returned code %d",result);

  if(result != RC_OK) return 1;
  

  /* Call TCP Send */ 
  kprintf(KR_OSTREAM,"TCP Send Called");  
  msg.snd_code = OC_NetSys_TCPSend; //Bind a udp socket 
  msg.snd_invKey = KR_NETSYS;
  msg.snd_w1 = 0;
  msg.snd_data = req_str;
  msg.snd_len = strlen(req_str);
  CALL(&msg);

  /* Call TCP Receive */ 
  /*kprintf(KR_OSTREAM,"TCP Receive Called");  
  msg.snd_code = OC_NetSys_TCPReceive; //TCP Receive 
  msg.snd_invKey = KR_NETSYS;
  msg.snd_w1 = 100;
  msg.rcv_data = &buf[0];
  msg.rcv_limit = MAX_SIZE;
  CALL(&msg);

  kprintf(KR_OSTREAM,"%s",&buf[0]);
  */
  eros_Sleep_sleep(KR_SLEEP,200000);
  /* Close the connection */
  kprintf(KR_OSTREAM,"TCP Close Called");  
  result = eros_domain_net_ipv4_netsys_tcp_close(KR_NETSYS);
  kprintf(KR_OSTREAM,"Closereturned code %d",result);
  
    
  kprintf(KR_OSTREAM,"TCP Test returned");
  return 0;
}
