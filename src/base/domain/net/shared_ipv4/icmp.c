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

/* Implementation of the icmp layer */
#include <stddef.h>
#include <eros/target.h>
#include <eros/endian.h>
#include <eros/Invoke.h>

#include <domain/domdbg.h>

#include <string.h>

#include "netsyskeys.h"
#include "Session.h"

#include "include/inet.h"
#include "include/icmp.h"
#include "include/ip.h"

extern struct session ActiveSessions[MAX_SESSIONS];
#define DEBUGICMP  if(0)

void
icmp_input(struct pstore *p,int ssid,struct netif *inp)
{
  unsigned char type;
  unsigned char code;
  struct icmp_echo_hdr *iecho = NULL;
  struct ip_hdr *iphdr;
  struct ip_addr tmpaddr;
  uint16_t hlen;
  uint16_t BROADCAST = 0; /* Check if this is a ping broadcast or not */
  int index;
  struct pstore *q;
  
  iphdr = PSTORE_PAYLOAD(p,ssid);;
  hlen = IPH_HL(iphdr) * 4;
  if (pstore_header(p, -((int16_t)hlen),ssid)
      ||(p->tot_len < sizeof(uint16_t)*2)) {
    kprintf(KR_OSTREAM,"icmp_input: short ICMP(%u bytes)received",p->tot_len);
    pstore_free(p,ssid);
    return;      
  }
  
  type = *((uint8_t *)PSTORE_PAYLOAD(p,ssid));
  code = *(((uint8_t *)PSTORE_PAYLOAD(p,ssid))+1);
  switch (type) {
  case ICMP_ECHO:
    if (ip_addr_isbroadcast(&iphdr->dest, &inp->netmask) ||
	ip_addr_ismulticast(&iphdr->dest)) {
      kprintf(KR_OSTREAM,"Broadcast Ping");
      BROADCAST = 1;
    }
    DEBUGICMP kprintf(KR_OSTREAM,"icmp_input: ping");
    if (p->tot_len < sizeof(struct icmp_echo_hdr)) {
      kprintf(KR_OSTREAM,"icmp_input: bad ICMP echo received");
      pstore_free(p,ssid);
      return;      
    }
    iecho = PSTORE_PAYLOAD(p,ssid);    
    if (inet_chksum_pstore(p,ssid) != 0) {
      kprintf(KR_OSTREAM,"icmp_input:checksum failed for received ICMP echo");
      pstore_free(p,ssid);
      return;
    }
    
    DEBUGICMP kprintf(KR_OSTREAM,"Pong!");

    tmpaddr.addr = iphdr->src.addr;
    if(!BROADCAST)  iphdr->src.addr = iphdr->dest.addr;
    else iphdr->src.addr = inp->ip_addr.addr;
    iphdr->dest.addr = tmpaddr.addr;
    ICMPH_TYPE_SET(iecho,ICMP_ER);
    /* adjust the checksum */
    if(!BROADCAST) {
      if (iecho->chksum >= htons(0xffff - (ICMP_ECHO << 8))) {
	iecho->chksum += htons(ICMP_ECHO << 8) + 1;
      } else {
	iecho->chksum += htons(ICMP_ECHO << 8);
      }
    }else {
      /* Compute a checksum */
      iecho->chksum = 0;
      iecho->chksum = inet_chksum(iecho,p->len);
    }
    pstore_header(p, hlen,ssid);

    /* Allocate a zero length pstore and chain it to the transmit
     * store. We do this to avoid an explicit copy over to the Xmit
     * ring buffer */
    q = pstore_alloc(PSTORE_TRANSPORT,0,XMIT_STACK_SPACE,ssid);
    if(q == NULL) {
      kprintf(KR_OSTREAM,"icmp echo: No available pbuf");
      pstore_free(p,ssid);
    return;
    }
    pstore_chain_no_ref(q,p,ssid);
    
    ip_output(q, ssid, &(iphdr->src), IP_HDRINCL,
	      IPH_TTL(iphdr), IP_PROTO_ICMP);
    
    break; 
    
  case ICMP_ER:
    if (ip_addr_isbroadcast(&iphdr->dest, &inp->netmask) ||
	ip_addr_ismulticast(&iphdr->dest)) {
      kprintf(KR_OSTREAM,"Broadcast Reply");
      BROADCAST = 1;
    }
    DEBUGICMP kprintf(KR_OSTREAM,"icmp_input: reply pong");
    if (p->tot_len < sizeof(struct icmp_echo_hdr)) {
      kprintf(KR_OSTREAM,"icmp_input: bad ICMP reply received");
      pstore_free(p,ssid);
      return;      
    }
    iecho = PSTORE_PAYLOAD(p,ssid);    
    if (inet_chksum_pstore(p,ssid) != 0) {
      kprintf(KR_OSTREAM,"icmp_input:checksum failed for received ICMP echo");
      pstore_free(p,ssid);
      return;
    }
    if((index=find_icmpsession(iecho->id))!=-1) {
      if(IsIcmpListening(index)) {
	setIcmpListening(index);
	wakeSession(index);
      }
    }
    break; 
  default:
    kprintf(KR_OSTREAM,"icmp_input:ICMP type %d code %d not supported", 
	    (int)type, (int)code);
  }
}

#if 0
void
icmp_dest_unreach(struct pstore *p, enum icmp_dur_type t)
{
  struct pstore *q;
  struct ip_hdr *iphdr;
  struct icmp_dur_hdr *idur;
  
  q = pstore_alloc(PSTORE_IP, 8 + IP_HLEN + 8, PSTORE_RAM);
  /* ICMP header + IP header + 8 bytes of data */

  iphdr = p->payload;
  
  idur = q->payload;
  ICMPH_TYPE_SET(idur, ICMP_DUR);
  ICMPH_CODE_SET(idur, t);

  memcpy((char *)q->payload + 8, p->payload, IP_HLEN + 8);
  
  /* calculate checksum */
  idur->chksum = 0;
  idur->chksum = inet_chksum(idur, q->len);

  ip_output(q, NULL, &(iphdr->src),
	    ICMP_TTL, IP_PROTO_ICMP);
  pstore_free(q);
}


#if IP_FORWARD
void
icmp_time_exceeded(struct pstore *p, enum icmp_te_type t)
{
  struct pstore *q;
  struct ip_hdr *iphdr;
  struct icmp_te_hdr *tehdr;

  q = pstore_alloc(PSTORE_IP, 8 + IP_HLEN + 8, PSTORE_RAM);

  iphdr = p->payload;
#if ICMP_DEBUG
  kprintf(KR_OSTREAM, ("icmp_time_exceeded from "));
  ip_addr_debug_print(&(iphdr->src));
  kprintf(KR_OSTREAM, (" to "));
  ip_addr_debug_print(&(iphdr->dest));
  kprintf(KR_OSTREAM, ("\n"));
#endif /* ICMP_DEBNUG */

  tehdr = q->payload;
  ICMPH_TYPE_SET(tehdr, ICMP_TE);
  ICMPH_CODE_SET(tehdr, t);

  /* copy fields from original packet */
  memcpy((char *)q->payload + 8, (char *)p->payload, IP_HLEN + 8);
  
  /* calculate checksum */
  tehdr->chksum = 0;
  tehdr->chksum = inet_chksum(tehdr, q->len);
  
  ip_output(q, NULL, &(iphdr->src),
	    ICMP_TTL, IP_PROTO_ICMP);
  pstore_free(q);
}

#endif /* IP_FORWARDING > 0 */

/* This function outputs "ping" packets. This should be shifted out of
 * the network subsystem into a user application & the network sub system
 * allows raw ip sockets. 
 */  
uint32_t
icmp_echo_output(uint16_t ssid,struct ip_addr ipaddr)
{
  static uint16_t seqno;
  struct icmp_echo_hdr *iecho;
  struct pstore *p;
  int i;
  
  /*ICMP header+50 bytes of data*/
  p = pstore_alloc(PSTORE_IP, 8+50, PSTORE_RAM); 
  if(p==NULL){ 
    kprintf(KR_OSTREAM,"No pstore RAM left");
    return -1;
  }
  
  iecho = p->payload;
  
  ICMPH_TYPE_SET(iecho,ICMP_ECHO);
  ICMPH_CODE_SET(iecho,0);
  
  iecho->id = id; /* id of connection */
  
  /* Seqno of ping packet and increment */
  iecho->seqno = seqno;
  seqno ++;
  
  for(i=0;i<50;i++) {
    ((char *)p->payload)[i+8] = i;
  }
  
  /* calculate checksum */
  iecho->chksum = 0;
  iecho->chksum = inet_chksum(iecho,p->len);
  kprintf(KR_OSTREAM,"iecho len %d",p->len);
  
  ip_output(p,&(NETIF.ip_addr), &ipaddr,ICMP_TTL, IP_PROTO_ICMP);
  pstore_free(p);
  
  return 1;
}
#endif
