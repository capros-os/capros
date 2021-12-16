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

#ifndef _DHCP_H_
#define _DHCP_H_

#include <eros/target.h>

#include "udp.h"

/* period (in seconds) of the application calling dhcp_coarse_tmr() */
#define DHCP_COARSE_TIMER_SECS 60 
/* period (in milliseconds) of the application calling dhcp_fine_tmr() */
#define DHCP_FINE_TIMER_MSECS 500 

struct dhcp
{
  uint8_t state;              /* current DHCP state machine state */
  uint8_t tries;              /* retries of current request */
  uint32_t xid;               /* transaction id. of last sent request */ 
  struct udp_pcb *pcb;        /* our connection to the DHCP server */ 
  struct pbuf *p;             /* (first) pbuf of incoming msg */
  struct dhcp_msg *msg_in;    /* incoming msg */
  struct dhcp_msg *options_in;/* incoming msg options */
  uint16_t options_in_len;    /* ingoing msg options length */
  struct pbuf *p_out;         /* pbuf of outcoming msg */
  struct dhcp_msg *msg_out;   /* outgoing msg */
  uint16_t options_out_len;   /* outgoing msg options length */
  uint16_t request_timeout;   /* #ticks with period DHCP_FINE_TIMER_SECS 
			       * for request timeout */
  uint16_t t1_timeout;	      /* #ticks with period DHCP_COARSE_TIMER_SECS
			       * for renewal time */
  uint16_t t2_timeout;	      /* #ticks with period DHCP_COARSE_TIMER_SECS
			       * for rebind time */
  struct ip_addr server_ip_addr;/* dhcp server address that offered this 
				 * lease */
  struct ip_addr offered_ip_addr;
  struct ip_addr offered_sn_mask;
  struct ip_addr offered_gw_addr;
  struct ip_addr offered_bc_addr;
  uint32_t offered_t0_lease;    /* lease period (in seconds) */
  uint32_t offered_t1_renew;    /* recommended renew time (usually 50%
				 * of lease period) */
  uint32_t offered_t2_rebind;   /* recommended rebind time (usually 66%
				 * of lease period)	*/

/** Patch #1308
 *	TODO: See dhcp.c "TODO"s
 */
#if 0
  struct ip_addr offered_si_addr;
  uint8_t *boot_file_name;
#endif
};

/** minimum set of fields of any DHCP message */
struct dhcp_msg
{
  uint8_t op;
  uint8_t htype;
  uint8_t hlen;
  uint8_t hops;
  uint32_t xid;
  uint16_t secs;
  uint16_t flags;
  uint32_t ciaddr;
  uint32_t yiaddr;
  uint32_t siaddr;
  uint32_t giaddr;
#define DHCP_CHADDR_LEN 16U
  uint8_t chaddr[DHCP_CHADDR_LEN];
#define DHCP_SNAME_LEN 64U
  uint8_t sname[DHCP_SNAME_LEN];
#define DHCP_FILE_LEN 128U
  uint8_t file[DHCP_FILE_LEN];
  uint32_t cookie;
#define DHCP_MIN_OPTIONS_LEN 68U
/** allow this to be configured in lwipopts.h, but not too small */
#if ((!defined(DHCP_OPTIONS_LEN)) || (DHCP_OPTIONS_LEN < DHCP_MIN_OPTIONS_LEN))
/** set this to be sufficient for your options in outgoing DHCP msgs */
#  define DHCP_OPTIONS_LEN DHCP_MIN_OPTIONS_LEN
#endif
  uint8_t options[DHCP_OPTIONS_LEN];
};

/* start DHCP configuration */
uint32_t dhcp_start();
/* stop DHCP configuration */
void dhcp_stop();
/* enforce lease renewal */
uint32_t dhcp_renew();
/* inform server of our IP address */
void dhcp_inform();

/* if enabled, check whether the offered IP address is not in use, using ARP */
//#if DHCP_DOES_ARP_CHECK
//void dhcp_arp_reply(struct netif *netif, struct ip_addr *addr);
//#endif

/* to be called every minute */
void dhcp_coarse_tmr(void);
/* to be called every half second */
void dhcp_fine_tmr(void);
 
/* DHCP message item offsets and length */
#define DHCP_MSG_OFS (UDP_DATA_OFS)  
  #define DHCP_OP_OFS (DHCP_MSG_OFS + 0)
  #define DHCP_HTYPE_OFS (DHCP_MSG_OFS + 1)
  #define DHCP_HLEN_OFS (DHCP_MSG_OFS + 2)
  #define DHCP_HOPS_OFS (DHCP_MSG_OFS + 3)
  #define DHCP_XID_OFS (DHCP_MSG_OFS + 4)
  #define DHCP_SECS_OFS (DHCP_MSG_OFS + 8)
  #define DHCP_FLAGS_OFS (DHCP_MSG_OFS + 10)
  #define DHCP_CIADDR_OFS (DHCP_MSG_OFS + 12)
  #define DHCP_YIADDR_OFS (DHCP_MSG_OFS + 16)
  #define DHCP_SIADDR_OFS (DHCP_MSG_OFS + 20)
  #define DHCP_GIADDR_OFS (DHCP_MSG_OFS + 24)
  #define DHCP_CHADDR_OFS (DHCP_MSG_OFS + 28)
  #define DHCP_SNAME_OFS (DHCP_MSG_OFS + 44)
  #define DHCP_FILE_OFS (DHCP_MSG_OFS + 108)
#define DHCP_MSG_LEN 236

#define DHCP_COOKIE_OFS (DHCP_MSG_OFS + DHCP_MSG_LEN)
#define DHCP_OPTIONS_OFS (DHCP_MSG_OFS + DHCP_MSG_LEN + 4)

#define DHCP_CLIENT_PORT 68	
#define DHCP_SERVER_PORT 67

/* DHCP client states */
#define DHCP_REQUESTING 1
#define DHCP_INIT       2
#define DHCP_REBOOTING  3
#define DHCP_REBINDING  4
#define DHCP_RENEWING   5
#define DHCP_SELECTING  6
#define DHCP_INFORMING  7
#define DHCP_CHECKING   8
#define DHCP_PERMANENT  9
#define DHCP_BOUND      10
/* not yet implemented #define DHCP_RELEASING 11 */
#define DHCP_BACKING_OFF 12
#define DHCP_OFF         13
 
#define DHCP_BOOTREQUEST 1
#define DHCP_BOOTREPLY   2

#define DHCP_DISCOVER 1
#define DHCP_OFFER    2
#define DHCP_REQUEST  3
#define DHCP_DECLINE  4
#define DHCP_ACK      5
#define DHCP_NAK      6
#define DHCP_RELEASE  7
#define DHCP_INFORM   8

#define DHCP_HTYPE_ETH 1

#define DHCP_HLEN_ETH 6

#define DHCP_BROADCAST_FLAG 15
#define DHCP_BROADCAST_MASK (1 << DHCP_FLAG_BROADCAST)

/* BootP options */
#define DHCP_OPTION_PAD         0
#define DHCP_OPTION_SUBNET_MASK 1 /* RFC 2132 3.3 */
#define DHCP_OPTION_ROUTER      3 
#define DHCP_OPTION_HOSTNAME    12
#define DHCP_OPTION_IP_TTL      23
#define DHCP_OPTION_MTU         26
#define DHCP_OPTION_BROADCAST   28
#define DHCP_OPTION_TCP_TTL     37
#define DHCP_OPTION_END         255

/* DHCP options */
#define DHCP_OPTION_REQUESTED_IP 50 /* RFC 2132 9.1, requested IP address */
#define DHCP_OPTION_LEASE_TIME   51 /* RFC 2132 9.2, time in seconds, 
				     * in 4 bytes */
#define DHCP_OPTION_OVERLOAD     52 /* RFC2132 9.3, use file and/or 
				     * sname field for options */
#define DHCP_OPTION_MESSAGE_TYPE 53 /* RFC 2132 9.6, important for DHCP */
#define DHCP_OPTION_MESSAGE_TYPE_LEN 1

#define DHCP_OPTION_SERVER_ID    54 /* RFC 2131 9.7, server IP address */
#define DHCP_OPTION_PARAMETER_REQUEST_LIST 55 /* RFC 2131 9.8, 
					       * requested option types */
#define DHCP_OPTION_MAX_MSG_SIZE 57 /* RFC 2131 9.10, message size 
				     * accepted >= 576 */
#define DHCP_OPTION_MAX_MSG_SIZE_LEN 2

#define DHCP_OPTION_T1        58 /* T1 renewal time */
#define DHCP_OPTION_T2        59 /* T2 rebinding time */
#define DHCP_OPTION_CLIENT_ID 61
#define DHCP_OPTION_TFTP_SERVERNAME 66
#define DHCP_OPTION_BOOTFILE 67

/* possible combinations of overloading the file and sname 
 * fields with options */
#define DHCP_OVERLOAD_NONE       0
#define DHCP_OVERLOAD_FILE       1
#define DHCP_OVERLOAD_SNAME	 2
#define DHCP_OVERLOAD_SNAME_FILE 3


#endif /* _DHCP_H_ */


