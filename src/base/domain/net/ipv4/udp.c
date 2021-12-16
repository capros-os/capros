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

/* UDP functions */ 
#include <stddef.h>
#include <eros/target.h>
#include <eros/endian.h>
#include <eros/Invoke.h>

#include <domain/domdbg.h>
#include <domain/NetSysKey.h>

#include <string.h>

#include "keyring.h"
#include "Session.h"

#include "include/inet.h"
#include "include/udp.h"
#include "include/icmp.h"
#include "include/memp.h"

#include "netif/netif.h"

#define DEBUGUDP if(0)

void 
udp_init(void) 
{
  udp_pcbs = pcb_cache = NULL;
}

/* Bind a UDP PCB. Add it to the active pcb list. If already exists
 * just return */
uint32_t
udp_bind(struct udp_pcb *pcb,struct ip_addr *ipaddr, uint16_t port) 
{
  struct udp_pcb *ipcb;
  uint8_t rebind;
  
  rebind = 0;
  
  /* Insert UDP pcb into list of active udp pcbs */
  for(ipcb = udp_pcbs; ipcb!= NULL; ipcb = ipcb->next) {
    if(pcb == ipcb) {
      /* Already on the list, just return */
      rebind = 1;
    }
  }
  ip_addr_set(&pcb->local_ip,ipaddr);
  
  if(port == 0) {
#ifndef UDP_LOCAL_PORT_RANGE_START
#define UDP_LOCAL_PORT_RANGE_START 4096
#define UDP_LOCAL_PORT_RANGE_END   0x7fff
#endif
    port = UDP_LOCAL_PORT_RANGE_START;
    ipcb = udp_pcbs;
    while ((ipcb != NULL) && (port != UDP_LOCAL_PORT_RANGE_END)) {
      if (ipcb->local_port == port) {
        port++;
        ipcb = udp_pcbs;
      } else
	ipcb = ipcb->next;
    }
    if (ipcb != NULL) {
      /* no more ports available in local range */
      return RC_NetSys_PortInUse;
    }   
  }
  pcb->local_port = port;
  
  /* Need to place the active PCB on the list */
  if(rebind == 0) {
    pcb->next = udp_pcbs;
    udp_pcbs = pcb;
  }
  
  DEBUGUDP kprintf(KR_OSTREAM,"udp bind successful");
  return RC_OK;
}


/* Associate the UDP PCB with the remote address */
uint32_t 
udp_connect(struct udp_pcb *pcb,struct ip_addr *ipaddr, uint16_t port)
{
  struct udp_pcb *ipcb;
  
  
  if (pcb->local_port == 0) {
    uint32_t err = udp_bind(pcb, &pcb->local_ip, pcb->local_port);
    if (err != RC_OK)
      return err;
  }
    
  ip_addr_set(&pcb->remote_ip, ipaddr);
  pcb->remote_port = port;
  pcb->flags |= UDP_FLAGS_CONNECTED;

  /* Insert UDP PCB into the list of active UDP PCBs. */
  for(ipcb = udp_pcbs; ipcb != NULL; ipcb = ipcb->next) {
    if(pcb == ipcb) {
      /* Already on the list, just return. */
      return RC_OK;
    }
  }
  /* We need to place the PCB on the list. */
  pcb->next = udp_pcbs;
  udp_pcbs = pcb;
  
  DEBUGUDP kprintf(KR_OSTREAM,"udp connect successful");
  return RC_OK;
}

void
udp_disconnect(struct udp_pcb *pcb)
{
  pcb->flags &= ~UDP_FLAGS_CONNECTED;
}


/* The PCB is removed from the list of UDP PCB's and the data structure
 * is freed for others */
void
udp_remove(struct udp_pcb *pcb) 
{
  struct udp_pcb *ipcb;
  
  /* pcb to be removed is first in list? */
  if (udp_pcbs == pcb) {
    /* make list start at 2nd pcb */
    udp_pcbs = udp_pcbs->next;
    /* pcb not 1st in list */
  } else for(ipcb = udp_pcbs; ipcb != NULL; ipcb = ipcb->next) {
    /* find pcb in udp_pcbs list */
    if(ipcb->next != NULL && ipcb->next == pcb) {
      /* remove pcb from list */
      ipcb->next = pcb->next;
    }
  }

  /* "Free memory" allocated for udp_pcb */
  memp_free(MEMP_UDP_PCB,pcb);
}


/* Create a UDP PCB */
struct udp_pcb *
udp_new(void) {
  struct udp_pcb *pcb;
  
  pcb = memp_alloc(MEMP_UDP_PCB);
  if(pcb != NULL) {
    bzero(pcb,sizeof(struct udp_pcb));
  }
  DEBUGUDP kprintf(KR_OSTREAM,"udp new successful");
  return pcb;
}

/* Send data using udp.*/
uint32_t
udp_send(struct udp_pcb *pcb, struct pbuf *p)
{
  struct udp_hdr *udphdr;
  struct ip_addr *src_ip=NULL;
  uint32_t err;
  struct pbuf *q=NULL; /* q will be sent down the stack*/
  
  if (pcb->local_port == 0) {
    err = udp_bind(pcb, &pcb->local_ip, pcb->local_port);
    DEBUGUDP kprintf(KR_OSTREAM,"udp_send: Not yet bound to a local port");
    return err;
  }

  if(pbuf_header(p, UDP_HLEN)) {
    q = pbuf_alloc(PBUF_IP, UDP_HLEN, PBUF_RAM);
    if(q == NULL) return RC_NetSys_PbufsExhausted;
    pbuf_chain(q, p);
  }else {
    q = p;
  }
  
  udphdr = q->payload;
  udphdr->src = htons(pcb->local_port);
  udphdr->dest = htons(pcb->remote_port);
  udphdr->chksum = 0x0000;

  /* using IP_ANY_ADDR? */
  if (ip_addr_isany(&pcb->local_ip)) {
    /* use outgoing network interface IP address as source address */
    src_ip = &(NETIF.ip_addr);
  } else {
    /* use UDP PCB local IP address as source address */
    src_ip = &(pcb->local_ip);
  }
  
  //if((netif = ip_route(&(pcb->remote_ip))) == NULL)  return ERR_RTE;
  if(pcb->flags & UDP_FLAGS_UDPLITE) {
    udphdr->len = htons(pcb->chksum_len);
    /* calculate checksum */
    udphdr->chksum = inet_chksum_pseudo(p,src_ip, &(pcb->remote_ip),
                                        IP_PROTO_UDP, pcb->chksum_len);
    /* Avoid collision if chksum is 0x0000 (no chksum) */
    if(udphdr->chksum == 0x0000) udphdr->chksum = 0xffff;
    err = ip_output(q,src_ip,&pcb->remote_ip,UDP_TTL, 
		    IP_PROTO_UDPLITE); 
  } else {
    udphdr->len = htons(p->tot_len);
    /* calculate checksum */
    if((pcb->flags & UDP_FLAGS_NOCHKSUM) == 0) {
      udphdr->chksum = inet_chksum_pseudo(p, src_ip, &pcb->remote_ip,
                                          IP_PROTO_UDP, p->tot_len);
      
      if(udphdr->chksum == 0x0000)  udphdr->chksum = 0xffff;
    }
    err = ip_output(q,src_ip,&pcb->remote_ip,UDP_TTL,IP_PROTO_UDP);    
  }
  return err;
}


void
udp_input(struct pbuf *p)
{
  struct udp_hdr *udphdr;
  struct udp_pcb *pcb;
  struct ip_hdr *iphdr;
  uint16_t src, dest;
  
  iphdr = p->payload;
  
  if (pbuf_header(p, -((int16_t)(UDP_HLEN + IPH_HL(iphdr) * 4)))) {
    /* drop short packets */
    pbuf_free(p);
    return;
  }

  udphdr = (struct udp_hdr *)((uint8_t *)p->payload - UDP_HLEN);

  src = ntohs(udphdr->src);
  dest = ntohs(udphdr->dest);
  
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
  
  /* Check checksum if this is a match or if it was directed at us. */
  if(pcb != NULL || ip_addr_cmp(&(NETIF.ip_addr), &iphdr->dest)) {
    pbuf_header(p, UDP_HLEN);
    if(IPH_PROTO(iphdr) == IP_PROTO_UDPLITE) {
      /* Do the UDP Lite checksum */
      if(inet_chksum_pseudo(p, (struct ip_addr *)&(iphdr->src),
			    (struct ip_addr *)&(iphdr->dest),
			    IP_PROTO_UDPLITE, ntohs(udphdr->len)) != 0) {
	pbuf_free(p);
	return;
      }
    } else {
      if(udphdr->chksum != 0) {
	if(inet_chksum_pseudo(p, (struct ip_addr *)&(iphdr->src),
			      (struct ip_addr *)&(iphdr->dest),
			      IP_PROTO_UDP, p->tot_len) != 0) {
	  pbuf_free(p);
	  return;
	}
      }
    }
    
    pbuf_header(p, -UDP_HLEN);
    if(pcb != NULL) {
      /* If our client has filled up this receive function call it*/
      if(pcb->recv != NULL) {
	pcb->recv(pcb->recv_arg, pcb, p, &(iphdr->src), src);
      }else {
	if(pcb->ssid != 0 && pcb->listening == 1){
	  pcb->pbuf = p;
	  wakeSession(pcb->ssid);
	  pcb->listening = 1;
	}else {
	  pbuf_free(p);
	}
      }
    } else {
#if 0
      /* No match was found, send ICMP destination port unreachable unless
	 destination address was broadcast/multicast. */
      if(!ip_addr_isbroadcast(&iphdr->dest, &(NETIF.netmask)) &&
	 !ip_addr_ismulticast(&iphdr->dest)) {
      
	/* deconvert from host to network byte order */
	udphdr->src = htons(udphdr->src);
	udphdr->dest = htons(udphdr->dest);
	
	/* adjust pbuf pointer */
	p->payload = iphdr;
	icmp_dest_unreach(p, ICMP_DUR_PORT);
      }
#endif
      pbuf_free(p);
    }
  } else {
    pbuf_free(p);
  }
}


void
udp_recv(struct udp_pcb *pcb,
         void (* recv)(void *arg, struct udp_pcb *upcb, struct pbuf *p,
                       struct ip_addr *addr, uint16_t port),
         void *recv_arg)
{
  /* remember recv() callback and user data */
  pcb->recv = recv;
  pcb->recv_arg = recv_arg;
}
