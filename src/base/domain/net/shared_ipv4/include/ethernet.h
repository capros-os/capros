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

#ifndef _ETHERNET_H_
#define _ETHERNET_H_

/*The number of bytes in an ethernet (MAC) address*/
#define	ETHER_ADDR_LEN	6

/*The number of bytes in the type field*/
#define	ETHER_TYPE_LEN	2

/*The number of bytes in the trailing CRC field*/
#define	ETHER_CRC_LEN	4

/*The length of the combined header*/
#define	ETHER_HDR_LEN	(ETHER_ADDR_LEN*2+ETHER_TYPE_LEN)

/*The minimum packet length*/
#define	ETHER_MIN_LEN	64

/*The maximum packet length*/
#define	ETHER_MAX_LEN	1518

#define	ETHERMTU	(ETHER_MAX_LEN-ETHER_HDR_LEN-ETHER_CRC_LEN)
#define	ETHERMIN	(ETHER_MIN_LEN-ETHER_HDR_LEN-ETHER_CRC_LEN)

/*A macro to validate a length with*/
#define	ETHER_IS_VALID_LEN(foo)	\
	((foo) >= ETHER_MIN_LEN && (foo) <= ETHER_MAX_LEN)

struct eth_addr{
  uint8_t addr[6];
};

/*Structure of a 10Mb/s Ethernet header*/
struct	eth_hdr {
  struct eth_addr dest;
  struct eth_addr src;
  uint16_t type;
};

struct ether_pkt {
  struct eth_hdr header;
  char   data[ETHERMTU];
};


/*Structure of a 48-bit Ethernet address*/
struct	ether_addr {
  uint8_t octet[ETHER_ADDR_LEN];
};

/* Type of packet the ethernet carries */
#define	ETHERTYPE_8023	    0x0004  /* IEEE 802.3 packet*/
#define ETHERTYPE_IP        0x0800  /* IP Protocol*/
#define ETHERTYPE_ARP       0x0806  /* ARP */
#define	ETHERTYPE_IPV6	    0x86DD  /* IP protocol version 6 */
#define	ETHERTYPE_LOOPBACK  0x9000  /* Loopback: used to test interfaces */
#define	ETHERTYPE_MAX	    0xFFFF  /* Maximum valid ethernet type, reserved*/

#endif /* _ETHERNET_H_ */


