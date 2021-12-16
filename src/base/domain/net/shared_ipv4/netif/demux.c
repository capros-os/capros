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


/** Eager Demultiplexing (@See LRP) is during data reception. As the packet
 * is received by the hardware, it runs through the lists of the tcp_pcbs
 * and udp_pcbs (checking destination and source ports) to see whom the
 * packet belongs to. It now dumps the packet into that session's memory
 * shared with the netsys
 */

#include <stddef.h>
#include <eros/target.h>
#include <eros/Invoke.h>

#include <domain/domdbg.h>

#include "../include/ip.h"
#include "../include/ip_frag.h"
#include "../include/tcp.h"
#include "../include/udp.h"
#include "../include/pstore.h"

#include "enet_session.h"
#include "enetkeys.h"
#include "demux.h"

#define DEBUG_DEMUX if(0)
#define PARANOIA_DEMUX if(0)

/* extern variables */
extern struct udp_pcb *udp_pcbs;
extern struct tcp_pcb *tcp_active_pcbs;
extern struct tcp_pcb *tcp_tw_pcbs;      
extern struct tcp_pcb_listen *tcp_listen_pcbs;  
extern int release_rcvr;

/* Demux incoming packet:: 
 * First check if arp or ip packet. All others are dumped (FIX THIS)
 * Input is a pointer to the packet. 
 * If ip packet
 * Strategy: Switch on the protocol in the ip header. 
 *           Run through the respective lists looking for (host,port) match
 *           Emit out the ssid of the session to which the packet belongs to
 * FIX:: Fragmented IP packets without the transport header are a special case
 * and are not handled by the stack as yet.
 */
inline void
demux_pkt(void *p,int pkt_len)
{
  struct eth_hdr *ethhdr = NULL;
  uint16_t proto; 
  uint16_t src, dest;
  
  /* Make this an ethernet packet */
  ethhdr = (struct eth_hdr *)p;
  switch(htons(ethhdr->type)) {
  case ETHERTYPE_IP: {
    struct ip_hdr *iphdr = (struct ip_hdr *)((uint8_t *)p + ETHER_HDR_LEN);
    
    DEBUG_DEMUX
      kprintf(KR_OSTREAM,">>>>>>>>>>>>>>>>>>>>>>>>>>>>demux:: ip packet\n");
    proto = IPH_PROTO(iphdr);  /* Identify protocol */
    
    /* Check to see if transport segment is available */
    if((IPH_OFFSET(iphdr) & htons(IP_OFFMASK | IP_MF)) == 0) {
      goto transport_header_present;
      
      kprintf(KR_OSTREAM,"fragmented packet");
      kprintf(KR_OSTREAM,"dst: %x, src: %x, proto: %x\n", iphdr->dest.addr, 
	      iphdr->src.addr, proto);
      return;
      goto bad;
    }
  transport_header_present:
    /* Look into specific pcb's now */
    switch (proto) {
    case(IP_PROTO_UDP) : {
      struct udp_hdr *udphdr;
      int hdrlen;
      struct udp_pcb *pcb;
      
      /* Set pointer to udp header. Get Source and destination */
      udphdr = (struct udp_hdr *)((uint8_t *)iphdr + IPH_HL(iphdr)*4);
      src = ntohs(udphdr->src);
      dest = ntohs(udphdr->dest);
      
      /* All this space comes from the stack's account */
      hdrlen = ETHER_HDR_LEN + IPH_HL(iphdr)*4 + sizeof(struct udp_hdr);

      /* Demultiplex packet. First, go for a perfect match. */
      for(pcb = udp_pcbs; pcb != NULL; pcb = pcb->next) {
	if(pcb->remote_port == src &&
	   pcb->local_port == dest &&
	   (ip_addr_isany(&pcb->remote_ip) ||
	    ip_addr_cmp(&(pcb->remote_ip), &(iphdr->src))) &&
	   (ip_addr_isany(&pcb->local_ip) ||
	    ip_addr_cmp(&(pcb->local_ip), &(iphdr->dest)))) {
	  break;
	}
      }
      
      /* Go for wildcard matches.*/
      if(pcb == NULL) {
	for(pcb = udp_pcbs; pcb != NULL; pcb = pcb->next) {
	  if(/*((pcb->flags & UDP_FLAGS_CONNECTED) == 0) &&*/
	     /* destination port matches? */
	     (pcb->local_port == dest) &&
	     /* not bound to a specific (local) interface address? or... */
	     (ip_addr_isany(&pcb->local_ip) ||
	      /* ...matching interface address? */
	      ip_addr_cmp(&(pcb->local_ip), &(iphdr->dest)))) {
	    /* FIX:: pcb remote port is rewired to the src port of the udp 
	     * packet*/
	    pcb->remote_port = src;
	    break;
	  }
	}
      }
      DEBUG_DEMUX
	kprintf(KR_OSTREAM,"udp pcb finding %d",pcb!=NULL?pcb->ssid:-1);
      /* If we found something return the session id -1 otherwise */
      if(pcb == NULL) return;
      else {
	struct pstore *stack_p = NULL;
	
	if(pcb->ssid != 0) {
	  stack_p = copy_data_in(p,hdrlen,RECV_STACK_SPACE,
				 &((char*)p)[hdrlen],pkt_len-hdrlen,
				 RECV_CLIENT_SPACE,pcb->ssid);
	  if(stack_p == NULL) return;
	}else {
	  stack_p = copy_data_in(p,pkt_len,RECV_STACK_SPACE,
				 NULL,0,RECV_CLIENT_SPACE,pcb->ssid);
	  if(stack_p == NULL) return;
	}
	
	stack_p->status = PSTORE_READY;
	release_rcvr = 1;
		
	return;
      }
    }
      
      break;
    case( IP_PROTO_TCP) : {
      struct tcp_pcb *pcb, *prev;
      struct tcp_pcb_listen *lpcb;
      struct tcp_hdr *tcphdr = (struct tcp_hdr *)((uint8_t *)iphdr
						  + IPH_HL(iphdr)*4);
      struct pstore *stack_p = NULL;
      uint8_t offset;
      int hdrlen;
      int ssid;
      
      DEBUG_DEMUX kprintf(KR_OSTREAM,">>>>>>>>>>>>>TCP pkt");
      offset = TCPH_OFFSET(tcphdr) >> 4;
      dest = ntohs(tcphdr->dest);
      src = ntohs(tcphdr->src);
      
      /* All the space for the headers comes from the stack's space */
      hdrlen = ETHER_HDR_LEN + IPH_HL(iphdr)*4 + offset * 4;
      
      /* Demultiplex an incoming segment. First, we check if it is destined
       * for an active connection. */  
      prev = NULL;  
      for(pcb = tcp_active_pcbs; pcb != NULL; pcb = pcb->next) {
	if (pcb->remote_port == src &&
	    pcb->local_port == dest &&
	    ip_addr_cmp(&(pcb->remote_ip), &(iphdr->src)) &&
	    ip_addr_cmp(&(pcb->local_ip), &(iphdr->dest))) {
	  
	  /* Move this PCB to the front of the list so that subsequent
	   * lookups will be faster (we exploit locality in TCP segment
	   * arrivals). */
	  if (prev != NULL) {
	    prev->next = pcb->next;
	    pcb->next = tcp_active_pcbs;
	    tcp_active_pcbs = pcb; 
	  }
	  break;
	}
	prev = pcb;
      }
      
      /* If it did not go to an active connection, we check the connections
       * in the TIME-WAIT state. */
      if (pcb == NULL) {
	for(pcb = tcp_tw_pcbs; pcb != NULL; pcb = pcb->next) {
	  if (pcb->remote_port == src &&
	      pcb->local_port == dest &&
	      ip_addr_cmp(&(pcb->remote_ip), &(iphdr->src)) &&
	      ip_addr_cmp(&(pcb->local_ip), &(iphdr->dest))) {
	    /* We don't really care enough to move this PCB to the front
	     * of the list since we are not very likely to receive that
	     * many segments for connections in TIME-WAIT. */
	    DEBUG_DEMUX
	      kprintf(KR_OSTREAM,"Demux:packed for TIME_WAITing connection");
	    return;	  
	  }
	}
      }
      
      if(pcb == NULL) {
	/* Finally, if we still did not get a match, we check all PCBs that
	 * are LISTENing for incoming connections. */
	prev = NULL;  
	lpcb = tcp_listen_pcbs;
	DEBUG_DEMUX
	  kprintf(KR_OSTREAM,"Listen pcb lip=%08x, lp = %d,dest = %08x,dep=%d",
		  lpcb->local_ip.addr,lpcb->local_port,iphdr->dest.addr,
		  dest);
	for(lpcb = tcp_listen_pcbs; lpcb != NULL; lpcb = lpcb->next) {
	  if ((ip_addr_isany(&(lpcb->local_ip)) ||
	       ip_addr_cmp(&(lpcb->local_ip), &(iphdr->dest))) &&
	      lpcb->local_port == dest) {	  
	    /* Move this PCB to the front of the list so that subsequent
	     * lookups will be faster (we exploit locality in TCP segment
	     * arrivals). */
	    if (prev != NULL) {
	      ((struct tcp_pcb_listen *)prev)->next = lpcb->next;
	      /* our successor is the remainder of the listening list */
	      lpcb->next = tcp_listen_pcbs;
	      /* put this listening pcb at the head of the listening list */
	      tcp_listen_pcbs = lpcb; 
	    }
	    DEBUG_DEMUX
	      kprintf(KR_OSTREAM,"tcp_input: packed for LISTENing connection");
	    break;
	  }
	  prev = (struct tcp_pcb *)lpcb;
	}
      }
      
      if(pcb!=NULL) ssid = pcb->ssid;
      else if(lpcb!=NULL) ssid = lpcb->ssid;
      else ssid = 0;
      
      PARANOIA_DEMUX
	kprintf(KR_OSTREAM,"pcb of conn = %d hdrlen = %d,pkt_len = %d",
		ssid,hdrlen,pkt_len);
      if(ssid != 0) {
	stack_p = copy_data_in(p,hdrlen,RECV_STACK_SPACE,
			       &((char*)p)[hdrlen],pkt_len-hdrlen,
			       RECV_CLIENT_SPACE,ssid);
	if(stack_p == NULL) return;
      }else {
	stack_p = copy_data_in(p,pkt_len,RECV_STACK_SPACE,
			       NULL,0,RECV_CLIENT_SPACE,ssid);
	if(stack_p == NULL) return;
      }
      
      stack_p->status = PSTORE_READY;
      release_rcvr = 1;
      
      return;
    }
      
    bad:
      kprintf(KR_OSTREAM,
	      "Transport Header not present!No support for eager demuxing");
      return;
      break;
    case(IP_PROTO_ICMP):  {
      struct pstore *stack_p;
      
      DEBUG_DEMUX
	kprintf(KR_OSTREAM,">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>icmp pkt??");
      
      stack_p = copy_data_in(p,pkt_len,RECV_STACK_SPACE,
			     NULL,0,RECV_CLIENT_SPACE,0);
      if(stack_p == NULL) return;

      stack_p->status = PSTORE_READY;
      release_rcvr = 1;
      
      return;
    }
      
    default:
      /* default type of ip pkt */
      break;
    }
    break;
  }
  case ETHERTYPE_ARP: {
    /* Allocate pstores out of the stack */
    struct pstore *stack_p;
    
    DEBUG_DEMUX 
      kprintf(KR_OSTREAM,">>>>>>>>>>>>>>>>>>>>>>>>>>>>demux:: ARP packet\n");
    
    stack_p = copy_data_in(p,pkt_len,RECV_STACK_SPACE,
			   NULL,0,RECV_CLIENT_SPACE,0);
    if(stack_p == NULL) return;
    
    stack_p->status = PSTORE_READY;
    release_rcvr = 1;

    break;
  }
  default:
    kprintf(KR_OSTREAM,"Transport header not TCP||UDP");
  }
  return;
}
