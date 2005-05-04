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

#ifndef _ETHARP_H_
#define _ETHARP_H_

#include <eros/target.h>

#include "pbuf.h"
#include "ip_addr.h"
#include "ip.h"
#include "ethernet.h"
#include "../netif/netif.h"

#define ARP_TABLE_SIZE       10   /* Number of entries in the arp table */
#define ETHARP_ALWAYS_INSERT 1    /* IF 1:cache entries are updated or added 
				   * for every kind of ARP traffic or 
				   * broadcast IP traffic. Recommended for 
				   * routers.
				   * If defined to 0, only existing cache 
				   * entries are updated. Entries are added 
				   * when IP is sending to them.*/

/** the ARP message */
struct etharp_hdr {
  struct eth_hdr ethhdr;
  uint16_t hwtype;
  uint16_t proto;
  uint16_t _hwlen_protolen;
  uint16_t opcode;
  struct eth_addr shwaddr;
  struct ip_addr sipaddr;
  struct eth_addr dhwaddr;
  struct ip_addr dipaddr;
}__attribute__ ((packed));

struct ethip_hdr {
  struct eth_hdr eth;
  struct ip_hdr ip;
}__attribute__ ((packed));


#define ARP_TMR_INTERVAL 10000

#define ETHTYPE_ARP 0x0806
#define ETHTYPE_IP  0x0800

void etharp_init(void);
void etharp_tmr(void);
struct pbuf *etharp_ip_input(struct netif *netif,struct pbuf *p);
struct pbuf *etharp_arp_input(struct netif *netif,
			      struct eth_addr *ethaddr,struct pbuf *p);
struct pbuf *etharp_output(struct netif *netif,
			   struct ip_addr *ipaddr,struct pbuf *q);
uint32_t etharp_query(struct netif *,struct ip_addr *ipaddr, struct pbuf *q);
struct eth_addr *etharp_lookup(struct netif *netif,struct ip_addr *ipaddr,
			       struct pbuf *p);


#endif /* _ETHARP_H_ */


