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

/* Internet Protocol : see rfc 791. Taken from freebsd */

#ifndef _IP_H_
#define _IP_H_

#include <eros/endian.h>

#include "pstore.h"
#include "ip_addr.h"

/* From rfc 791 */

struct ip_hdr{
  uint16_t  ip_vhl_tos;         /* header length & version,type of service */
  uint16_t  ip_len;             /* total length */
  uint16_t  ip_id;              /* identification */
  uint16_t  ip_off;             /* fragment offset field */
#define	IP_RF 0x8000	        /* reserved fragment flag */
#define	IP_DF 0x4000	        /* dont fragment flag */
#define	IP_MF 0x2000	        /* more fragments flag */
#define	IP_OFFMASK 0x1fff       /* mask for fragmenting bits */
  uint16_t  ip_ttl_proto;       /* time to live, protocol */
  uint16_t ip_sum;	        /* checksum */
  struct ip_addr src,dest;      /* source and dest address */
};

#define IP_HLEN           20

#define IP_HDRINCL        NULL

#define IP_PROTO_ICMP     1
#define IP_PROTO_UDP      17
#define IP_PROTO_UDPLITE  170
#define IP_PROTO_TCP      6

#define IPH_V(hdr)      (ntohs((hdr)->ip_vhl_tos) >> 12)
#define IPH_HL(hdr)     ((ntohs((hdr)->ip_vhl_tos) >> 8) & 0x0f)
#define IPH_TOS(hdr)    (htons((ntohs((hdr)->ip_vhl_tos) & 0xff)))
#define IPH_LEN(hdr)    ((hdr)->ip_len)
#define IPH_ID(hdr)     ((hdr)->ip_id)
#define IPH_OFFSET(hdr) ((hdr)->ip_off)
#define IPH_TTL(hdr)    (ntohs((hdr)->ip_ttl_proto) >> 8)
#define IPH_PROTO(hdr)  (ntohs((hdr)->ip_ttl_proto) & 0xff)
#define IPH_CHKSUM(hdr) ((hdr)->ip_sum)

#define IPH_VHLTOS_SET(hdr,v,hl,tos) (hdr)->ip_vhl_tos=(htons(((v) << 12)\
                                                       | ((hl) << 8) | (tos)))
#define IPH_LEN_SET(hdr,len) (hdr)->ip_len = (len)
#define IPH_ID_SET(hdr,id) (hdr)->ip_id = (id)
#define IPH_OFFSET_SET(hdr,off) (hdr)->ip_off = (off)
#define IPH_TTL_SET(hdr, ttl) (hdr)->ip_ttl_proto = \
                              (htons(IPH_PROTO(hdr) | ((ttl) << 8)))
#define IPH_PROTO_SET(hdr, proto) (hdr)->ip_ttl_proto = \
                              (htons((proto) | (IPH_TTL(hdr) << 8)))
#define IPH_CHKSUM_SET(hdr,chksum) (hdr)->ip_sum = (chksum)

/* Function prototypes */
void ip_init(void);
uint32_t ip_input(struct pstore *p,int ssid);
uint32_t ip_output(struct pstore *p,int ssid,
		   struct ip_addr *src,struct ip_addr *dest,
		   uint8_t ttl, uint8_t proto);

#endif /* _IP_H_*/
