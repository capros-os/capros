/*
 * Copyright (C) 2003, Jonathan S. Shapiro.
 * Copyright (C) 2007, Strawberry Development Group.
 *
 * This file is part of the CapROS Operating System distribution.
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
/* This material is based upon work supported by the US Defense Advanced
Research Projects Agency under Contract No. W31P4Q-07-C-0070.
Approved for public release, distribution unlimited. */

/* A simple no frills ttcp receiver. It binds a tcp socket and listens 
 * on it for connections. It uses shared memory to communicate between
 * the app and the netsys */
#include <stddef.h>
#include <eros/target.h>
#include <eros/NodeKey.h>
#include <eros/Invoke.h>
#include <eros/endian.h>
#include <eros/KeyConst.h>

#include <idl/capros/Process.h>
#include <idl/capros/Sleep.h>

#include <domain/SpaceBankKey.h>
#include <domain/ConstructorKey.h>
#include <domain/NetSysKey.h>
#include <domain/domdbg.h>
#include <domain/Runtime.h>

#include <idl/capros/net/shared_ipv4/netsys.h>
#include <addrspace/addrspace.h>

#include <ctype.h>
#include <string.h>

#include "constituents.h"

#define KR_OSTREAM      KR_APP(0)
#define KR_NETSYS_C     KR_APP(1)
#define KR_NETSYS_S     KR_APP(2)
#define KR_NETSYS       KR_APP(3)
#define KR_SLEEP        KR_APP(4)
#define KR_SUB_BANK     KR_APP(5)
#define KR_NEW_SUBSPACE KR_APP(6)
#define KR_XMIT_BUF     KR_APP(7)
#define KR_RECV_BUF     KR_APP(8)

#define KR_SCRATCH      KR_APP(20)

/* The largest size any domain currently has for sharing or otherwise
mapping memory is 1GB.  This domain maps an array of 32-bit values
into that region and the offset into this region starts at zero. */
#define _1GB_ 0x40000000u
#define MAX_BUFFER_OFFSET ((_1GB_ / sizeof(uint32_t)) - 1)

#define buflen          8*1024      /* length of buffer */
#define bufoffset       0           /* align buffer to this */
#define bufalign        16*1024     /* modulo this */

/* Globals */
int sinkmode = 0;            /* 0=normal I/O, !0=transmit mode */
int trans = 0;               /* 0=receive !0=transmit mode */
uint32_t numCalls = 0;       /* # of Network stack calls */

uint32_t *main_content = NULL;
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
  uint32_t u;
  Message msg;
  
  node_extended_copy(KR_CONSTIT, KC_OSTREAM,KR_OSTREAM);
  node_extended_copy(KR_CONSTIT, KC_NETSYS_C,KR_NETSYS_C);
  node_extended_copy(KR_CONSTIT, KC_SLEEP,KR_SLEEP);

  /* Prepare our default address space for mapping in additional
   * memory. */
  addrspace_prep_for_mapping(KR_SELF, KR_BANK, KR_SCRATCH, KR_NEW_SUBSPACE);

  /* Create a subbank to use for window creation */
  if (spcbank_create_subbank(KR_BANK, KR_SUB_BANK) != RC_OK) {
    kprintf(KR_OSTREAM, "shared_ttcp failed to create sub bank.\n");
    return -1;
  }

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
  result = eros_domain_net_shared_ipv4_netsys_get_netconfig(KR_NETSYS_S,&ipaddr,
						     &netmask,&gateway);
  if(result!=RC_OK) {
    kprintf(KR_OSTREAM,"Network configuration not done");
    return 0;
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
  msg.rcv_key1 = KR_XMIT_BUF;
  msg.rcv_key2 = KR_RECV_BUF;
  msg.snd_code = OC_NetSys_GetNewSessionKey;
  CALL(&msg);

  if(result!=RC_OK) {
    kprintf(KR_OSTREAM,"shared_shared_ipv4 test could not get Session key");
    return -1;
  }
  
  /* Map the new addr space into this domain's space */
  capros_Process_getAddrSpace(KR_SELF, KR_SCRATCH);
  node_swap(KR_SCRATCH, 16, KR_XMIT_BUF, KR_VOID);
  //node_swap(KR_SCRATCH, 17, KR_RECV_BUF, KR_VOID);
   
  /* In order to access this mapped space, this domain needs a
   * well-known address:  use the well-known address that corresponds
   * to slot 16 :*/
  main_content = (uint32_t *)0x80000000u;

#define TESTING 0
#if TESTING
    kprintf(KR_OSTREAM, "Test: clearing mapped memory...\n");

    kprintf(KR_OSTREAM, "   ... trying 0x%08x: = %d\n", &main_content[0x02],
	    main_content[0x02]);
    main_content[0x02] = 12;

    kprintf(KR_OSTREAM, " written memory = %d\n", main_content[0x02]);
    kprintf(KR_OSTREAM, "   ... trying 0x%08x:\n", &main_content[0x040]);
    //main_content[0x040] = 34;
    
#endif

  
  /* In order to make full use of the entire 1GB space (for resizing
   * or doing other "offscreen memory" tasks), we need
   * local window keys in the rest of the available slots */
  for (u = 17; u < 24; u++)
    addrspace_insert_lwk(KR_SCRATCH, 16, u, EROS_ADDRESS_LSS);
  
  msg.rcv_key0 = KR_VOID;

  if (bufalign != 0)
    actbuf = &buf[(bufalign - ((int)buf % bufalign) + bufoffset) % bufalign];
  
  kprintf(KR_OSTREAM,"ttcp-r: buflen=%d, nbuf=%d, align=%d/%d, port=%d",
	  buflen, nbuf, bufalign, bufoffset, port);
  kprintf(KR_OSTREAM,"  tcp");
  
  /* Now do the equivalent of opening a stream socket and binding 
   * it to the port 5001 */
  result = eros_domain_net_shared_ipv4_netsys_tcp_bind(KR_NETSYS,dummy,0,port);
  if(result!=RC_OK) {
    kprintf(KR_OSTREAM,"Binding 5001 TCP port ... [FAILED]");
    return 1;
  }
  
  /* We are the server and should listen for the connections */
  result = eros_domain_net_shared_ipv4_netsys_tcp_listen(KR_NETSYS);
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
