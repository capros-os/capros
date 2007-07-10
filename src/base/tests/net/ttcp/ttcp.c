/*
 * Copyright (C) 2003, Jonathan S. Shapiro.
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
/* A simple no frills ttcp receiver. It binds a tcp socket and listens 
 * on it for connections */
#include <stddef.h>
#include <eros/target.h>
#include <eros/NodeKey.h>
#include <eros/Invoke.h>
#include <eros/ProcessKey.h>
#include <eros/endian.h>

#include <idl/capros/Sleep.h>

#include <domain/ConstructorKey.h>
#include <domain/NetSysKey.h>
#include <domain/domdbg.h>
#include <domain/Runtime.h>

#include <idl/capros/net/ipv4/netsys.h>

#include <ctype.h>
#include <string.h>

#include "constituents.h"

#define KR_OSTREAM      KR_APP(0)
#define KR_NETSYS_C     KR_APP(1)
#define KR_NETSYS_S     KR_APP(2)
#define KR_NETSYS       KR_APP(3)
#define KR_SLEEP        KR_APP(4)

#define buflen          8*1024      /* length of buffer */
#define bufoffset       0           /* align buffer to this */
#define bufalign        16*1024     /* modulo this */

/* Globals */
int sinkmode = 0;            /* 0=normal I/O, !0=transmit mode */
int trans = 0;               /* 0=receive !0=transmit mode */
uint32_t numCalls = 0;       /* # of Network stack calls */

int nbuf = 2*1024;           /* number of buffers to send in sinkmode */
char buf[buflen+bufalign];   /* ptr to dynamic buffer */

/* This is a ttcp server. So we put it into receive mode */
int 
main(void)
{
  result_t result;
  uint32_t ipaddr,netmask,gateway;
  uint32_t dummy=0;   /* We use a wrapper node, so w1 is bashed by kernel */
  short port = 5001;  /* The default port for ttcp */
  char     *actbuf;
  Message msg;
  
  node_extended_copy(KR_CONSTIT, KC_OSTREAM,KR_OSTREAM);
  node_extended_copy(KR_CONSTIT, KC_NETSYS_C,KR_NETSYS_C);
  node_extended_copy(KR_CONSTIT, KC_SLEEP,KR_SLEEP);
  
  /* Default settings */
  sinkmode = !sinkmode;
  
  /* Construct the netsys domain */
  result = constructor_request(KR_NETSYS_C,KR_BANK,KR_SCHED,
			       KR_VOID,KR_NETSYS_S);
  if (result != RC_OK) {
    kprintf(KR_OSTREAM, "nettest:Constructing NETSYS...[FAILED]\n");
  }

  /* Sleep for sometime to allow the netsys to configure itself
   * using DHCP */
  capros_Sleep_sleep(KR_SLEEP,2000);

  /* Check if we have been configured */
  result = eros_domain_net_ipv4_netsys_get_netconfig(KR_NETSYS_S,&ipaddr,
						     &netmask,&gateway);
  if(result!=RC_OK) {
    kprintf(KR_OSTREAM,"Network configuration not done");
   // return 0;
  }

  memset(&msg,0,sizeof(Message));
  msg.snd_invKey = KR_NETSYS_S;
  msg.rcv_key0 = KR_NETSYS;
  msg.snd_code = OC_NetSys_GetSessionCreatorKey;
  CALL(&msg); /* Receive a session creator key */
    
  /* We now have netsys session creator key. 
   * So go ahead and create a session*/
  msg.snd_invKey = KR_NETSYS;
  msg.snd_key0 = KR_BANK;
  msg.rcv_key0 = KR_NETSYS;
  msg.snd_code = OC_NetSys_GetNewSessionKey;
  CALL(&msg);
 
  msg.rcv_key0 = KR_VOID;

  if (bufalign != 0)
    actbuf = &buf[(bufalign - ((int)buf % bufalign) + bufoffset) % bufalign];
  
  kprintf(KR_OSTREAM,"ttcp-r: buflen=%d, nbuf=%d, align=%d/%d, port=%d",
	  buflen, nbuf, bufalign, bufoffset, port);
  kprintf(KR_OSTREAM,"  tcp");
  
  /* Now do the equivalent of opening a stream socket and binding 
   * it to the port 5001 */
  result = eros_domain_net_ipv4_netsys_tcp_bind(KR_NETSYS,dummy,0,port);
  if(result!=RC_OK) {
    kprintf(KR_OSTREAM,"Binding 5001 TCP port ... [FAILED]");
    return 1;
  }
  
  /* We are the server and should listen for the connections */
  result = eros_domain_net_ipv4_netsys_tcp_listen(KR_NETSYS);
  if(result!=RC_OK) {
    kprintf(KR_OSTREAM,"Listening on 5001 TCP  ... [FAILED]");
    return 1;
  }
  
#if 0
  /* Print out the statistics of the client */
  fromlen = sizeof(frominet);
  if((fd=accept(fd, &frominet, &fromlen) ) < 0) err("accept");
  { 
    struct sockaddr_in peer;
    int peerlen = sizeof(peer);
    if (getpeername(fd, (struct sockaddr_in *) &peer, &peerlen) < 0) {
      err("getpeername");
    }
    kprintf(KR_OSTREAM,"ttcp-r: accept from %s",inet_ntoa(peer.sin_addr));
  }
  
  if (sinkmode) {      
    register int cnt;
    while ((cnt=Nread(fd,buf,buflen)) > 0)
      nbytes += cnt;
  }else {
    while((cnt=Nread(fd,buf,buflen)) > 0 &&write(1,buf,cnt) == cnt)
      nbytes += cnt;
  }
  
  kprintf(KR_OSTREAM,"ttcp%s: buffer address %#x\n",trans?"-t":"-r",buf);
#endif  
  return 0;
}
