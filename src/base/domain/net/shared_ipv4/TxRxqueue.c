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

#include <string.h>
#include <eros/target.h>
#include <eros/endian.h>
#include <eros/Invoke.h>
#include <eros/NodeKey.h>

#include <domain/domdbg.h>

#include "include/ip_addr.h"
#include "include/TxRxqueue.h"
#include "include/pstore.h"
#include "include/ethernet.h"
#include "include/ip.h"
#include "include/etharp.h"

#include "netif/netif.h"
#include "netsyskeys.h"
#include "Session.h"

#define DEBUG_TXRX if(0)

extern struct session ActiveSessions[MAX_SESSIONS];
extern int release_xmitr;

struct eth_addr ethbroadcast = {{0xff,0xff,0xff,0xff,0xff,0xff}};

uint32_t 
pstoreQ_init(uint32_t start_addr,struct pstore_queue **PQ) 
{
  /* Our queues start from here 2*Qstart (just for no reason) */
  uint32_t Qstart = PSTORE_POOL_SIZE*(PSTORE_POOL_BUFSIZE
				      + sizeof(struct pstore));
  
  struct pstore_queue *pq = (void *)(2*Qstart + start_addr);
  *PQ = &pq[0];
  DEBUG_TXRX kprintf(KR_OSTREAM,"TX/Rx Queue starts at %08x",*PQ);
  return RC_OK;
}
/* Prepare the ethernet header for this packet by looking at the arp
 * cache. If it is a broadcast packet then we are already done. Then
 * go wake up the parked xmit_helper and this time let it go */
uint32_t 
netif_start_xmit(struct pstore *p,int type,struct ip_addr *ipaddr,int ssid)
{
  struct eth_addr mcastaddr,*dest=NULL;
  struct ip_addr *queryaddr;
  char *s = NULL;
  
  if(type != -1) {
    /* Construct Ethernet header. Start with looking up deciding which
     * MAC address to use as a destination address. Broadcasts and
     * multicasts are special, all other addresses are looked up in the
     * ARP table. */
    if (ip_addr_isany(ipaddr) ||
        ip_addr_isbroadcast(ipaddr, &(NETIF.netmask))) {
      dest = (struct eth_addr *)&ethbroadcast;
    } else if (ip_addr_ismulticast(ipaddr)) {
      /* Hash IP multicast address to MAC address. */
      mcastaddr.addr[0] = 0x01;
      mcastaddr.addr[1] = 0x0;
      mcastaddr.addr[2] = 0x5e;
      mcastaddr.addr[3] = ip4_addr2(ipaddr) & 0x7f;
      mcastaddr.addr[4] = ip4_addr3(ipaddr);
      mcastaddr.addr[5] = ip4_addr4(ipaddr);
      dest = &mcastaddr;
    }else {
      if (ip_addr_maskcmp(ipaddr, &(NETIF.ip_addr), &(NETIF.netmask))) {
        /* Use destination IP address if the destination is on the same
         * subnet as we are. */
        queryaddr = ipaddr;
      } else {
        /* Otherwise we use the default router as the address to send
         * the Ethernet frame to. */
        queryaddr = &(NETIF.gw);
      }
      dest = etharp_lookup(&NETIF,queryaddr,p,ssid);
    }
    if(dest == NULL) {
      pstore_free(p,ssid);
      return RC_OK;
    }
    pstore_header(p,ETHER_HDR_LEN,ssid);
    s = PSTORE_PAYLOAD(p,ssid);
    
    memcpy(&s[0],&dest->addr[0],ETHER_ADDR_LEN);
    memcpy(&s[ETHER_ADDR_LEN],&NETIF.hwaddr[0],ETHER_ADDR_LEN);
    
    s[ETHER_ADDR_LEN*2] = type >> 8;
    s[ETHER_ADDR_LEN*2+1] = type;
  }
  
  p->status = PSTORE_READY;
  DEBUG_TXRX 
    kprintf(KR_OSTREAM,"netsys:Sending packet sector = %d stat=%d add=%08x",
	    p->sector,p->status,&p[0]);
  
  release_xmitr = 1;

  return RC_OK;
}

/* Call the appropriate function, demuxing the packet as a result */
uint32_t 
rx_service() 
{
  int i,ssid,*cur_p;
  uint8_t sect = RECV_STACK_SPACE;
  uint32_t start_address;
  struct pstore *p;
  struct eth_hdr *ethhdr = NULL;
  bool work_done = 0;
  
  /* For all the available sessions */
  for(i=0;i<MAX_SESSIONS;i++){ 
    /* Check to see if valid session exists */
    if(ActiveSessions[i].ssid != -1) {
      /* Load up all our temporary variables for this session */
      ssid = i;
      cur_p = &ActiveSessions[ssid].mt[sect].cur_p;
      start_address = ActiveSessions[ssid].mt[sect].start_address;
      
      /* Our next buffer to look at */
      p = (void *)(start_address + 
		   (*cur_p)*(PSTORE_POOL_BUFSIZE + sizeof(struct pstore)));
      
      /* We don't have any filled up buffer to service */
     if(p->status == PSTORE_READY) {
       DEBUG_TXRX
	  kprintf(KR_OSTREAM,"rx_recv:ssid=%d s=%d add=%08x len=%d stat=%d",
		  ssid,sect,&p[0],p->tot_len,p->status);
	
	/* Else advance our pointer */
	*cur_p = (*cur_p + 1) >= PSTORE_POOL_SIZE ? 0 : *cur_p + 1;
	
	/* Else, peek at the header info and appropriately demultiplex */
	ethhdr = (void *)(start_address + p->offset);
	
	switch(htons(ethhdr->type)) {
	case ETHERTYPE_IP:
	  /* Add to arp table here */
	  etharp_ip_input(&NETIF,p,ssid); 
	  pstore_header(p,-ETHER_HDR_LEN,ssid);
	  ip_input(p,ssid);
	  break;
	case ETHERTYPE_ARP:
	  p = etharp_arp_input(&NETIF,
			       (struct eth_addr *)&(NETIF.hwaddr),p,ssid);
	  break;
	default:
	  DEBUG_TXRX kprintf(KR_OSTREAM,"default packet received");
	  pstore_free(p,ssid);
	}
#if 0
	/* Our next buffer to look at */
	if(0 == *cur_p) p = (void *)start_address;
	else p = (void*)((uint32_t)&p[0] + PSTORE_POOL_BUFSIZE + 
			 sizeof(struct pstore));
	/* Remember the last session serviced */
	work_done = 1;
#endif	
	return 1;
      }
    }
  }

  return work_done;
}

