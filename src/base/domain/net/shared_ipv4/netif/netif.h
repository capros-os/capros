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

#ifndef __netif_h__
#define __netif_h__

#include "../include/ip_addr.h"
#include "../include/ethernet.h"
#include "../include/dhcp.h"
#include "../include/TxRxqueue.h"

#include <idl/capros/Stream.h>

/* must be the maximum of all used hardware address lengths
 * across all types of interfaces in use */
#define NETIF_MAX_HWADDR_LEN 6

/* What type of network card this is */
#define NETWORK_CARD ALTIMA 

/* whether the network interface is 'up'. this is
 * a software flag used to control whether this network
 * interface is enabled and processes traffic */
#define NETIF_FLAG_UP 0x1
/* if set, the netif has broadcast capability */
#define NETIF_FLAG_BROADCAST 0x2
/* if set, the netif is one end of a point-to-point connection */
#define NETIF_FLAG_POINTTOPOINT 0x4
/* if set, the interface is configured using DHCP */
#define NETIF_FLAG_DHCP 0x08
/* if set, the interface has an active link
 *  (set by the interface) */
#define NETIF_FLAG_LINK_UP 0x10

/* Interface specific data */
struct netif {
  
  /* IP address configuration in network byte order */
  struct ip_addr ip_addr;
  struct ip_addr netmask;
  struct ip_addr gw;

  /* number of bytes used in hwaddr */
  unsigned char hwaddr_len;
  
  /* Link level hardware address - we assume ethernet */
  unsigned char hwaddr[NETIF_MAX_HWADDR_LEN];

  /* the DHCP client state information for this netif */
  struct dhcp *dhcp;
  
  /* descriptive abbreviation */
  char name[2];
  
  /* Maximum transmission unit on this interface */
  uint16_t mtu;
 
  /* NETIF_FLAGS */
  uint8_t flags;
 
  /* Function pointers to appropriate functions */
  uint32_t (*interrupt)();
  uint32_t (*start_xmit)(struct pstore *p, int ssid);
  uint32_t (*close)();
  uint32_t (*rx)();
  void (*disable_ints)();
  void (*enable_ints)();
  
}__attribute__ ((packed));

/* For the time being we have only 1 ethernet device */
struct netif NETIF;

/* Get cpu ticks since last reboot */
static inline uint32_t 
rdtsc()
{
  uint32_t x;
  __asm__ volatile (".byte 0x0f, 0x31" : "=A" (x));
  return x;
}

#define rdpmc(counter,low,high) \
     __asm__ volatile("rdpmc" \
                          : "=a" (low), "=d" (high) \
                          : "c" (counter))

/* Function protoypes */
void netif_set_ipaddr(struct netif *netif, struct ip_addr *ipaddr);
void netif_set_netmask(struct netif *netif, struct ip_addr *netmast);
void netif_set_gw(struct netif *netif, struct ip_addr *gw);


/* IDs of some cards */
#define LANCE   0x2621
#define TORNADO 0x9200

#endif /*__netif_h__*/
