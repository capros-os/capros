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
/* This is a DHCP client for the lwIP TCP/IP stack. It aims to conform
 * with RFC 2131 and RFC 2132.
 *
 * TODO:
 * - Proper parsing of DHCP messages exploiting file/sname field overloading.
 * - Add JavaDoc style do_cumentation (API, internals).
 * - Support for interfaces other than Ethernet (SLIP, PPP, ...)
 *
 * Please coordinate changes and requests with Leon Woestenberg
 * <leon.woestenberg@gmx.net>
 *
 * Integration with your code:
 *
 * In lwip/dhcp.h
 * #define DHCP_COARSE_TIMER_SECS (recommended 60 which is a minute)
 * #define DHCP_FINE_TIMER_MSECS (recommended 500 which equals 
 *                                                 TCP coarse timer)
 *
 * Then have your application call dhcp_coarse_tmr() and
 * dhcp_fine_tmr() on the defined intervals.
 *
 * dhcp_start(struct netif *netif);
 * starts a DHCP client instance which configures the interface by
 * obtaining an IP address lease and maintaining it.
 *
 * Use dhcp_release(netif) to end the lease and use dhcp_stop(netif)
 * to remove the DHCP client.
 */

#include <stddef.h>
#include <eros/target.h>
#include <eros/endian.h>
#include <eros/Invoke.h>

#include <domain/domdbg.h>
#include <domain/NetSysKey.h>

#include <string.h>

#include "include/mem.h"
#include "include/udp.h"
#include "include/inet.h"
#include "include/ip_addr.h"
#include "include/dhcp.h"
#include "include/etharp.h"
#include "include/ip.h"
#include "include/opt.h"

#include "netif/netif.h"
#include "Session.h"

#define DEBUGDHCP if(0)

#include "netsyskeys.h"

extern struct session ActiveSessions[MAX_SESSIONS];
static uint32_t xid = 0xABCD0000;

/* global transaction identifier, must be
 * unique for each DHCP request. */

/* DHCP client state machine functions */
void dhcp_free_reply(struct dhcp *dhcp);
void dhcp_handle_ack(struct netif *netif);
void dhcp_handle_nak(struct netif *netif);
void dhcp_handle_offer(struct netif *netif);
uint32_t dhcp_discover(struct netif *netif);
uint32_t dhcp_select(struct netif *netif);
void dhcp_check(struct netif *netif);
void dhcp_bind(struct netif *netif);
uint32_t dhcp_decline(struct netif *netif);
uint32_t dhcp_rebind(struct netif *netif);
uint32_t dhcp_release(struct netif *netif);
void dhcp_set_state(struct dhcp *dhcp, unsigned char new_state);

/* receive, unfold, parse and free incoming messages */
void dhcp_recv(void *arg, struct udp_pcb *pcb, 
		      struct pstore *p, struct ip_addr *addr, uint16_t port);
uint32_t dhcp_unfold_reply(struct dhcp *dhcp);
uint8_t *dhcp_get_option_ptr(struct dhcp *dhcp, uint8_t option_type);
uint8_t dhcp_get_option_byte(uint8_t *ptr);
uint16_t dhcp_get_option_short(uint8_t *ptr);
uint32_t dhcp_get_option_long(uint8_t *ptr);

/* set the DHCP timers */
void dhcp_timeout(struct netif *netif);
void dhcp_t1_timeout(struct netif *netif);
void dhcp_t2_timeout(struct netif *netif);

/* build outgoing messages */
/* create a DHCP request, fill in common headers */
uint32_t dhcp_create_request(struct netif *netif);
/* free a DHCP request */
void dhcp_delete_request(struct netif *netif);
/* add a DHCP option (type, then length in bytes) */
void dhcp_option(struct dhcp *dhcp, uint8_t option_type, 
			uint8_t option_len);
/* add option values */
void dhcp_option_byte(struct dhcp *dhcp, uint8_t value);
void dhcp_option_short(struct dhcp *dhcp, uint16_t value);
void dhcp_option_long(struct dhcp *dhcp, uint32_t value);
/* always add the DHCP options trailer to end and pad */
void dhcp_option_trailer(struct dhcp *dhcp);

/*
 * Back-off the DHCP client (because of a received NAK response).
 *
 * Back-off the DHCP client because of a received NAK. Receiving a
 * NAK means the client asked for something non-sensible, for
 * example when it tries to renew a lease obtained on another network.
 *
 * We back-off and will end up restarting a fresh DHCP negotiation later.
 *
 * @param state pointer to DHCP state structure
 */ 
#if 0
void dhcp_handle_nak(struct netif *netif) {
  struct dhcp *dhcp = netif->dhcp;
  uint16_t msecs = 10 * 1000;
  kprintf(KR_OSTREAM,"dhcp_handle_nak(netif=%p) %c%c\n",
	  netif, netif->name[0], netif->name[1]);
  dhcp->request_timeout = (msecs + DHCP_FINE_TIMER_MSECS - 1) / DHCP_FINE_TIMER_MSECS;
  DEBUGDHCP
    kprintf(KR_OSTREAM,"dhcp_handle_nak():set request timeout %d msecs",msecs);
  dhcp_set_state(dhcp, DHCP_BACKING_OFF);
}
#endif

#if 0
/**
 * Checks if the offered IP address is already in use.
 * 
 * It does so by sending an ARP request for the offered address and
 * entering CHECKING state. If no ARP reply is received within a small
 * interval, the address is assumed to be free for use by us.
 */
void dhcp_check(struct netif *netif)
{
  struct dhcp *dhcp = netif->dhcp;
  uint32_t result;
  uint16_t msecs;
  
  kprintf(KR_OSTREAM,"dhcp_check(netif=%p) %c%c", netif, netif->name[0],
	  netif->name[1]);
  /* create an ARP query for the offered IP address, expecting that no host
   * responds, as the IP address should not be in use. */
  result = etharp_query(netif, &dhcp->offered_ip_addr, NULL,/**/);
  if (result != RC_OK) {
    kprintf(KR_OSTREAM,"dhcp_check:could not perform ARP query");
  }
  dhcp->tries++;
  msecs = 500;
  dhcp->request_timeout = (msecs + DHCP_FINE_TIMER_MSECS - 1) / DHCP_FINE_TIMER_MSECS;
  DEBUG_DHCP
  kprintf(KR_OSTREAM,"dhcp_check(): set request timeout %d msecs", msecs);
  dhcp_set_state(dhcp, DHCP_CHECKING);
}
#endif

/**
 * Remember the configuration offered by a DHCP server.
 * 
 * @param state pointer to DHCP state structure
 */
void dhcp_handle_offer(struct netif *netif)
{
  struct dhcp *dhcp = netif->dhcp;
  
  /* obtain the server address */
  uint8_t *option_ptr = dhcp_get_option_ptr(dhcp, DHCP_OPTION_SERVER_ID);
  DEBUGDHCP
    kprintf(KR_OSTREAM,"dhcp_handle_offer() %c%c",
	    netif->name[0],netif->name[1]);
  if (option_ptr != NULL) {
    dhcp->server_ip_addr.addr = htonl(dhcp_get_option_long(&option_ptr[2]));
    DEBUGDHCP
      kprintf(KR_OSTREAM,"dhcp_handle_offer():server 0x%x",
	      dhcp->server_ip_addr.addr);
    /* remember offered address */
    ip_addr_set(&dhcp->offered_ip_addr,
		(struct ip_addr *)&dhcp->msg_in->yiaddr);
    kprintf(KR_OSTREAM,"dhcp_handle_offer(): offer for 0x%x", 
	    dhcp->offered_ip_addr.addr);
    dhcp_select(netif);
  }
}

/**
 * Select a DHCP server offer out of all offers.
 *
 * Simply select the first offer received.
 *
 * @param netif the netif under DHCP control
 * @return lwIP specific error (see error.h)
 */
uint32_t dhcp_select(struct netif *netif)
{
  struct dhcp *dhcp = netif->dhcp;
  uint32_t result;
  uint32_t msecs;
  
  /* create and initialize the DHCP message header */
  result = dhcp_create_request(netif);
  if (result == RC_OK) {
    dhcp_option(dhcp, DHCP_OPTION_MESSAGE_TYPE, DHCP_OPTION_MESSAGE_TYPE_LEN);
    dhcp_option_byte(dhcp, DHCP_REQUEST);
    
    dhcp_option(dhcp, DHCP_OPTION_MAX_MSG_SIZE, DHCP_OPTION_MAX_MSG_SIZE_LEN);
    dhcp_option_short(dhcp, 576);
    
    /* MUST request the offered IP address */
    dhcp_option(dhcp, DHCP_OPTION_REQUESTED_IP, 4);
    dhcp_option_long(dhcp, ntohl(dhcp->offered_ip_addr.addr));
    
    dhcp_option(dhcp, DHCP_OPTION_SERVER_ID, 4);
    dhcp_option_long(dhcp, ntohl(dhcp->server_ip_addr.addr));
    
    dhcp_option(dhcp, DHCP_OPTION_PARAMETER_REQUEST_LIST, 3);
    dhcp_option_byte(dhcp, DHCP_OPTION_SUBNET_MASK);
    dhcp_option_byte(dhcp, DHCP_OPTION_ROUTER);
    dhcp_option_byte(dhcp, DHCP_OPTION_BROADCAST);
    
    dhcp_option_trailer(dhcp);
    /* shrink the pstore to the actual content length */
    pstore_realloc(dhcp->p_out, sizeof(struct dhcp_msg) - DHCP_OPTIONS_LEN + 
		   dhcp->options_out_len,dhcp->pcb->ssid);
    
    /* TODO: we really should bind to a specific local interface here
     * but we cannot specify an unconfigured netif as it is addressless */
    udp_bind(dhcp->pcb,(struct ip_addr *)IP_ADDR_ANY, DHCP_CLIENT_PORT);
    /* send broadcast to any DHCP server */
    udp_connect(dhcp->pcb,(struct ip_addr*)IP_ADDR_BROADCAST,DHCP_SERVER_PORT);
    udp_send(dhcp->pcb, dhcp->p_out);
    /* reconnect to any (or to server here?!) */
    udp_connect(dhcp->pcb,(struct ip_addr*)IP_ADDR_ANY,DHCP_SERVER_PORT);
    //dhcp_delete_request(netif);
    dhcp_set_state(dhcp, DHCP_REQUESTING);
  } else {
    kprintf(KR_OSTREAM,"dhcp_select: could not allocate DHCP request");
  }
  dhcp->tries++;
  msecs = dhcp->tries < 4 ? dhcp->tries * 1000 : 4 * 1000;
  dhcp->request_timeout = (msecs + DHCP_FINE_TIMER_MSECS - \
			   1) / DHCP_FINE_TIMER_MSECS;
  return result;
}


/* The DHCP timer that checks for lease renewal/rebind timeouts */
#if 0
void dhcp_coarse_tmr()
{
  struct netif *netif = netif_list;
  kprintf(KR_OSTREAM ,"dhcp_coarse_tmr()");
  /* iterate through all network interfaces */
  while (netif != NULL) {
    /* only act on DHCP configured interfaces */
    if (netif->dhcp != NULL) {
      /* timer is active (non zero), and triggers (zeroes) now? */
      if (netif->dhcp->t2_timeout-- == 1) {
        kprintf(KR_OSTREAM,"dhcp_coarse_tmr(): t2 timeout");
        /* this clients' rebind timeout triggered */
        dhcp_t2_timeout(netif);
	/* timer is active (non zero), and triggers (zeroes) now */
      } else if (netif->dhcp->t1_timeout-- == 1) {
        kprintf(KR_OSTREAM,"dhcp_coarse_tmr(): t1 timeout");
        /* this clients' renewal timeout triggered */
        dhcp_t1_timeout(netif);
      }
    }
    /* proceed to next netif */
    netif = netif->next;
  }
}
#endif

/**
 * DHCP transaction timeout handling
 * A DHCP server is expected to respond within a
 * short period of time.
 */
#if 0
void dhcp_fine_tmr()
{
  struct netif *netif = netif_list;
  /* loop through clients */
  while (netif != NULL) {
    /* only act on DHCP configured interfaces */
    if (netif->dhcp != NULL) {
      /* timer is active (non zero), and triggers (zeroes) now */
      if (netif->dhcp->request_timeout-- == 1) {
        kprintf(KR_OSTREAM,"dhcp_fine_tmr(): request timeout");
        /* this clients' request timeout triggered */
        dhcp_timeout(netif);
      }
    }
    /* proceed to next network interface */
    netif = netif->next;
  }
}
#endif

/**
 * A DHCP negotiation transaction, or ARP request, has timed out.
 *
 * The timer that was started with the DHCP or ARP request has
 * timed out, indicating no response was received in time. 
 *
 * @param netif the netif under DHCP control
 * 
 */
#if 0
static void dhcp_timeout(struct netif *netif)
{
  struct dhcp *dhcp = netif->dhcp;
  
  kprintf(KR_OSTREAM, "dhcp_timeout()");
  /* back-off period has passed, or server selection timed out */
  if ((dhcp->state == DHCP_BACKING_OFF) || (dhcp->state == DHCP_SELECTING)) {
    kprintf(KR_OSTREAM ,"dhcp_timeout(): restarting discovery");
    dhcp_discover(netif);
    /* receiving the requested lease timed out */
  } else if (dhcp->state == DHCP_REQUESTING) {
    kprintf(KR_OSTREAM ,"dhcp_timeout():REQUESTING,DHCP request timed out");
    if (dhcp->tries <= 5) {
      dhcp_select(netif);
    } else {
      kprintf(KR_OSTREAM,"dhcp_timeout():REQUESTING,releasing,restarting");
      dhcp_release(netif);
      dhcp_discover(netif);
    }
    /* received no ARP reply for the offered address (which is good) */
  } else if (dhcp->state == DHCP_CHECKING) {
    kprintf(KR_OSTREAM,"dhcp_timeout():CHECKING, ARP request timed out");
    if (dhcp->tries <= 1) {
      dhcp_check(netif);
      /* no ARP replies on the offered address, 
       * looks like the IP address is indeed free */
    } else {
      /* bind the interface to the offered address */
      dhcp_bind(netif);
    }
  }
  /* did not get response to renew request? */
  else if (dhcp->state == DHCP_RENEWING) {
    kprintf(KR_OSTREAM,"dhcp_timeout():RENEWING, DHCP request timed out");
    /* just retry renewal */ 
    /* note that the rebind timer will eventually time-out if renew 
     * does not work */
    dhcp_renew(netif);
    /* did not get response to rebind request? */
  } else if (dhcp->state == DHCP_REBINDING) {
    kprintf(KR_OSTREAM,"dhcp_timeout():REBINDING, DHCP request timed out");
    if (dhcp->tries <= 8) {
      dhcp_rebind(netif);
    } else {
      kprintf(KR_OSTREAM,"dhcp_timeout(): RELEASING, DISCOVERING");
      dhcp_release(netif);
      dhcp_discover(netif);
    }
  }
}
#endif

/**
 * The renewal period has timed out.
 *
 * @param netif the netif under DHCP control
 */
#if 0
static void dhcp_t1_timeout(struct netif *netif)
{
  struct dhcp *dhcp = netif->dhcp;
 
  kprintf(KR_OSTREAM, "dhcp_t1_timeout()");
  if ((dhcp->state == DHCP_REQUESTING) || (dhcp->state == DHCP_BOUND) 
      || (dhcp->state == DHCP_RENEWING)) {
    /* just retry to renew */
    /* note that the rebind timer will eventually time-out
     * if renew does not work */
    kprintf(KR_OSTREAM,"dhcp_t1_timeout(): must renew");
    dhcp_renew(netif);
  }
}

/* The rebind period has timed out */
static void dhcp_t2_timeout(struct netif *netif)
{
  struct dhcp *dhcp = netif->dhcp;

  kprintf(KR_OSTREAM,"dhcp_t2_timeout()");
  if ((dhcp->state == DHCP_REQUESTING) || (dhcp->state == DHCP_BOUND) || 
      (dhcp->state == DHCP_RENEWING)) {
    /* just retry to rebind */
    kprintf(KR_OSTREAM,"dhcp_t2_timeout(): must rebind");
    dhcp_rebind(netif);
  }
}
#endif

/**
 * Extract options from the server ACK message.
 * @param netif the netif under DHCP control
 */
void dhcp_handle_ack(struct netif *netif)
{
  struct dhcp *dhcp = netif->dhcp;
  uint8_t *option_ptr;
  
  /* clear options we might not get from the ACK */
  dhcp->offered_sn_mask.addr = 0;
  dhcp->offered_gw_addr.addr = 0;
  dhcp->offered_bc_addr.addr = 0;
  
  /* lease time given? */
  option_ptr = dhcp_get_option_ptr(dhcp, DHCP_OPTION_LEASE_TIME);
  if (option_ptr != NULL) {
    /* remember offered lease time */
    dhcp->offered_t0_lease = dhcp_get_option_long(option_ptr + 2);
  }
  /* renewal period given? */
  option_ptr = dhcp_get_option_ptr(dhcp, DHCP_OPTION_T1);
  if (option_ptr != NULL) {
    /* remember given renewal period */
    dhcp->offered_t1_renew = dhcp_get_option_long(option_ptr + 2);
  } else {
    /* calculate safe periods for renewal */
    dhcp->offered_t1_renew = dhcp->offered_t0_lease / 2;
  }
  
  /* renewal period given? */
  option_ptr = dhcp_get_option_ptr(dhcp, DHCP_OPTION_T2);
  if (option_ptr != NULL) {
    /* remember given rebind period */
    dhcp->offered_t2_rebind = dhcp_get_option_long(option_ptr + 2);
  } else {
    /* calculate safe periods for rebinding */
    dhcp->offered_t2_rebind = dhcp->offered_t0_lease;
  }
  
  /* (y)our internet address */
  ip_addr_set(&dhcp->offered_ip_addr, &dhcp->msg_in->yiaddr);
  
  /**
   * Patch #1308
   * TODO: we must check if the file field is not overloaded by DHCP options!
   */
#if 0
  /* boot server address */
  ip_addr_set(&dhcp->offered_si_addr, &dhcp->msg_in->siaddr);
  /* boot file name */
  if (dhcp->msg_in->file[0]) {
    dhcp->boot_file_name = mem_malloc(strlen(dhcp->msg_in->file) + 1);
    strcpy(dhcp->boot_file_name, dhcp->msg_in->file);
  }
#endif
  
  /* subnet mask */
  option_ptr = dhcp_get_option_ptr(dhcp, DHCP_OPTION_SUBNET_MASK);
  /* subnet mask given? */
  if (option_ptr != NULL) {
    dhcp->offered_sn_mask.addr = htonl(dhcp_get_option_long(&option_ptr[2]));
  }
  
  /* gateway router */
  option_ptr = dhcp_get_option_ptr(dhcp, DHCP_OPTION_ROUTER);
  if (option_ptr != NULL) {
    dhcp->offered_gw_addr.addr = htonl(dhcp_get_option_long(&option_ptr[2]));
  }
  
  /* broadcast address */
  option_ptr = dhcp_get_option_ptr(dhcp, DHCP_OPTION_BROADCAST);
  if (option_ptr != NULL) {
    dhcp->offered_bc_addr.addr = htonl(dhcp_get_option_long(&option_ptr[2]));
  }
}

/**
 * Start DHCP negotiation for a network interface.
 *
 * If no DHCP client instance was attached to this interface,
 * a new client is created first. If a DHCP client instance
 * was already present, it restarts negotiation.
 *
 * @param netif The lwIP network interface 
 * @return lwIP error code
 * - RC_OK - No error
 * - RC_NetSys_PbufsExhausted - Out of Memory
 *
 */
uint32_t dhcp_start(struct netif *netif)
{
  struct dhcp *dhcp = netif->dhcp;
  uint32_t result = RC_OK;
  
  if (dhcp == NULL) {
    dhcp = mem_malloc(sizeof(struct dhcp));
    if (dhcp == NULL) {
      kprintf(KR_OSTREAM,"dhcp_start(): could not allocate dhcp");
      netif->flags &= ~NETIF_FLAG_DHCP;
      return RC_NetSys_PbufsExhausted;
    }
    
    /* clear data structure */
    bzero(dhcp,sizeof(struct dhcp));
    dhcp->pcb = udp_new();
    if (dhcp->pcb == NULL) {
      kprintf(KR_OSTREAM,"dhcp_start(): could not obtain pcb");
      mem_free((void *)dhcp);
      dhcp = NULL;
      netif->flags &= ~NETIF_FLAG_DHCP;
      return RC_NetSys_PbufsExhausted;
    }
    /* stamp the ssid of this is 0 */
    (dhcp->pcb)->ssid = 0;
    /* store this dhcp client in the netif */
    netif->dhcp = dhcp;
  } else {
    kprintf(KR_OSTREAM,"dhcp_start(): restarting DHCP configuration");
  }
  /* (re)start the DHCP negotiation */
  result = dhcp_discover(netif);
  if (result != RC_OK) {
    /* free resources allocated above */
    dhcp_stop(netif);
  }
  return result;
}

#if 0
/**
 * Inform a DHCP server of our manual configuration.
 * 
 * This informs DHCP servers of our fixed IP address configuration
 * by sending an INFORM message. It does not involve DHCP address
 * configuration, it is just here to be nice to the network.
 *
 * @param netif The lwIP network interface 
 *
 */ 
void dhcp_inform(struct netif *netif)
{
  struct dhcp *dhcp;
  uint32_t result = RC_OK;
  
  dhcp = mem_malloc(sizeof(struct dhcp));
  if (dhcp == NULL) {
    kprintf(KR_OSTREAM,"dhcp_inform():could not allocate dhcp");
    return;
  }  
  netif->dhcp = dhcp;
  bzero(dhcp,sizeof(struct dhcp));

  dhcp->pcb = udp_new();
  if (dhcp->pcb == NULL) {
    kprintf(KR_OSTREAM,"dhcp_inform(): could not obtain pcb");
    mem_free((void *)dhcp);
    return;
  }
  /* create and initialize the DHCP message header */
  result = dhcp_create_request(netif);
  if (result == RC_OK) {
    dhcp_option(dhcp, DHCP_OPTION_MESSAGE_TYPE, DHCP_OPTION_MESSAGE_TYPE_LEN);
    dhcp_option_byte(dhcp, DHCP_INFORM);
    
    dhcp_option(dhcp, DHCP_OPTION_MAX_MSG_SIZE, DHCP_OPTION_MAX_MSG_SIZE_LEN);
    /* TODO: use netif->mtu ?! */
    dhcp_option_short(dhcp, 576);
    
    dhcp_option_trailer(dhcp);
    
    pstore_realloc(dhcp->p_out, sizeof(struct dhcp_msg) - DHCP_OPTIONS_LEN
		   + dhcp->options_out_len,dhcp->pcb->ssid);
    
    udp_bind(dhcp->pcb,(struct ip_addr *)IP_ADDR_ANY, DHCP_CLIENT_PORT);
    udp_connect(dhcp->pcb,(struct ip_addr*)IP_ADDR_BROADCAST,DHCP_SERVER_PORT);
    udp_send(dhcp->pcb, dhcp->p_out);
    udp_connect(dhcp->pcb,(struct ip_addr*)IP_ADDR_ANY, DHCP_SERVER_PORT);
    dhcp_delete_request(netif);
  } else {
    kprintf(KR_OSTREAM,"dhcp_inform: could not allocate DHCP request");
  }
  
  if (dhcp != NULL) {
    if (dhcp->pcb != NULL) udp_remove(dhcp->pcb);
    dhcp->pcb = NULL;
    mem_free((void *)dhcp);
    netif->dhcp = NULL;
  }
}

#if DHCP_DOES_ARP_CHECK
/**
 * Match an ARP reply with the offered IP address.
 *
 * @param addr The IP address we received a reply from
 */
void dhcp_arp_reply(struct netif *netif, struct ip_addr *addr)
{
  kprintf(KR_OSTREAM,"dhcp_arp_reply()");
  /* is this DHCP client doing an ARP check? */
  if ((netif->dhcp != NULL) && (netif->dhcp->state == DHCP_CHECKING)) {
    kprintf(KR_OSTREAM,"dhcp_arp_reply():CHECKING, arp reply for 0x%08lx",
	    addr->addr);
    /* did a host respond with the address we
     * were offered by the DHCP server? */
    if (ip_addr_cmp(addr, &netif->dhcp->offered_ip_addr)) {
      /* we will not accept the offered address */
      kprintf(KR_OSTREAM,"dhcp_arp_reply():arp reply matched with offered \
                        address, declining\n");
      dhcp_decline(netif);
    }
  }
}

/** 
 * Decline an offered lease.
 *
 * Tell the DHCP server we do not accept the offered address.
 * One reason to decline the lease is when we find out the address
 * is already in use by another host (through ARP).
 */
static uint32_t dhcp_decline(struct netif *netif)
{
  struct dhcp *dhcp = netif->dhcp;
  uint32_t result = RC_OK;
  uint16_t msecs;

  kprintf(KR_OSTREAM,"dhcp_decline()");
  dhcp_set_state(dhcp, DHCP_BACKING_OFF);
  /* create and initialize the DHCP message header */
  result = dhcp_create_request(netif);
  if (result == RC_OK) {
    dhcp_option(dhcp, DHCP_OPTION_MESSAGE_TYPE, DHCP_OPTION_MESSAGE_TYPE_LEN);
    dhcp_option_byte(dhcp, DHCP_DECLINE);

    dhcp_option(dhcp, DHCP_OPTION_MAX_MSG_SIZE, DHCP_OPTION_MAX_MSG_SIZE_LEN);
    dhcp_option_short(dhcp, 576);

    dhcp_option_trailer(dhcp);
    /* resize pstore to reflect true size of options */
    pstore_realloc(dhcp->p_out,sizeof(struct dhcp_msg) - DHCP_OPTIONS_LEN 
		 + dhcp->options_out_len);

    udp_bind(dhcp->pcb,(struct ip_addr *)IP_ADDR_ANY, DHCP_CLIENT_PORT);
    udp_connect(dhcp->pcb,&dhcp->server_ip_addr, DHCP_SERVER_PORT);
    udp_send(dhcp->pcb, dhcp->p_out);
    dhcp_delete_request(netif);
    kprintf(KR_OSTREAM,"dhcp_decline: BACKING OFF");
  } else {
    kprintf(KR_OSTREAM,"dhcp_decline: could not allocate DHCP request");
  }
  dhcp->tries++;
  msecs = 10*1000;
  dhcp->request_timeout = (msecs + DHCP_FINE_TIMER_MSECS - 1) / DHCP_FINE_TIMER_MSECS;
  DEBUG_DHCP
  kprintf(KR_OSTREAM,"dhcp_decline():set request timeout %d msecs", msecs);
  return result;
}
#endif

#endif

/* Start the DHCP process, discover a DHCP server */
uint32_t dhcp_discover(struct netif *netif)
{
  struct dhcp *dhcp = netif->dhcp;
  uint32_t result = RC_OK;
  uint16_t msecs;
    
  ip_addr_set(&dhcp->offered_ip_addr, IP_ADDR_ANY);
  /* create and initialize the DHCP message header */
  result = dhcp_create_request(netif);
  if (result == RC_OK) {
    dhcp_option(dhcp, DHCP_OPTION_MESSAGE_TYPE, DHCP_OPTION_MESSAGE_TYPE_LEN);
    dhcp_option_byte(dhcp, DHCP_DISCOVER);
    
    dhcp_option(dhcp, DHCP_OPTION_MAX_MSG_SIZE, DHCP_OPTION_MAX_MSG_SIZE_LEN);
    dhcp_option_short(dhcp, 576);
    
    dhcp_option(dhcp, DHCP_OPTION_PARAMETER_REQUEST_LIST, 3);
    dhcp_option_byte(dhcp, DHCP_OPTION_SUBNET_MASK);
    dhcp_option_byte(dhcp, DHCP_OPTION_ROUTER);
    dhcp_option_byte(dhcp, DHCP_OPTION_BROADCAST);
    
    dhcp_option_trailer(dhcp);
    
    pstore_realloc(dhcp->p_out,sizeof(struct dhcp_msg) - DHCP_OPTIONS_LEN 
		   + dhcp->options_out_len,dhcp->pcb->ssid);

    /* set receive callback function with netif as user data */
    udp_recv(dhcp->pcb,(void *)dhcp_recv, netif);
    udp_bind(dhcp->pcb,(struct ip_addr *)IP_ADDR_ANY, DHCP_CLIENT_PORT);
    udp_connect(dhcp->pcb,(struct ip_addr*)IP_ADDR_BROADCAST,DHCP_SERVER_PORT);
    
    udp_send(dhcp->pcb, dhcp->p_out);
    udp_bind(dhcp->pcb,(struct ip_addr *)IP_ADDR_ANY, DHCP_CLIENT_PORT);
    udp_connect(dhcp->pcb,(struct ip_addr*)IP_ADDR_ANY, DHCP_SERVER_PORT);
    //dhcp_delete_request(netif);
    dhcp_set_state(dhcp, DHCP_SELECTING);
  } else {
    kprintf(KR_OSTREAM,"dhcp_discover: could not allocate DHCP request");
  }
  dhcp->tries++;
  msecs = dhcp->tries < 4 ? (dhcp->tries + 1) * 1000 : 10 * 1000;
  dhcp->request_timeout = (msecs + DHCP_FINE_TIMER_MSECS - 1) / DHCP_FINE_TIMER_MSECS;
  kprintf(KR_OSTREAM,"dhcp_discover():set request timeout %d msecs",msecs);
  return result;
}


/**
 * Bind the interface to the offered IP address.
 * @param netif network interface to bind to the offered address
 */
void dhcp_bind(struct netif *netif)
{
  struct dhcp *dhcp = netif->dhcp;
  struct ip_addr sn_mask, gw_addr;

  /* temporary DHCP lease? */
  if (dhcp->offered_t1_renew != 0xffffffffu) {
    /* set renewal period timer */
    DEBUGDHCP
      kprintf(KR_OSTREAM,"dhcp_bind():t1 renewal timer %d secs\n",
	      dhcp->offered_t1_renew);
    dhcp->t1_timeout = (dhcp->offered_t1_renew + 
			DHCP_COARSE_TIMER_SECS / 2) / DHCP_COARSE_TIMER_SECS;
    if (dhcp->t1_timeout == 0) dhcp->t1_timeout = 1;
    DEBUGDHCP
      kprintf(KR_OSTREAM,"dhcp_bind(): set request timeout %d msecs",
	      dhcp->offered_t1_renew*1000);
  }
  /* set renewal period timer */
  if (dhcp->offered_t2_rebind != 0xffffffffUL) {
    kprintf(KR_OSTREAM,"dhcp_bind():t2 rebind timer %d secs", 
	    dhcp->offered_t2_rebind);
    dhcp->t2_timeout = (dhcp->offered_t2_rebind + \
			DHCP_COARSE_TIMER_SECS / 2) / DHCP_COARSE_TIMER_SECS;
    if (dhcp->t2_timeout == 0) dhcp->t2_timeout = 1;
    kprintf(KR_OSTREAM,"dhcp_bind(): set request timeout %d msecs", 
	    dhcp->offered_t2_rebind*1000);
  }
  /* copy offered network mask */
  ip_addr_set(&sn_mask, &dhcp->offered_sn_mask);
  
  /* subnet mask not given? */
  /* TODO: this is not a valid check. what if the network mask is 0? */
  if (sn_mask.addr == 0) {
    /* choose a safe subnet mask given the network class */
    uint8_t first_octet = ip4_addr1(&sn_mask);
    if (first_octet <= 127) sn_mask.addr = htonl(0xff000000);
    else if (first_octet >= 192) sn_mask.addr = htonl(0xffffff00);
    else sn_mask.addr = htonl(0xffff0000);
  }
  
  ip_addr_set(&gw_addr, &dhcp->offered_gw_addr);
  /* gateway address not given? */
  if (gw_addr.addr == 0) {
    /* copy network address */
    gw_addr.addr = (dhcp->offered_ip_addr.addr & sn_mask.addr);
    /* use first host address on network as gateway */
    gw_addr.addr |= htonl(0x00000001);
  }
  
  kprintf(KR_OSTREAM,"dhcp_bind(): IP: 0x%x", dhcp->offered_ip_addr.addr);
  netif_set_ipaddr(netif, &dhcp->offered_ip_addr);
  kprintf(KR_OSTREAM ,"dhcp_bind(): SN: 0x%x", sn_mask.addr);
  netif_set_netmask(netif, &sn_mask);
  kprintf(KR_OSTREAM ,"dhcp_bind(): GW: 0x%x", gw_addr.addr);
  netif_set_gw(netif, &gw_addr);
  /* netif is now bound to DHCP leased address */
  dhcp_set_state(dhcp, DHCP_BOUND);
}

#if 0
/**
 * Renew an existing DHCP lease at the involved DHCP server.
 * 
 * @param netif network interface which must renew its lease
 */
uint32_t dhcp_renew(struct netif *netif)
{
  struct dhcp *dhcp = netif->dhcp;
  uint32_t result;
  uint16_t msecs;
 
  kprintf(KR_OSTREAM,"dhcp_renew()");
  dhcp_set_state(dhcp, DHCP_RENEWING);

  /* create and initialize the DHCP message header */
  result = dhcp_create_request(netif);
  if (result == RC_OK) {
    dhcp_option(dhcp, DHCP_OPTION_MESSAGE_TYPE, DHCP_OPTION_MESSAGE_TYPE_LEN);
    dhcp_option_byte(dhcp, DHCP_REQUEST);
    
    dhcp_option(dhcp, DHCP_OPTION_MAX_MSG_SIZE, DHCP_OPTION_MAX_MSG_SIZE_LEN);
    /* TODO: use netif->mtu in some way */
    dhcp_option_short(dhcp, 576);
    
#if 0
    dhcp_option(dhcp, DHCP_OPTION_REQUESTED_IP, 4);
    dhcp_option_long(dhcp, ntohl(dhcp->offered_ip_addr.addr));
#endif

#if 0
    dhcp_option(dhcp, DHCP_OPTION_SERVER_ID, 4);
    dhcp_option_long(dhcp, ntohl(dhcp->server_ip_addr.addr));
#endif
    /* append DHCP message trailer */
    dhcp_option_trailer(dhcp);

    pstore_realloc(dhcp->p_out, sizeof(struct dhcp_msg) - DHCP_OPTIONS_LEN 
		 + dhcp->options_out_len);

    udp_bind(dhcp->pcb,(struct ip_addr*)IP_ADDR_ANY, DHCP_CLIENT_PORT);
    udp_connect(dhcp->pcb, &dhcp->server_ip_addr, DHCP_SERVER_PORT);
    udp_send(dhcp->pcb, dhcp->p_out);
    dhcp_delete_request(netif);
    
    kprintf(KR_OSTREAM,"dhcp_renew: RENEWING");
  } else {
    kprintf(KR_OSTREAM ,"dhcp_renew: could not allocate DHCP request");
  }
  dhcp->tries++;
  /* back-off on retries, but to a maximum of 20 seconds */
  msecs = dhcp->tries < 10 ? dhcp->tries * 2000 : 20 * 1000;
  dhcp->request_timeout = (msecs + DHCP_FINE_TIMER_MSECS - 1) / DHCP_FINE_TIMER_MSECS;
  kprintf(KR_OSTREAM ,"dhcp_renew(): set request timeout %u msecs", msecs);
  return result;
}

/**
 * Rebind with a DHCP server for an existing DHCP lease.
 * 
 * @param netif network interface which must rebind with a DHCP server
 */
static uint32_t dhcp_rebind(struct netif *netif)
{
  struct dhcp *dhcp = netif->dhcp;
  uint32_t result;
  uint16_t msecs;
  
  kprintf(KR_OSTREAM,"dhcp_rebind()");
  dhcp_set_state(dhcp, DHCP_REBINDING);

  /* create and initialize the DHCP message header */
  result = dhcp_create_request(netif);
  if (result == RC_OK) {
    
    dhcp_option(dhcp, DHCP_OPTION_MESSAGE_TYPE, DHCP_OPTION_MESSAGE_TYPE_LEN);
    dhcp_option_byte(dhcp, DHCP_REQUEST);
    
    dhcp_option(dhcp, DHCP_OPTION_MAX_MSG_SIZE, DHCP_OPTION_MAX_MSG_SIZE_LEN);
    dhcp_option_short(dhcp, 576);
    
#if 0
    dhcp_option(dhcp, DHCP_OPTION_REQUESTED_IP, 4);
    dhcp_option_long(dhcp, ntohl(dhcp->offered_ip_addr.addr));
    
    dhcp_option(dhcp, DHCP_OPTION_SERVER_ID, 4);
    dhcp_option_long(dhcp, ntohl(dhcp->server_ip_addr.addr));
#endif
    
    dhcp_option_trailer(dhcp);
    
    pstore_realloc(dhcp->p_out, sizeof(struct dhcp_msg) - DHCP_OPTIONS_LEN + 
		 dhcp->options_out_len);
    
    udp_bind(dhcp->pcb,(struct ip_addr *)IP_ADDR_ANY, DHCP_CLIENT_PORT);
    udp_connect(dhcp->pcb,(struct ip_addr*)IP_ADDR_BROADCAST,DHCP_SERVER_PORT);
    udp_send(dhcp->pcb, dhcp->p_out);
    udp_connect(dhcp->pcb,(struct ip_addr*)IP_ADDR_ANY,DHCP_SERVER_PORT);
    dhcp_delete_request(netif);
    kprintf(KR_OSTREAM,"dhcp_rebind: REBINDING");
  } else {
    kprintf(KR_OSTREAM,"dhcp_rebind: could not allocate DHCP request");
  }
  dhcp->tries++;
  msecs = dhcp->tries < 10 ? dhcp->tries * 1000 : 10 * 1000;
  dhcp->request_timeout = (msecs + DHCP_FINE_TIMER_MSECS - 1) / DHCP_FINE_TIMER_MSECS;
  kprintf(KR_OSTREAM,"dhcp_rebind(): set request timeout %u msecs", msecs);
  return result;
}

/**
 * Release a DHCP lease.
 * 
 * @param netif network interface which must release its lease
 */
static uint32_t dhcp_release(struct netif *netif)
{
  struct dhcp *dhcp = netif->dhcp;
  uint32_t result;
  uint16_t msecs;
  
  kprintf(KR_OSTREAM,"dhcp_release()");
  
  /* idle DHCP client */
  dhcp_set_state(dhcp, DHCP_OFF);
  
  /* create and initialize the DHCP message header */
  result = dhcp_create_request(netif);
  if (result == RC_OK) {
    dhcp_option(dhcp, DHCP_OPTION_MESSAGE_TYPE, DHCP_OPTION_MESSAGE_TYPE_LEN);
    dhcp_option_byte(dhcp, DHCP_RELEASE);
    
    dhcp_option_trailer(dhcp);
    
    pstore_realloc(dhcp->p_out, sizeof(struct dhcp_msg) - DHCP_OPTIONS_LEN 
		 + dhcp->options_out_len);
    
    udp_bind(dhcp->pcb,(struct ip_addr *)IP_ADDR_ANY, DHCP_CLIENT_PORT);
    udp_connect(dhcp->pcb, &dhcp->server_ip_addr, DHCP_SERVER_PORT);
    udp_send(dhcp->pcb, dhcp->p_out);
    dhcp_delete_request(netif);
    kprintf(KR_OSTREAM,"dhcp_release: RELEASED, DHCP_OFF");
  } else {
    kprintf(KR_OSTREAM,"dhcp_release: could not allocate DHCP request");
  }
  dhcp->tries++;
  msecs = dhcp->tries < 10 ? dhcp->tries * 1000 : 10 * 1000;
  dhcp->request_timeout = (msecs + DHCP_FINE_TIMER_MSECS - 1) / DHCP_FINE_TIMER_MSECS;
  kprintf(KR_OSTREAM,"dhcp_release(): set request timeout %u msecs", msecs);
  /* remove IP address from interface */
  netif_set_ipaddr(netif, IP_ADDR_ANY);
  netif_set_gw(netif, IP_ADDR_ANY);
  netif_set_netmask(netif, IP_ADDR_ANY);
  /* TODO: netif_down(netif); */
  return result;
}
#endif 

/**
 * Remove the DHCP client from the interface.
 *
 * @param netif The network interface to stop DHCP on
 */
void dhcp_stop(struct netif *netif)
{
  struct dhcp *dhcp = netif->dhcp;
 
  kprintf(KR_OSTREAM,"dhcp_stop()");
  /* netif is DHCP configured? */
  if (dhcp != NULL) {
    if (dhcp->pcb != NULL) {
      udp_remove(dhcp->pcb);
      dhcp->pcb = NULL;
    }
    if (dhcp->p != NULL) {
      pstore_free(dhcp->p,dhcp->pcb->ssid);
      dhcp->p = NULL;
    }
    /* free unfolded reply */
    dhcp_free_reply(dhcp);
    mem_free((void *)dhcp);
    netif->dhcp = NULL;
  }
}

/*
 * Set the DHCP state of a DHCP client.
 * 
 * If the state changed, reset the number of tries.
 *
 * TODO: we might also want to reset the timeout here?
 */
void dhcp_set_state(struct dhcp *dhcp, unsigned char new_state)
{
  if (new_state != dhcp->state) {
    dhcp->state = new_state;
    dhcp->tries = 0;
  }
}

/*
 * Concatenate an option type and length field to the outgoing
 * DHCP message.
 *
 */
void 
dhcp_option(struct dhcp *dhcp, uint8_t option_type, uint8_t option_len)
{
  dhcp->msg_out->options[dhcp->options_out_len++] = option_type;
  dhcp->msg_out->options[dhcp->options_out_len++] = option_len;
}

/* Concatenate a single byte to the outgoing DHCP message. */
void dhcp_option_byte(struct dhcp *dhcp, uint8_t value)
{
  dhcp->msg_out->options[dhcp->options_out_len++] = value;
}                             

void dhcp_option_short(struct dhcp *dhcp, uint16_t value)
{
  dhcp->msg_out->options[dhcp->options_out_len++] = (value & 0xff00U) >> 8;
  dhcp->msg_out->options[dhcp->options_out_len++] =  value & 0x00ffU;
}

void dhcp_option_long(struct dhcp *dhcp, uint32_t value)
{
  dhcp->msg_out->options[dhcp->options_out_len++] = (value & 0xff000000u)>>24;
  dhcp->msg_out->options[dhcp->options_out_len++] = (value & 0x00ff0000u)>>16;
  dhcp->msg_out->options[dhcp->options_out_len++] = (value & 0x0000ff00u)>>8;
  dhcp->msg_out->options[dhcp->options_out_len++] = (value & 0x000000ffu);
}

/**
 * Extract the DHCP message and the DHCP options.
 *
 * Extract the DHCP message and the DHCP options, each into a contiguous
 * piece of memory. As a DHCP message is variable sized by its options,
 * and also allows overriding some fields for options, the easy approach
 * is to first unfold the options into a conitguous piece of memory, and
 * use that further on.
 * 
 */
uint32_t dhcp_unfold_reply(struct dhcp *dhcp)
{
  struct pstore *p = dhcp->p;
  uint8_t *ptr;
  uint16_t i;
  uint16_t j = 0;
  
  /* free any left-overs from previous unfolds */
  dhcp_free_reply(dhcp);
  /* options present? */
  if (dhcp->p->tot_len > (sizeof(struct dhcp_msg) - DHCP_OPTIONS_LEN)) {
    dhcp->options_in_len = dhcp->p->tot_len - (sizeof(struct dhcp_msg) 
					       - DHCP_OPTIONS_LEN);
    dhcp->options_in = mem_malloc(dhcp->options_in_len);
    if (dhcp->options_in == NULL) {
      kprintf(KR_OSTREAM,"dhcp_unfold_reply:could not alloc. dhcp->options"); 
      return RC_NetSys_PbufsExhausted;
    }
  }
  dhcp->msg_in = mem_malloc(sizeof(struct dhcp_msg) - DHCP_OPTIONS_LEN);
  if (dhcp->msg_in == NULL) {
    kprintf(KR_OSTREAM,"dhcp_unfold_reply():could not allocate dhcp->msg_in"); 
    mem_free((void *)dhcp->options_in);
    dhcp->options_in = NULL;
    return RC_NetSys_PbufsExhausted;
  }
  
  ptr = (uint8_t *)dhcp->msg_in;
  /* proceed through struct dhcp_msg */
  for (i = 0; i < sizeof(struct dhcp_msg) - DHCP_OPTIONS_LEN; i++) {
    *ptr++ = ((uint8_t *)PSTORE_PAYLOAD(p,dhcp->pcb->ssid))[j++];
    /* reached end of pstore? */
    if (j == p->len) {
      /* proceed to next pstore in chain */
      p = PSTORE_NEXT(p,dhcp->pcb->ssid);
      j = 0;
    }
  }
  kprintf(KR_OSTREAM,"dhcp_unfold_reply:copied %u bytes into dhcp->msg_in",
	  i); 
  if (dhcp->options_in != NULL) {
    ptr = (uint8_t *)dhcp->options_in;
    /* proceed through options */
    for (i = 0; i < dhcp->options_in_len; i++) {
      *ptr++ = ((uint8_t *)PSTORE_PAYLOAD(p,dhcp->pcb->ssid))[j++];
      /* reached end of pstore? */
      if (j == p->len) {
        /* proceed to next pstore in chain */
        p = PSTORE_NEXT(p,dhcp->pcb->ssid);
        j = 0;
      }
    }
    kprintf(KR_OSTREAM,"dhcp_unfold_reply:copied%ubytes to dhcp->options_in[]",
	    i); 
  }
  return RC_OK;
}


/**
 * Free the incoming DHCP message including contiguous copy of 
 * its DHCP options.
 */
void dhcp_free_reply(struct dhcp *dhcp)
{
  if (dhcp->msg_in != NULL) {
    mem_free((void *)dhcp->msg_in);
    dhcp->msg_in = NULL;
  }
  if (dhcp->options_in) {
    mem_free((void *)dhcp->options_in);
    dhcp->options_in = NULL;
    dhcp->options_in_len = 0;
  }
  kprintf(KR_OSTREAM,"dhcp_free_reply(): free'd"); 
}


/* If an incoming DHCP message is in response to us, 
 * then trigger the state machine */

void dhcp_recv(void *arg, struct udp_pcb *pcb, struct pstore *p, 
		      struct ip_addr *addr, uint16_t port)
{
  struct netif *netif = (struct netif *)arg;
  struct dhcp *dhcp = netif->dhcp;
  struct dhcp_msg *reply_msg = 
    (struct dhcp_msg *)PSTORE_PAYLOAD(p,dhcp->pcb->ssid);
  uint8_t *options_ptr;
  uint8_t msg_type;
  uint8_t i;
  
  DEBUGDHCP
    kprintf(KR_OSTREAM,"dhcp_recv() from DHCP server %u.%u.%u.%u:%u",
	    (uint8_t)(ntohl(addr->addr) >> 24 & 0xff),
	    (uint8_t)(ntohl(addr->addr) >> 16 & 0xff),
	    (uint8_t)(ntohl(addr->addr) >>  8 & 0xff), 
	    (uint8_t)(ntohl(addr->addr) & 0xff), port);
    
  /* prevent warnings about unused arguments */
  (void)pcb; (void)addr; (void)port;
  dhcp->p = p;
  /* TODO: check packet length before reading them */
  if (reply_msg->op != DHCP_BOOTREPLY) {
    kprintf(KR_OSTREAM,"not a DHCP reply message, but type %u",reply_msg->op);
    pstore_free(p,dhcp->pcb->ssid);
    dhcp->p = NULL;
    return;
  }  
  /* iterate through hardware address and match against DHCP message */
  for (i = 0; i < netif->hwaddr_len; i++) {
    if (netif->hwaddr[i] != reply_msg->chaddr[i]) { 
      DEBUGDHCP
	kprintf(KR_OSTREAM,"netif->addr[%u]=%2x != reply_msg->chaddr[%u]=%2x",
		i, netif->hwaddr[i], i, reply_msg->chaddr[i]);
      pstore_free(p,dhcp->pcb->ssid);
      dhcp->p = NULL;
      return;
    }      
  }
  
  /* match transaction ID against what we expected */
  if (ntohl(reply_msg->xid) != dhcp->xid) {
    kprintf(KR_OSTREAM,"transaction id mismatch");
    pstore_free(p,dhcp->pcb->ssid);
    dhcp->p = NULL;
    return;
  }else {
    kprintf(KR_OSTREAM,"Boot Reply seemss to be for us: %x",
	    htonl(reply_msg->xid));
  }
  
  /* option fields could be unfold? */
  if (dhcp_unfold_reply(dhcp) != RC_OK) {
    kprintf(KR_OSTREAM,"problem unfolding DHCP message-too short on memory?");
    pstore_free(p,dhcp->pcb->ssid);
    dhcp->p = NULL;
    return;
  }
  
  kprintf(KR_OSTREAM ,"searching DHCP_OPTION_MESSAGE_TYPE");
  /* obtain pointer to DHCP message type */ 
  options_ptr = dhcp_get_option_ptr(dhcp, DHCP_OPTION_MESSAGE_TYPE);
  if (options_ptr == NULL) {
    kprintf(KR_OSTREAM,"DHCP_OPTION_MESSAGE_TYPE option not found"); 
    pstore_free(p,dhcp->pcb->ssid);
    dhcp->p = NULL;
    return;
  }  

  /* read DHCP message type */
  msg_type = dhcp_get_option_byte(options_ptr + 2);
  /* message type is DHCP ACK? */
  if (msg_type == DHCP_ACK) {
    kprintf(KR_OSTREAM,"DHCP_ACK received"); 
    /* in requesting state? */
    if (dhcp->state == DHCP_REQUESTING) {
      dhcp_handle_ack(netif);
      dhcp->request_timeout = 0;
#if DHCP_DOES_ARP_CHECK
      /* check if the acknowledged lease address is already in use */
      dhcp_check(netif);
#else
      /* bind interface to the acknowledged lease address */
      dhcp_bind(netif);
#endif
    }
    /* already bound to the given lease address? */
    else if ((dhcp->state == DHCP_REBOOTING) || 
	     (dhcp->state == DHCP_REBINDING) || 
	     (dhcp->state == DHCP_RENEWING)) {
      dhcp->request_timeout = 0;
      dhcp_bind(netif);
    }
  }
  /* received a DHCP_NAK in appropriate state? */
  else if ((msg_type == DHCP_NAK) &&
	   ((dhcp->state == DHCP_REBOOTING) || 
	    (dhcp->state == DHCP_REQUESTING) || 
	    (dhcp->state == DHCP_REBINDING) || 
	    (dhcp->state == DHCP_RENEWING  ))) {
    kprintf(KR_OSTREAM,"DHCP_NAK received"); 
    dhcp->request_timeout = 0;
    //dhcp_handle_nak(netif);
  }
  /* received a DHCP_OFFER in DHCP_SELECTING state? */
  else if ((msg_type == DHCP_OFFER) && (dhcp->state == DHCP_SELECTING)) {
    kprintf(KR_OSTREAM,"DHCP_OFFER received in DHCP_SELECTING state"); 
    dhcp->request_timeout = 0;
    /* remember offered lease */
    dhcp_handle_offer(netif);
  }
  pstore_free(p,dhcp->pcb->ssid);
  dhcp->p = NULL;
}


uint32_t dhcp_create_request(struct netif *netif)
{
  struct dhcp *dhcp = netif->dhcp;
  uint16_t i;
  
  dhcp->p_out = pstore_alloc(PSTORE_TRANSPORT,sizeof(struct dhcp_msg), 
			     XMIT_STACK_SPACE,dhcp->pcb->ssid);
  if (dhcp->p_out == NULL) {
    kprintf(KR_OSTREAM,"dhcp_create_request(): could not allocate pstore");
    return RC_NetSys_PbufsExhausted;
  }
  /* give unique transaction identifier to this request */
  dhcp->xid = xid++;  
  
  //dhcp->msg_out = dhcp->p_out->payload;
  dhcp->msg_out = (struct dhcp_msg*)PSTORE_PAYLOAD(dhcp->p_out,dhcp->pcb->ssid);
  
  dhcp->msg_out->op = DHCP_BOOTREQUEST;
  /* TODO: make link layer independent */  
  dhcp->msg_out->htype = DHCP_HTYPE_ETH;  
  /* TODO: make link layer independent */  
  dhcp->msg_out->hlen = DHCP_HLEN_ETH;  
  dhcp->msg_out->hops = 0;
  dhcp->msg_out->xid = htonl(dhcp->xid);  
  dhcp->msg_out->secs = 0;
  dhcp->msg_out->flags = 0;
  dhcp->msg_out->ciaddr = netif->ip_addr.addr;
  dhcp->msg_out->yiaddr = 0;
  dhcp->msg_out->siaddr = 0;
  dhcp->msg_out->giaddr = 0;
  for (i = 0; i < DHCP_CHADDR_LEN; i++) {
    /* copy netif hardware address, pad with zeroes */
    dhcp->msg_out->chaddr[i] = (i < netif->hwaddr_len) ? netif->hwaddr[i] : 0/* pad byte*/;
  }
  for (i = 0; i < DHCP_SNAME_LEN; i++) dhcp->msg_out->sname[i] = 0;
  for (i = 0; i < DHCP_FILE_LEN; i++) dhcp->msg_out->file[i] = 0;
  dhcp->msg_out->cookie = htonl(0x63825363UL);
  dhcp->options_out_len = 0;
  /* fill options field with an incrementing array (for debugging purposes) */
  for (i = 0; i < DHCP_OPTIONS_LEN; i++) dhcp->msg_out->options[i] = i;
  return RC_OK;
}

void dhcp_delete_request(struct netif *netif)
{
  struct dhcp *dhcp = netif->dhcp;

  pstore_free(dhcp->p_out,dhcp->pcb->ssid);
  dhcp->p_out = NULL;
  dhcp->msg_out = NULL;
}

/**
 * Add a DHCP message trailer
 *
 * Adds the END option to the DHCP message, and if
 * necessary, up to three padding bytes.
 */
void dhcp_option_trailer(struct dhcp *dhcp)
{
  dhcp->msg_out->options[dhcp->options_out_len++] = DHCP_OPTION_END;
  /* packet is too small, or not 4 byte aligned? */
  while ((dhcp->options_out_len < DHCP_MIN_OPTIONS_LEN) || 
	 (dhcp->options_out_len & 3)) {
    /* add a fill/padding byte */
    dhcp->msg_out->options[dhcp->options_out_len++] = 0;
  }
}

/**
 * Find the offset of a DHCP option inside the DHCP message.
 *
 * @param client DHCP client
 * @param option_type
 *
 * @return a byte offset into the UDP message where the option was found, or
 * zero if the given option was not found.
 */
uint8_t *dhcp_get_option_ptr(struct dhcp *dhcp, uint8_t option_type)
{
  uint8_t overload = DHCP_OVERLOAD_NONE;
  
  /* options available? */
  if ((dhcp->options_in != NULL) && (dhcp->options_in_len > 0)) {
    /* start with options field */
    uint8_t *options = (uint8_t *)dhcp->options_in;
    uint16_t offset = 0;
    /*at least 1 byte to read and no end marker, 
     * then at least 3 bytes to read? */
    while ((offset < dhcp->options_in_len) && 
	   (options[offset] != DHCP_OPTION_END)) {
      /* are the sname and/or file field overloaded with options? */
      if (options[offset] == DHCP_OPTION_OVERLOAD) {
	kprintf(KR_OSTREAM,"overloaded message detected");
	/* skip option type and length */
	offset += 2;
	overload = options[offset++];
      }
      /* requested option found */
      else if (options[offset] == option_type) {
	kprintf(KR_OSTREAM ,"option found at offset %u in options", offset);
	return &options[offset];
	/* skip option */
      } else {
	DEBUGDHCP 
	  kprintf(KR_OSTREAM,"skipping option %u in options", options[offset]);
	/* skip option type */
	offset++;
	/* skip option length, and then length bytes */
	offset += 1 + options[offset];
      }
    }
    /* is this an overloaded message? */
    if (overload != DHCP_OVERLOAD_NONE) {
      uint16_t field_len;
      if (overload == DHCP_OVERLOAD_FILE) {
	kprintf(KR_OSTREAM,"overloaded file field");
	options = (uint8_t *)&dhcp->msg_in->file;
	field_len = DHCP_FILE_LEN;
      } else if (overload == DHCP_OVERLOAD_SNAME) {
	kprintf(KR_OSTREAM,"overloaded sname field");
	options = (uint8_t *)&dhcp->msg_in->sname;
	field_len = DHCP_SNAME_LEN;
	/* TODO: check if else if () is necessary */
      } else {
	kprintf(KR_OSTREAM,"overloaded sname and file field");
	options = (uint8_t *)&dhcp->msg_in->sname;
	field_len = DHCP_FILE_LEN + DHCP_SNAME_LEN;
      }
      offset = 0;
      
      /* at least 1 byte to read and no end marker */
      while ((offset < field_len) && (options[offset] != DHCP_OPTION_END)) {
	if (options[offset] == option_type) {
	  kprintf(KR_OSTREAM ,"option found at offset=%u", offset);
	  return &options[offset];
	  /* skip option */
	} else {
	  kprintf(KR_OSTREAM ,"skipping option %u", options[offset]);
	  /* skip option type */
	  offset++;
	  offset += 1 + options[offset];
	}
      }
    }
  }
  return 0;
}

/**
 * Return the byte of DHCP option data.
 *
 * @param client DHCP client.
 * @param ptr pointer obtained by dhcp_get_option_ptr().
 *
 * @return byte value at the given address.
 */
uint8_t dhcp_get_option_byte(uint8_t *ptr)
{
  kprintf(KR_OSTREAM,"option byte value=%u", *ptr);
  return *ptr;
}                             


/**
 * Return the 16-bit value of DHCP option data.
 *
 * @param client DHCP client.
 * @param ptr pointer obtained by dhcp_get_option_ptr().
 *
 * @return byte value at the given address.
 */
uint16_t dhcp_get_option_short(uint8_t *ptr)
{
  uint16_t value;
  value = *ptr++ << 8;
  value |= *ptr;
  kprintf(KR_OSTREAM,"option short value=%u", value);
  return value;
}                             

/**
 * Return the 32-bit value of DHCP option data.
 *
 * @param client DHCP client.
 * @param ptr pointer obtained by dhcp_get_option_ptr().
 *
 * @return byte value at the given address.
 */
uint32_t dhcp_get_option_long(uint8_t *ptr)
{
  uint32_t value;
  value = (uint32_t)(*ptr++) << 24;
  value |= (uint32_t)(*ptr++) << 16;
  value |= (uint32_t)(*ptr++) << 8;
  value |= (uint32_t)(*ptr++);
  kprintf(KR_OSTREAM,"option long value=%d", value);
  return value;
}              
