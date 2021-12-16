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

/* IPV4 layer. */ 
#include <stddef.h>
#include <eros/target.h>
#include <eros/endian.h>
#include <eros/Invoke.h>

#include <domain/domdbg.h>
#include <domain/NetSysKey.h>

#include "include/ip.h"
#include "include/inet.h"
#include "include/ethernet.h"
#include "include/udp.h"
#include "include/tcp.h"
#include "include/ip_frag.h"
#include "include/TxRxqueue.h"

#include "netsyskeys.h"
#include "netif/netif.h"
#include "Session.h"

extern struct session ActiveSessions[MAX_SESSIONS];
#define DEBUGIP if(0)
#define TIME_IP if(0)
#define PARANOIA_IP if(0)

/* Sends an IP packet on a network interface. This function constructs
 * the IP header and calculates the IP header checksum. If the source
 * IP address is NULL, the IP address of the outgoing network
 * interface is filled in as source address.
 */
uint32_t
ip_output(struct pstore *p,int ssid,
	  struct ip_addr *src,struct ip_addr *dest,
	  uint8_t ttl,uint8_t proto)
{
  static struct ip_hdr *iphdr;
  static uint16_t ip_id = 0;
  
  if(dest != IP_HDRINCL) {
    if(pstore_header(p,IP_HLEN,ssid)) {
      pstore_free(p,ssid);
      return RC_NetSys_PbufsExhausted;
    }
    
    iphdr = PSTORE_PAYLOAD(p,ssid);
    
    IPH_TTL_SET(iphdr,ttl);
    IPH_PROTO_SET(iphdr,proto);
    
    ip_addr_set(&(iphdr->dest), dest);
    
    IPH_VHLTOS_SET(iphdr,4,IP_HLEN/4,0);
    IPH_LEN_SET(iphdr, htons(p->tot_len));
    IPH_OFFSET_SET(iphdr, htons(IP_DF));
    IPH_ID_SET(iphdr, htons(ip_id));
    ++ip_id;

    if(ip_addr_isany(src))  ip_addr_set(&(iphdr->src), &(NETIF.ip_addr));
    else ip_addr_set(&(iphdr->src), src);
        
    IPH_CHKSUM_SET(iphdr, 0);
    IPH_CHKSUM_SET(iphdr, inet_chksum(iphdr, IP_HLEN));
  } else {
    iphdr = PSTORE_PAYLOAD(p,ssid);
    dest = &(iphdr->dest);
  }
  
  /* don't fragment if interface has mtu set to 0 [loopif] */
  if (NETIF.mtu && (p->tot_len > NETIF.mtu))
    return ip_frag(p,ssid);
  return netif_start_xmit(p,ETHERTYPE_IP,&(iphdr->dest),ssid);  
}


/* This function is called by the network interface device driver when
 * an IP packet is received. The function does the basic checks of the
 * IP header such as packet size being at least larger than the header
 * size etc. If the packet was not destined for us, the packet is
 * forwarded (using ip_forward). The IP checksum is always checked.
 *
 * Finally, the packet is sent to the upper layer protocol input function.
 */
uint32_t
ip_input(struct pstore *p,int ssid) 
{
  static struct ip_hdr *iphdr;
  static uint16_t iphdrlen;
  uint16_t valid_packet = 0;

  /* identify the IP header */
  iphdr = PSTORE_PAYLOAD(p,ssid);
  if(IPH_V(iphdr) != 4) { /* Only ipv4 for now */
    kprintf(KR_OSTREAM,"Ip version ! = 4");
    pstore_free(p,ssid);
    return RC_OK;
  }
  
  /* obtain IP header length in number of 32-bit words */
  iphdrlen = IPH_HL(iphdr);
  iphdrlen *= 4;

#if 0
  /* header length exceeds first pstore length? */  
  if(iphdrlen > p->len) {
    kprintf(KR_OSTREAM,"hdr len > first pstore length");
    pstore_free(p,ssid);
    return RC_OK;
  }

  /* verify checksum */
  if(inet_chksum(iphdr,iphdrlen) != 0) {
    kprintf(KR_OSTREAM,"ip_input: inetchksum bad");
    pstore_free(p,ssid);
    return RC_OK;
  }
#endif
  PARANOIA_IP 
    kprintf(KR_OSTREAM,"ip pkt from %x to %x",iphdr->src,iphdr->dest);

  /* Trim pstore. This should have been done at the netif layer,
   * but we'll do it anyway just to be sure that its done. */
  pstore_realloc(p, ntohs(IPH_LEN(iphdr)),ssid);

  /* is this packet for us? */
  /* interface configured? */
  if(!ip_addr_isany(&(NETIF.ip_addr))) {
    /* unicast to this interface address? */
    if(ip_addr_cmp(&(iphdr->dest),&(NETIF.ip_addr)) ||
       /* or broadcast matching this interface network address? */
       (ip_addr_isbroadcast(&(iphdr->dest),&(NETIF.netmask)) &&
	ip_addr_maskcmp(&(iphdr->dest),&(NETIF.ip_addr),&(NETIF.netmask))) ||
       /* or restricted broadcast? */
       ip_addr_cmp(&(iphdr->dest), IP_ADDR_BROADCAST)) {
      /* Packet goes through */
      valid_packet = 1;
    }
  }else {
    /* FIX::When we are unconfigured we look greedily at each and every 
     * packet for dhcp information?
     */
    valid_packet = 1;
  }

  if(!valid_packet) {
    DEBUGIP kprintf(KR_OSTREAM,"ip Pkt not for us but for %x",iphdr->dest);
    pstore_free(p,ssid);
    return RC_OK;
  }

  if((IPH_OFFSET(iphdr) & htons(IP_OFFMASK | IP_MF)) != 0) {
    p = ip_reass(p,ssid);
    if(p==NULL) return RC_OK;
    iphdr = PSTORE_PAYLOAD(p,ssid);
  }
  
  /* send to upper layers */
  switch(IPH_PROTO(iphdr)) {
  case IP_PROTO_UDP:
    DEBUGIP kprintf(KR_OSTREAM,"udp pkt");
    udp_input(p,ssid);
    break;

  case IP_PROTO_TCP:
    DEBUGIP kprintf(KR_OSTREAM,"tcp pkt");
    tcp_input(p,ssid,&NETIF);
    break;
    
  case IP_PROTO_ICMP: 
    {
      /* FIX: We should not respond to any icmp requests 
      * unless we have a valid ip. For now we just check for
      * dhcp configuration success & only then answer icmp pkts
      * But we must wait for dhcp to receive an ip, else bail out
      * and never come in here */
      if(!ip_addr_isany(&NETIF.ip_addr)) icmp_input(p,ssid,&NETIF);
      else {
	kprintf(KR_OSTREAM,"Unconfigured address icmp req");
	pstore_free(p,ssid);
      }
      break;
    }
  default:
    DEBUGIP kprintf(KR_OSTREAM,"default pkt");
#if 0
    /* send ICMP destination protocol unreachable unless is was a broadcast */
    if(!ip_addr_isbroadcast(&(iphdr->dest), &(NETIF.netmask)) &&
       !ip_addr_ismulticast(&(iphdr->dest))) {
      p->payload = iphdr;
      icmp_dest_unreach(p, ICMP_DUR_PROTO);
    }
#endif
    pstore_free(p,ssid);
  }
  
  return RC_OK;
}
