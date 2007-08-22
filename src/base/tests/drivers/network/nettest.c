/*
 * Copyright (C) 2002, Jonathan S. Shapiro.
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

#include <stddef.h>
#include <eros/target.h>
#include <eros/Invoke.h>
#include <eros/NodeKey.h>
#include <eros/endian.h>

#include <idl/capros/Process.h>
#include <idl/capros/Sleep.h>

#include <domain/ConstructorKey.h>
#include <domain/domdbg.h>
#include <domain/Runtime.h>
#include <domain/drivers/NetKey.h>

#include "constituents.h"
#include "ethernet.h"
#include "ip.h"
#include "icmp.h"
#include "arp.h"

#define KR_OSTREAM     KR_APP(0)
#define KR_START       KR_APP(1)
#define KR_SLEEP       KR_APP(2)
#define KR_NETIF_C     KR_APP(3)
#define KR_NETIF_S     KR_APP(4)

/*Protocols over IP*/
#define IP_ICMP        1
#define IP_UDP         17

/* Globals */
struct ether_pkt pkt;
uint8_t eaddr[ETHER_ADDR_LEN];


/* Function prototypes */
void prepareIpPacket(IP_HEADER*,IN_ADDR,IN_ADDR,void *,int,int);

int
main(void)
{
  Message msg;
  uint32_t result;
  int i;
  //uint8_t daddr[ETHER_ADDR_LEN] = {0x00,0xB0,0xD0,0x82,0x33,0xC9};
  uint8_t daddr[ETHER_ADDR_LEN] = {0xff,0xff,0xff,0xff,0xff,0xff};
  char data[ETHERMTU];
  IP_HEADER iphdr;
  icmp ICMP;
  char *s = (char *)&ICMP;
  IN_ADDR srcip;
  IN_ADDR destip;
    
  node_extended_copy(KR_CONSTIT,KC_OSTREAM,KR_OSTREAM);
  node_extended_copy(KR_CONSTIT,KC_NETIF_C,KR_NETIF_C);
  node_extended_copy(KR_CONSTIT,KC_SLEEP,KR_SLEEP);
      
  result = capros_Process_makeStartKey(KR_SELF, 0, KR_START);
  if(result != RC_OK) {
    kprintf(KR_OSTREAM,"nettest:: Start Key...[Failed]");
  }
  
  /* Construct the netif network driver */
  result = constructor_request(KR_NETIF_C, KR_BANK, KR_SCHED, KR_VOID,
                               KR_NETIF_S);
  if (result != RC_OK) {
    kprintf(KR_OSTREAM, "nettest:Constructing netif...[FAILED]\n");
    return 0;
  }

  msg.snd_invKey = KR_NETIF_S;
  msg.snd_key0 = KR_VOID;
  msg.snd_key1 = KR_VOID;
  msg.snd_key2 = KR_VOID;
  msg.snd_rsmkey = KR_VOID;
  msg.snd_data = 0;
  msg.snd_len = 0;
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
  
  ICMP.icmp_type = ICMP_ECHO;
  ICMP.icmp_code = 0;
  ICMP.icmp_id = 0;
  ICMP.icmp_seq = 1;
  ICMP.icmp_cksum = inchksum((unsigned short *)&ICMP,sizeof(icmp));
  
  for(i=sizeof(IP_HEADER);i<sizeof(icmp)+sizeof(IP_HEADER);i++) {
    data[i] = s[i-sizeof(IP_HEADER)];  
  }
  
  srcip.s_addr = 0x129610AC;
  destip.s_addr = 0xB27BA8C0;
  prepareIpPacket(&iphdr,srcip,destip,(void *)&data[sizeof(IP_HEADER)],
		  sizeof(icmp),IP_ICMP);
  
  /* Copy in IP header */
  for(i=0;i<sizeof(IP_HEADER);i++) {
    data[i] = ((char *)((void *)&iphdr))[i];
  }
  
  for(i=0;i<5;i++) {
    msg.snd_code = OC_netif_xmit;
    msg.snd_len = ETHER_MAX_LEN;
    msg.snd_data = &data;
    msg.snd_w1 = ETHERTYPE_IP;
    msg.snd_w2 = daddr[3] + (daddr[4]<<8) + (daddr[5]<<16);
    msg.snd_w3 = daddr[0] + (daddr[1]<<8) + (daddr[2]<<16);
    kprintf(KR_OSTREAM,"%x::%x",msg.snd_w2,msg.snd_w3);
    CALL(&msg);
    kprintf(KR_OSTREAM,"CALL  returned");
    capros_Sleep_sleep(KR_SLEEP,1000);
  }

  return 0;
}

/* Generate an  IP packet */
void
prepareIpPacket(IP_HEADER *iphdr,IN_ADDR src,IN_ADDR dest,
		void *data,int size,int prot) {
  
  int len = size + sizeof(IP_HEADER);
  
  /* For now we fill hopefully working values */
  iphdr->ip_vhl = 0x45;   /*IPv4*/
  iphdr->ip_tos = 0; 
  iphdr->ip_len = htons(len);
  iphdr->ip_id = 0;
  iphdr->ip_off = 0;   /*fragment 0*/
  iphdr->ip_ttl = 64;  /*Max time*/
  iphdr->ip_p = prot;
  iphdr->ip_src = src;
  iphdr->ip_dst = dest;
  iphdr->ip_sum = inchksum((unsigned short *)iphdr,sizeof(IP_HEADER));
    
  kprintf(KR_OSTREAM,"sizeof IP_HEADER = %d",sizeof(IP_HEADER));
  return;
}
