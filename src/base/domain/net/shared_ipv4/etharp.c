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

/**
 * Address Resolution Protocol module for IP over Ethernet
 *
 * Functionally, ARP is divided into two parts. The first maps an IP address
 * to a physical address when sending a packet, and the second part answers
 * requests from other machines.
 */
 
/*
 * TODO:
 *
 RFC 3220 4.6          IP Mobility Support for IPv4          January 2002 

 -  A Gratuitous ARP [45] is an ARP packet sent by a node in order 
 to spontaneously cause other nodes to update an entry in their 
 ARP cache.  A gratuitous ARP MAY use either an ARP Request or 
 an ARP Reply packet.  In either case, the ARP Sender Protocol 
 Address and ARP Target Protocol Address are both set to the IP 
 address of the cache entry to be updated, and the ARP Sender 
 Hardware Address is set to the link-layer address to which this 
 cache entry should be updated.  When using an ARP Reply packet, 
 the Target Hardware Address is also set to the link-layer 
 address to which this cache entry should be updated (this field 
 is not used in an ARP Request packet). 

 In either case, for a gratuitous ARP, the ARP packet MUST be 
 transmitted as a local broadcast packet on the local link.  As 
 specified in [36], any node receiving any ARP packet (Request 
 or Reply) MUST update its local ARP cache with the Sender 
 Protocol and Hardware Addresses in the ARP packet, if the 
 receiving node has an entry for that IP address already in its 
 ARP cache.  This requirement in the ARP protocol applies even 
 for ARP Request packets, and for ARP Reply packets that do not 
 match any ARP Request transmitted by the receiving node [36]. 
 *
 My suggestion would be to send a ARP request for our newly obtained
 address upon configuration of an Ethernet interface.
*/

#include <string.h>
#include <eros/target.h>
#include <eros/endian.h>
#include <eros/Invoke.h>

#include <domain/domdbg.h>

#include "netsyskeys.h"

#include "include/inet.h"
#include "include/etharp.h"
#include "include/ip.h"
#include "include/dhcp.h"
#include "include/TxRxqueue.h"
#include "netif/netif.h"
#include "Session.h"

/* Remove from here. Move to another file later*/
#define RC_ERR_MEM   90

#define ARP_MAXAGE 120   /* the time an ARP entry stays valid after its
			  * last update, (120 * 10) seconds = 20 minutes. */
#define ARP_MAXPENDING 2 /* the time an ARP entry stays pending after 
			  * first request, (2 * 10) seconds = 20 seconds. */

#define HWTYPE_ETHERNET 1

/* ARP message types */
#define ARP_REQUEST 1
#define ARP_REPLY 2

#define ARPH_HWLEN(hdr) (ntohs((hdr)->_hwlen_protolen) >> 8)
#define ARPH_PROTOLEN(hdr) (ntohs((hdr)->_hwlen_protolen) & 0xff)

#define ARPH_HWLEN_SET(hdr, len)    (hdr)->_hwlen_protolen = \
                                     htons(ARPH_PROTOLEN(hdr) | ((len) << 8))
#define ARPH_PROTOLEN_SET(hdr, len) (hdr)->_hwlen_protolen = \
                                     htons((len) | (ARPH_HWLEN(hdr) << 8))

#define DEBUG_ETHARP if(0)

extern struct session ActiveSessions[MAX_SESSIONS];

enum etharp_state {
  ETHARP_STATE_EMPTY,
  ETHARP_STATE_PENDING,
  ETHARP_STATE_STABLE
};

struct etharp_entry {
  struct ip_addr ipaddr;
  struct eth_addr ethaddr;
  enum etharp_state state;
  struct pstore *p;
  uint8_t ctime;
};

static const struct eth_addr ethbroadcast = {{0xff,0xff,0xff,0xff,0xff,0xff}};
static struct etharp_entry arp_table[ARP_TABLE_SIZE];
static struct pstore *update_arp_entry(struct netif *netif, 
				     struct ip_addr *ipaddr,
				     struct eth_addr *ethaddr, uint8_t flags);
#define ARP_INSERT_FLAG 1

/* Initializes ARP module.*/
void
etharp_init(void)
{
  uint8_t i;
  /* clear ARP entries */
  for(i = 0; i < ARP_TABLE_SIZE; ++i) {
    arp_table[i].state = ETHARP_STATE_EMPTY;
#if ARP_QUEUEING
    arp_table[i].p = NULL;
#endif
  }
}

/* Clears expired entries in the ARP table.
 * This function should be called every ETHARP_TMR_INTERVAL
 * microseconds (10 seconds), in order to expire entries in the ARP table.
 */
void
etharp_tmr(void)
{
  uint8_t i;
  
  /* remove expired entries from the ARP table */
  for (i = 0; i < ARP_TABLE_SIZE; ++i) {
    arp_table[i].ctime++;
    if ((arp_table[i].state == ETHARP_STATE_STABLE) &&       
        (arp_table[i].ctime >= ARP_MAXAGE)) {
      arp_table[i].state = ETHARP_STATE_EMPTY;
#if ARP_QUEUEING
      /* remove any queued packet */
      pstore_free(arp_table[i].p);      
      arp_table[i].p = NULL;
#endif
    } else if ((arp_table[i].state == ETHARP_STATE_PENDING) &&
	       (arp_table[i].ctime >= ARP_MAXPENDING)) {
      arp_table[i].state = ETHARP_STATE_EMPTY;
#if ARP_QUEUEING
      /* remove any queued packet */
      pstore_free(arp_table[i].p);      
      arp_table[i].p = NULL;
#endif
    }
  }  
}

/* Return an empty ARP entry or, if the table is full, ARP_TABLE_SIZE if all
 * entries are pending, otherwise the oldest entry.
 *
 * @return The ARP entry index that is available, ARP_TABLE_SIZE if no usable
 * entry is found.
 */
static uint8_t
find_arp_entry(void)
{
  uint8_t i, j, maxtime;
  
  /* Try to find an unused entry in the ARP table. */
  for (i = 0; i < ARP_TABLE_SIZE; ++i) {
    if (arp_table[i].state == ETHARP_STATE_EMPTY) {
      DEBUG_ETHARP 
	kprintf(KR_OSTREAM,"find_arp_entry: found empty entry %d", i);
      break;
    }
  }
  
  /* If no unused entry is found, we try to find the oldest entry and
     throw it away. If all entries are new and have 0 ctime drop one  */
  if (i == ARP_TABLE_SIZE) {
    maxtime = 0;
    j = ARP_TABLE_SIZE;
    for (i = 0; i < ARP_TABLE_SIZE; ++i) {
      /* remember entry with oldest stable entry in j*/
      if ((arp_table[i].state == ETHARP_STATE_STABLE) &&
#if ARP_QUEUEING /* do not want to re-use an entry with queued packets */
	  (arp_table[i].p == NULL) &&
#endif
	  (arp_table[i].ctime >= maxtime)) {
        maxtime = arp_table[i].ctime;
	j = i;
      }
    }
    if (j != ARP_TABLE_SIZE) {
      kprintf(KR_OSTREAM,"find_arp_entry:found oldest stable entry %d",j);
    } else {
      kprintf(KR_OSTREAM,"find_arp_entry:no replacable entry could be found");
    }
    i = j;
  }
  DEBUG_ETHARP 
    kprintf(KR_OSTREAM,"find_arp_entry:ret %d,state %d",i,arp_table[i].state);
  return i;
}

/* Update (or insert) a IP/MAC address pair in the ARP cache.
 *
 * @param ipaddr IP address of the inserted ARP entry.
 * @param ethaddr Ethernet address of the inserted ARP entry.
 * @param flags Defines behaviour:
 * - ARP_INSERT_FLAG Allows ARP to insert this as a new item. If not specified,
 * only existing ARP entries will be updated.
 *
 * @return pstore If non-NULL, a packet that was queued on a pending entry.
 * You should sent it and must call pstore_free() afterwards.
 *
 * @see pstore_free()
 */
static struct pstore *
update_arp_entry(struct netif *netif, struct ip_addr *ipaddr,
		 struct eth_addr *ethaddr, uint8_t flags)
{
  uint8_t i, k;

  DEBUG_ETHARP 
    kprintf(KR_OSTREAM,"update_arp_entry:%d.%d.%d.%d-%2x:%2x:%2x:%x:%2x:%2x", 
	    ip4_addr1(ipaddr), ip4_addr2(ipaddr), 
	    ip4_addr3(ipaddr), ip4_addr4(ipaddr),
	    ethaddr->addr[0], ethaddr->addr[1], ethaddr->addr[2], 
	    ethaddr->addr[3], ethaddr->addr[4], ethaddr->addr[5]);
  
  /* do not update for 0.0.0.0 addresses */
  if (ipaddr->addr == 0) {
    kprintf(KR_OSTREAM,"update_arp_entry:will not add 0.0.0.0 to ARP cache");
    return NULL;
  }
  
  /* Walk through the ARP mapping table and try to find an entry to
   * update. If none is found, the IP -> MAC address mapping is
   * inserted in the ARP table. */
  for (i = 0; i < ARP_TABLE_SIZE; ++i) {
    /* Check if the source IP address of the incoming packet matches
     * the IP address in this ARP table entry. */
    if (ip_addr_cmp(ipaddr, &arp_table[i].ipaddr)) {
      /* pending entry? */
      if (arp_table[i].state == ETHARP_STATE_PENDING) {
	DEBUG_ETHARP 
	  kprintf(KR_OSTREAM,"updateArpEntry:pending entry %d goes stable",i);
	/* A pending entry was found, mark it stable */
	arp_table[i].state = ETHARP_STATE_STABLE;
	/* fall-through to next if */
      }
      /* stable entry? (possible just marked to become stable) */
      if (arp_table[i].state == ETHARP_STATE_STABLE) {
#if ARP_QUEUEING
	struct pstore *p;
	struct eth_hdr *ethhdr;
#endif
	DEBUG_ETHARP 
	  kprintf(KR_OSTREAM,"update_arp_entry:updating stable entry %d",i);
	/* An old entry found, update this and return. */
	for (k = 0; k < netif->hwaddr_len; ++k) {
	  arp_table[i].ethaddr.addr[k] = ethaddr->addr[k];
	}
	/* reset time stamp */
	arp_table[i].ctime = 0;
#if ARP_QUEUEING
	p = arp_table[i].p;
	/* queued packet present? */
	if (p != NULL) {
	  /* NULL attached buffer immediately */
	  arp_table[i].p = NULL;
	  /* fill-in Ethernet header */
	  ethhdr = p->payload;
	  for (k = 0; k < netif->hwaddr_len; ++k) {
	    ethhdr->dest.addr[k] = ethaddr->addr[k];
	  }
	  ethhdr->type = htons(ETHTYPE_IP);	  	 	  
	  DEBUG_ETHARP
	    kprintf(KR_OSTREAM,"update_arp_entry:sending queued IP packet");
	  /* send the queued IP packet */
	  netif_start_xmit(p);
	  /* free the queued IP packet */
	  pstore_free(p);
	}
#endif
	return NULL;
      }
    } /* if */
  } /* for */
  
  /* no matching ARP entry was found */
  DEBUG_ETHARP 
    kprintf(KR_OSTREAM,"update_arp_entry:IP address not yet in table");
  /* allowed to insert an entry? */
  if ((ETHARP_ALWAYS_INSERT) || (flags & ARP_INSERT_FLAG)) {
    DEBUG_ETHARP kprintf(KR_OSTREAM,"update_arp_entry:adding entry to table");
    /* find an empty or old entry. */
    i = find_arp_entry();
    if (i == ARP_TABLE_SIZE) {
      DEBUG_ETHARP kprintf(KR_OSTREAM,"update_arp_entry:no available entry found");
      return NULL;
    }
    /* see if find_arp_entry() gave us an old stable,
     * or empty entry to re-use */
    if (arp_table[i].state == ETHARP_STATE_STABLE) {
      DEBUG_ETHARP
	kprintf(KR_OSTREAM,"updatearpentry:overwriting old stable entry %d",i);
      /* stable entries should have no queued packets (TODO: allow later) */
    } else {
      DEBUG_ETHARP
	kprintf(KR_OSTREAM,
		"update_arp_entry:filling empty entry %d "
		"with state %d", i, arp_table[i].state);
    }
    /* set IP address */  
    ip_addr_set(&arp_table[i].ipaddr, ipaddr);
    /* set Ethernet hardware address */  
    for (k = 0; k < netif->hwaddr_len; ++k) {
      arp_table[i].ethaddr.addr[k] = ethaddr->addr[k];
    }
    /* reset time-stamp */  
    arp_table[i].ctime = 0;
    /* mark as stable */  
    arp_table[i].state = ETHARP_STATE_STABLE;
    /* no queued packet */  
#if ARP_QUEUEING
    arp_table[i].p = NULL;
#endif
  }
  else {
    kprintf(KR_OSTREAM,"update_arp_entry:no matching stable entry to update");
  }
  return NULL;
}

/**
 * Updates the ARP table using the given packet.
 *
 * Uses the incoming IP packet's source address to update the
 * ARP cache for the local network. The function does not alter
 * or free the packet. This function must be called before the
 * packet p is passed to the IP layer.
 *
 * @param netif The lwIP network interface on which the IP packet pstore arrived.
 * @param pstore The IP packet that arrived on netif.
 * 
 * @return NULL
 *
 * @see pstore_free()
 */
struct pstore *
etharp_ip_input(struct netif *netif, struct pstore *p,int ssid)
{
  struct ethip_hdr *hdr;
  
  /* Only insert an entry if the source IP address of the
   * incoming IP packet comes from a host on the local network. */
  hdr = PSTORE_PAYLOAD(p,ssid);
  
  /* source is not on local network? */
  if (!ip_addr_maskcmp(&(hdr->ip.src), &(netif->ip_addr), &(netif->netmask))) {
    /* do nothing */
    return NULL;
  }
  
  DEBUG_ETHARP kprintf(KR_OSTREAM,"etharp_ip_input: updating ETHARP table");
  /* update ARP table, ask to insert entry */
  update_arp_entry(netif, &(hdr->ip.src),&(hdr->eth.src), ARP_INSERT_FLAG);
  return NULL;
}

/**
 * Responds to ARP requests, updates ARP entries and sends queued IP packets.
 * 
 * Should be called for incoming ARP packets. The pstore in the argument
 * is freed by this function.
 *
 * @param netif The lwIP network interface on which the ARP packet pstore arrived
 * @param pstore The ARP packet that arrived on netif. Is freed by this function.
 * @param ethaddr Ethernet address of netif.
 *
 * @return NULL
 *
 * @see pstore_free()
 */
struct pstore *
etharp_arp_input(struct netif *netif, struct eth_addr *ethaddr, 
		 struct pstore *p,int ssid)
{
  struct etharp_hdr *hdr;
  struct pstore *q, *r ,*oldp;
  uint8_t i;
  int32_t nextoffset;
  
  /* drop short ARP packets */
  if (p->tot_len < sizeof(struct etharp_hdr)) {
    kprintf(KR_OSTREAM,"etharp_arp_input:packet too short(%d/%d)",
	    p->tot_len, sizeof(struct etharp_hdr));
    pstore_free(p,ssid);
    return NULL;
  }
  
  hdr = PSTORE_PAYLOAD(p,ssid);
  
  switch (htons(hdr->opcode)) {
    /* ARP request? */
  case ARP_REQUEST:
    /* ARP request. If it asked for our address, we send out a
     * reply. In any case, we time-stamp any existing ARP entry,
     * and possiby send out an IP packet that was queued on it. */
    
    DEBUG_ETHARP {
      kprintf(KR_OSTREAM,"etharp_arp_input:incoming ARP request");
      kprintf(KR_OSTREAM,"looking for %d.%d.%d.%d",
	      ip4_addr1(&(hdr->dipaddr)), ip4_addr2(&(hdr->dipaddr)), 
	      ip4_addr3(&(hdr->dipaddr)), ip4_addr4(&(hdr->dipaddr)));
    }
    
    /* we are not configured? */
    if (netif->ip_addr.addr == 0) {
      kprintf(KR_OSTREAM,
	      "etharp_arp_input:we are unconfigured, "
	      "ARP request ignored.");
      pstore_free(p,ssid);
      return NULL;
    }
    /* update the ARP cache */
    update_arp_entry(netif, &(hdr->sipaddr), &(hdr->shwaddr), 0);
    /* ARP request for our address? */
    if (ip_addr_cmp(&(hdr->dipaddr), &(netif->ip_addr))) {
      DEBUG_ETHARP
	kprintf(KR_OSTREAM,
		"etharp_arp_input:replying to ARP request "
		"for our IP address");

      hdr->opcode = htons(ARP_REPLY);
      
      ip_addr_set(&(hdr->dipaddr), &(hdr->sipaddr));
      ip_addr_set(&(hdr->sipaddr), &(netif->ip_addr));
      
      for(i = 0; i < netif->hwaddr_len; ++i) {
        hdr->dhwaddr.addr[i] = hdr->shwaddr.addr[i];
        hdr->shwaddr.addr[i] = ethaddr->addr[i];
        hdr->ethhdr.dest.addr[i] = hdr->dhwaddr.addr[i];
        hdr->ethhdr.src.addr[i] = ethaddr->addr[i];
      }
      
      hdr->hwtype = htons(HWTYPE_ETHERNET);
      ARPH_HWLEN_SET(hdr, netif->hwaddr_len);

      hdr->proto = htons(ETHTYPE_IP);
      ARPH_PROTOLEN_SET(hdr, sizeof(struct ip_addr));      

      hdr->ethhdr.type = htons(ETHTYPE_ARP);      
      
      /* We would have received this on the recv_stack space. Now 
       * copy this pstore into the xmit stack space */
      q = pstore_alloc(PSTORE_TRANSPORT,p->tot_len,XMIT_STACK_SPACE,ssid);
      if(q == NULL) {
	kprintf(KR_OSTREAM,"etharp_arp_input: No available pbuf");
	pstore_free(p,ssid);
	return NULL;
      }
      /* Remember our parent in the chain allocated */
      r = q;
      
      /* Remember p to kill it */
      oldp = p; 
      /* now copy over the contents into the new pstore */
      do {
	nextoffset = p->nextoffset;
	memcpy(PSTORE_PAYLOAD(q,ssid),PSTORE_PAYLOAD(p,ssid),p->len);
	q = PSTORE_NEXT(q,ssid);
	p = PSTORE_NEXT(p,ssid);
      }while(nextoffset != -1);
      
      /* free the original pstore */
      pstore_free(oldp,ssid);
      
      /* switch to oldp */
      p = r;
            
      /* return ARP reply */
      netif_start_xmit(p,-1,&(hdr->dipaddr),ssid);
      return p;
    } else {
      DEBUG_ETHARP
	kprintf(KR_OSTREAM,"arp_input:incoming ARP req. was not for us");
    }
    break;
  case ARP_REPLY:    
    /* ARP reply. We insert or update the ARP table. */
    kprintf(KR_OSTREAM,"etharp_arp_input: incoming ARP reply");
    /* DHCP needs to know about ARP replies */
    //dhcp_arp_reply(netif, &hdr->sipaddr);
    
    /* ARP reply directed to us? */
    if (ip_addr_cmp(&(hdr->dipaddr), &(netif->ip_addr))) {
      DEBUG_ETHARP 
	kprintf(KR_OSTREAM,"etharp_arp_input:incoming ARP reply is for us");
      /* update_the ARP cache, ask to insert */
      update_arp_entry(netif,&(hdr->sipaddr),&(hdr->shwaddr),ARP_INSERT_FLAG);
      /* ARP reply not directed to us */
    } else {
      DEBUG_ETHARP
	kprintf(KR_OSTREAM,"etharp_arp_input:incoming ARP reply isn't for us");
      /* update the destination address pair */
      update_arp_entry(netif, &(hdr->sipaddr), &(hdr->shwaddr), 0);
      /* update the destination address pair */
      update_arp_entry(netif, &(hdr->dipaddr), &(hdr->dhwaddr), 0);
    }
    break;
  default:
    kprintf(KR_OSTREAM,"etharp_arp_input:ARP unknown opcode type %d", 
	    htons(hdr->opcode));
    break;
  }
  /* free ARP packet */
  pstore_free(p,ssid);
  /* nothing to send, we did it! */
  return NULL;
}


/** 
 * Resolve and fill-in Ethernet address header for outgoing packet.
 *
 * If ARP has the Ethernet address in cache, the given packet is
 * returned, ready to be sent.
 *
 * If ARP does not have the Ethernet address in cache the packet is
 * queued and a ARP request is sent (on a best-effort basis). This
 * ARP request is returned as a pstore, which should be sent by the
 * caller.
 *
 * If ARP failed to allocate resources, NULL is returned.
 *
 * A returned non-NULL packet should be sent by the caller.
 *
 * @param netif The lwIP network interface which the IP packet will be sent on.
 * @param ipaddr The IP address of the packet destination.
 * @param pstore The pstore(s) containing the IP packet to be sent.
 * 
 * @return If non-NULL, a packet ready to be sent. 
 */
struct pstore *
etharp_output(struct netif *netif, struct ip_addr *ipaddr, 
	      struct pstore *q,int ssid)
{
  struct eth_addr *dest, *srcaddr, mcastaddr;
  struct eth_hdr *ethhdr;
  uint8_t i;

  /* Make room for Ethernet header. */
  if (pstore_header(q, sizeof(struct eth_hdr),ssid) != 0) {  
    /* The pstore_header() call shouldn't fail, and we'll just bail
     * out if it does.. */
    kdprintf(KR_OSTREAM,"etharp_output: could not allocate room for header");
    return NULL;
  }
  
  /* obtain source Ethernet address of the given interface */
  srcaddr = (struct eth_addr *)netif->hwaddr;
  
  /* assume unresolved Ethernet address */
  dest = NULL;
  /* Construct Ethernet header. Start with looking up deciding which
   * MAC address to use as a destination address. Broadcasts and
   * multicasts are special, all other addresses are looked up in the
   * ARP table. */

  /* destination IP address is an IP broadcast address? */
  if (ip_addr_isany(ipaddr) ||
      ip_addr_isbroadcast(ipaddr, &(netif->netmask))) {
    /* broadcast on Ethernet also */
    dest = (struct eth_addr *)&ethbroadcast;
  }
  /* destination IP address is an IP multicast address? */
  else if (ip_addr_ismulticast(ipaddr)) {
    /* Hash IP multicast address to MAC address. */
    mcastaddr.addr[0] = 0x01;
    mcastaddr.addr[1] = 0x0;
    mcastaddr.addr[2] = 0x5e;
    mcastaddr.addr[3] = ip4_addr2(ipaddr) & 0x7f;
    mcastaddr.addr[4] = ip4_addr3(ipaddr);
    mcastaddr.addr[5] = ip4_addr4(ipaddr);
    /* destination Ethernet address is multicast */
    dest = &mcastaddr;
  }
  /* destination IP address is an IP unicast address */
  else {
    /* destination IP network address not on local network? */
    /* this occurs if the packet is routed to the default gateway 
     * on this interface */
    if (!ip_addr_maskcmp(ipaddr, &(netif->ip_addr), &(netif->netmask))) {
      /* gateway available? */
      if (netif->gw.addr != 0) {
	/* use the gateway IP address */
	ipaddr = &(netif->gw);
      }
      /* no gateway available? */
      else {
	/* IP destination address outside local network, 
	 * but no gateway available */
	return NULL;
      }
    }

    /* Ethernet address for IP destination address is in ARP cache? */
    for(i = 0; i < ARP_TABLE_SIZE; ++i) {
      /* match found? */    
      if (arp_table[i].state == ETHARP_STATE_STABLE &&
	  ip_addr_cmp(ipaddr, &arp_table[i].ipaddr)) {
        dest = &arp_table[i].ethaddr;
        break;
      }
    }
    /* could not find the destination Ethernet address in ARP cache? */
    if (dest == NULL) {
      /* ARP query for the IP address, submit this IP packet for queueing */
      etharp_query(netif, ipaddr, q,ssid);
      /* return nothing */
      return NULL;
    }
    /* destination Ethernet address resolved from ARP cache */
    else {
      /* fallthrough */
    }
  }
  
  /* destination Ethernet address known */
  if (dest != NULL) {
    /* A valid IP->MAC address mapping was found, so we construct the
     * Ethernet header for the outgoing packet. */
    ethhdr = PSTORE_PAYLOAD(q,ssid);
    
    for(i = 0; i < netif->hwaddr_len; i++) {
      ethhdr->dest.addr[i] = dest->addr[i];
      ethhdr->src.addr[i] = srcaddr->addr[i];
    }
    
    ethhdr->type = htons(ETHTYPE_IP);
    /* return the outgoing packet */
    return q;
  }
  /* never reached; here for safety */ 
  return NULL;
}


/**
 * Send an ARP request for the given IP address.
 *
 * Sends an ARP request for the given IP address, unless
 * a request for this address is already pending. Optionally
 * queues an outgoing packet on the resulting ARP entry.
 *
 * @param netif The lwIP network interface where ipaddr
 * must be queried for.
 * @param ipaddr The IP address to be resolved.
 * @param q If non-NULL, a pstore that must be queued on the
 * ARP entry for the ipaddr IP address.
 *
 * @return NULL.
 *
 * @note Might be used in the future by manual IP configuration
 * as well.
 *
 * TODO: use the ctime field to see how long ago an ARP request was sent,
 * possibly retry.
 */
uint32_t 
etharp_query(struct netif *netif, struct ip_addr *ipaddr, 
	     struct pstore *q,int ssid)
{
  struct eth_addr *srcaddr;
  struct etharp_hdr *hdr;
  struct pstore *p;
  uint32_t result = RC_OK;
  uint8_t i;
  uint8_t perform_arp_request = 1;

  /* prevent 'unused argument' warning if ARP_QUEUEING == 0 */
  //(void)q;
  
  srcaddr = (struct eth_addr *)netif->hwaddr;
  /* bail out if this IP address is pending */
  for (i = 0; i < ARP_TABLE_SIZE; ++i) {
    if (ip_addr_cmp(ipaddr, &arp_table[i].ipaddr)) {
      if (arp_table[i].state == ETHARP_STATE_PENDING) {
        kprintf(KR_OSTREAM,
		"etharp_query:requested IP already pending "
		"as entry %d",i);
        /* break out of for-loop, user may wish to queue a packet 
	 * on a stable entry */
        /* TODO: we will issue a new ARP request, which should not occur 
	 * too often we might want to run a faster timer on ARP to limit 
	 * this */
        break;
      }
      else if (arp_table[i].state == ETHARP_STATE_STABLE) {
       kprintf(KR_OSTREAM,
	       "etharp_query:requested IP already stable as "
	       "entry %d", i);
       /* user may wish to queue a packet on a stable entry, 
	* so we proceed without ARP requesting */
       /* TODO: even if the ARP entry is stable, 
	* we might do an ARP request anyway */
       perform_arp_request = 0;
       break;
      }
    }
  }
  /* queried address not yet in ARP table? */
  if (i == ARP_TABLE_SIZE) {
    kprintf(KR_OSTREAM,"etharp_query:IP address not found in ARP table %x",
	    ipaddr->addr);
    /* find an available entry */
    i = find_arp_entry();
    /* bail out if no ARP entries are available */
    if (i == ARP_TABLE_SIZE) {
      kprintf(KR_OSTREAM,"etharp_query: no more ARP entries available");
      return RC_ERR_MEM;
    }
    /* we will now recycle entry i */
    kprintf(KR_OSTREAM,"etharp_query: created ARP table entry %d",i);
    /* i is available, create ARP entry */
    ip_addr_set(&arp_table[i].ipaddr, ipaddr);
    arp_table[i].ctime = 0;
    arp_table[i].state = ETHARP_STATE_PENDING;
#if ARP_QUEUEING
    /* free queued packet, as entry is now invalidated */
    if (arp_table[i].p != NULL) {
      pstore_free(arp_table[i].p,ssid);
      arp_table[i].p = NULL;
      kprintf(KR_OSTREAM,
	      "etharp_query: dropped packet on ARP queue. "
	      "Should not occur");
    }
#endif
  }
#if ARP_QUEUEING
  /* any pstore to queue and queue is empty? */
  if (q != NULL) {
    /* yield later packets over older packets? */
#if ARP_QUEUE_FIRST == 0
    /* earlier queued packet on this entry? */
    if (arp_table[i].p != NULL) {
      pstore_free(arp_table[i].p,ssid);
      arp_table[i].p = NULL;
      kprintf(KR_OSTREAM,
	      "etharp_query:dropped packet on ARP queue. "
	      "Should not occur");
      /* fall-through into next if */
    }
#endif
    /* packet can be queued? */
    if (arp_table[i].p == NULL) {
      /* copy PSTORE_REF referenced payloads into PSTORE_RAM */
      q = pstore_take(q);
      /* remember pstore to queue, if any */
      arp_table[i].p = q;
      /* pstores are queued, increase the reference count */
      pstore_ref(q);
      kprintf(KR_OSTREAM,
	      "etharp_query:queued packet %p "
	      "on ARP entry %d.\n", (void *)q, i);
    }
  }
#endif
  /* ARP request? */
  if (perform_arp_request) {
    /* allocate a pstore for the outgoing ARP request packet */
    p = pstore_alloc(PSTORE_LINK, sizeof(struct etharp_hdr),
		     XMIT_STACK_SPACE,/*ssid = 0*/0);
    /* could allocate pstore? */
    if (p != NULL) {
      uint8_t j;
      kprintf(KR_OSTREAM,"etharp_query: sending ARP request");
      hdr = PSTORE_PAYLOAD(p,/*ssid = 0*/0);
      hdr->opcode = htons(ARP_REQUEST);
      for(j = 0; j < netif->hwaddr_len; ++j) {
	hdr->dhwaddr.addr[j] = 0x00;
	hdr->shwaddr.addr[j] = srcaddr->addr[j];
      }
      ip_addr_set(&(hdr->dipaddr), ipaddr);
      ip_addr_set(&(hdr->sipaddr), &(netif->ip_addr));
      
      hdr->hwtype = htons(HWTYPE_ETHERNET);
      ARPH_HWLEN_SET(hdr, netif->hwaddr_len);
      
      hdr->proto = htons(ETHTYPE_IP);
      ARPH_PROTOLEN_SET(hdr, sizeof(struct ip_addr));
      for(j = 0; j < netif->hwaddr_len; ++j) {
	hdr->ethhdr.dest.addr[j] = 0xff;
	hdr->ethhdr.src.addr[j] = srcaddr->addr[j];
      }
      hdr->ethhdr.type = htons(ETHTYPE_ARP);      
      /* send ARP query */
      result = netif_start_xmit(p,-1,(struct ip_addr*)IP_ADDR_BROADCAST,ssid);
      /* free ARP query packet */
      pstore_free(p,/*ssid = 0*/0);
      p = NULL;
    } else {
      result = RC_ERR_MEM;
      kprintf(KR_OSTREAM,"etharp_query:couldn't alloc pstore for ARP req");
    }
  }
  return result;
}

struct eth_addr *
etharp_lookup(struct netif *netif,struct ip_addr *ipaddr,
	      struct pstore *p,int ssid)
{
  int i;
  struct eth_addr *dest = NULL;

  /* Ethernet address for IP destination address is in ARP cache? */
  for(i = 0; i < ARP_TABLE_SIZE; ++i) {
    /* match found? */    
    if (arp_table[i].state == ETHARP_STATE_STABLE &&
	ip_addr_cmp(ipaddr, &arp_table[i].ipaddr)) {
      dest = &arp_table[i].ethaddr;
      break;
    }
  }
  /* could not find the destination Ethernet address in ARP cache? */
  if (dest == NULL) {
    /* ARP query for the IP address, submit this IP packet for queueing */
    etharp_query(netif, ipaddr,p,ssid);
    /* return nothing */
    return NULL;
  }
  /* destination Ethernet address resolved from ARP cache */
  else {
    /* return destination ethernet address */
    return dest;
  }
}

