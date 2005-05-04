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

#include "keyring.h"
#include "Session.h"

#include "include/inet.h"
#include "include/icmp.h"
#include "include/ip.h"

#define DEBUGICMP  if(0)

void
icmp_input(struct pbuf *p,struct netif *inp)
{
  unsigned char type;
  unsigned char code;
  struct icmp_echo_hdr *iecho = NULL;
  struct ip_hdr *iphdr;
  struct ip_addr tmpaddr;
  uint16_t hlen;
  uint16_t BROADCAST = 0; /* Check if this is a ping broadcast or not */
  int index;
  
  iphdr = p->payload;
  hlen = IPH_HL(iphdr) * 4;
  if (pbuf_header(p, -((int16_t)hlen)) || (p->tot_len < sizeof(uint16_t)*2)) {
    kprintf(KR_OSTREAM,"icmp_input: short ICMP(%u bytes)received",p->tot_len);
    pbuf_free(p);
    return;      
  }
  
  type = *((uint8_t *)p->payload);
  code = *(((uint8_t *)p->payload)+1);
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
      pbuf_free(p);

      return;      
    }
    iecho = p->payload;    
    if (inet_chksum_pbuf(p) != 0) {
      kprintf(KR_OSTREAM,"icmp_input:checksum failed for received ICMP echo");
      pbuf_free(p);
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
    pbuf_header(p, hlen);
    ip_output(p, &(iphdr->src), IP_HDRINCL,
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
      pbuf_free(p);
      return;      
    }
    iecho = p->payload;    
    if (inet_chksum_pbuf(p) != 0) {
      kprintf(KR_OSTREAM,"icmp_input:checksum failed for received ICMP echo");
      pbuf_free(p);
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
  pbuf_free(p);
}

void
icmp_dest_unreach(struct pbuf *p, enum icmp_dur_type t)
{
  struct pbuf *q;
  struct ip_hdr *iphdr;
  struct icmp_dur_hdr *idur;
  
  q = pbuf_alloc(PBUF_IP, 8 + IP_HLEN + 8, PBUF_RAM);
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
  pbuf_free(q);
}


#if IP_FORWARD
void
icmp_time_exceeded(struct pbuf *p, enum icmp_te_type t)
{
  struct pbuf *q;
  struct ip_hdr *iphdr;
  struct icmp_te_hdr *tehdr;

  q = pbuf_alloc(PBUF_IP, 8 + IP_HLEN + 8, PBUF_RAM);

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
  pbuf_free(q);
}

#endif /* IP_FORWARDING > 0 */


/* This function outputs "ping" packets. This should be shifted out of
 * the network subsystem into a user application & the network sub system
 * allows raw ip sockets. 
 */  
uint32_t
icmp_echo_output(uint16_t id,struct ip_addr ipaddr)
{
  static uint16_t seqno;
  struct icmp_echo_hdr *iecho;
  struct pbuf *p;
  int i;
  
  p = pbuf_alloc(PBUF_IP, 8+50, PBUF_RAM); /*ICMP header+50 bytes of data*/
  if(p==NULL){ 
    kprintf(KR_OSTREAM,"No pbuf RAM left");
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
  pbuf_free(p);
  
  return 1;
}
