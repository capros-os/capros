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

#include <stddef.h>

#include <eros/target.h>
#include <eros/NodeKey.h>
#include <eros/Invoke.h>

#include <domain/domdbg.h>

/* Our capabilities are all defined here */
#include "enetkeys.h"

/* Need to terminate tcp connections when ip address of 
 * the interface has changed*/
#include "../include/tcp.h"

#include "netif.h"

/* Set the netmask of interface */
void
netif_set_netmask(struct netif *netif, struct ip_addr *netmask)
{
  ip_addr_set(&(netif->netmask), netmask);
  kprintf(KR_OSTREAM,
	  "netif: netmask of interface "
	  "%c%c set to %u.%u.%u.%u\n",
	  netif->name[0], netif->name[1],
	  (uint8_t)(ntohl(netif->netmask.addr) >> 24 & 0xff),
	  (uint8_t)(ntohl(netif->netmask.addr) >> 16 & 0xff),
	  (uint8_t)(ntohl(netif->netmask.addr) >> 8 & 0xff),
	  (uint8_t)(ntohl(netif->netmask.addr) & 0xff));
}

/* Set the gateway of interface */
void
netif_set_gw(struct netif *netif, struct ip_addr *gw)
{
  ip_addr_set(&(netif->gw), gw);
  kprintf(KR_OSTREAM,
	  "netif: GW address of interface "
	  "%c%c set to %u.%u.%u.%u\n",
	  netif->name[0], netif->name[1], 
	  (uint8_t)(ntohl(netif->gw.addr) >> 24 & 0xff),
	  (uint8_t)(ntohl(netif->gw.addr) >> 16 & 0xff),
	  (uint8_t)(ntohl(netif->gw.addr) >> 8 & 0xff),
	  (uint8_t)(ntohl(netif->gw.addr) & 0xff));
}


/* FIX:: here we just set the address of an interface. We need to fix
 * this to avoid scenarios such as::
 *          - We are already assigned an IP address. Now suppose our ip
 *            address changes ( new dhcp server etc.) we need to terminate
 *            all our tcp connections. 
 */
void
netif_set_ipaddr(struct netif *netif, struct ip_addr *ipaddr)
{
  /* TODO: Handling of obsolete pcbs */
  /* See:  http://mail.gnu.org/archive/html/lwip-users/2003-03/msg00118.html */
#if 0
  struct tcp_pcb *pcb;
  struct tcp_pcb_listen *lpcb;
  
  /* address has changed? */
  if ((ip_addr_cmp(ipaddr, &(netif->ip_addr))) == 0) {
    extern struct tcp_pcb *tcp_active_pcbs;
    kprintf(KR_OSTREAM,"netif_set_ipaddr: netif address changed");
    pcb = tcp_active_pcbs;
    while (pcb != NULL) {
      if (ip_addr_cmp(&(pcb->local_ip), &(netif->ip_addr))) {
	/* The PCB is connected using the old ipaddr and must be aborted */
	struct tcp_pcb *next = pcb->next;
	kprintf(KR_OSTREAM,"netif_set_ipaddr: aborting pcb %d",(void *)pcb);
	tcp_abort(pcb);
	pcb = next;
      } else {
	pcb = pcb->next;
      }
    }
    for (lpcb = tcp_listen_pcbs; lpcb != NULL; lpcb = lpcb->next) {
      if (ip_addr_cmp(&(lpcb->local_ip), &(netif->ip_addr))) {
	/* The PCB is listening to the old ipaddr and
	 * is set to listen to the  new one instead */
	ip_addr_set(&(lpcb->local_ip), ipaddr);
      }
    }
  }
#endif
  ip_addr_set(&(netif->ip_addr), ipaddr);
  kprintf(KR_OSTREAM,
	  "netif: IP address of interface %c%c set to "
	  "%u.%u.%u.%u",
	  netif->name[0], netif->name[1], 
	  (uint8_t)(ntohl(netif->ip_addr.addr) >> 24 & 0xff),
	  (uint8_t)(ntohl(netif->ip_addr.addr) >> 16 & 0xff),
	  (uint8_t)(ntohl(netif->ip_addr.addr) >> 8 & 0xff),
	  (uint8_t)(ntohl(netif->ip_addr.addr) & 0xff));
}


