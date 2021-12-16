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

/* ICMP Protocol : rfc 792. */

#ifndef _ICMP_H_
#define _ICMP_H_

#include <eros/endian.h>

#include "pbuf.h"
#include "../netif/netif.h"

#define ICMP_TTL    255 /* ICMP Time-To-Live */

#define ICMP_ER   0     /* echo reply */
#define ICMP_DUR  3     /* destination unreachable */
#define ICMP_SQ   4     /* source quench */
#define ICMP_RD   5     /* redirect */
#define ICMP_ECHO 8     /* echo */
#define ICMP_TE   11    /* time exceeded */
#define ICMP_PP   12    /* parameter problem */
#define ICMP_TS   13    /* timestamp */
#define ICMP_TSR  14    /* timestamp reply */
#define ICMP_IRQ  15    /* information request */
#define ICMP_IR   16    /* information reply */

enum icmp_dur_type {
  ICMP_DUR_NET = 0,    /* net unreachable */
  ICMP_DUR_HOST = 1,   /* host unreachable */
  ICMP_DUR_PROTO = 2,  /* protocol unreachable */
  ICMP_DUR_PORT = 3,   /* port unreachable */
  ICMP_DUR_FRAG = 4,   /* fragmentation needed and DF set */
  ICMP_DUR_SR = 5      /* source route failed */
};

enum icmp_te_type {
  ICMP_TE_TTL = 0,     /* time to live exceeded in transit */
  ICMP_TE_FRAG = 1     /* fragment reassembly time exceeded */
};

void icmp_input(struct pbuf *p,struct netif *netif);
void icmp_dest_unreach(struct pbuf *p, enum icmp_dur_type t);
void icmp_time_exceeded(struct pbuf *p, enum icmp_te_type t);
uint32_t icmp_echo_output(uint16_t id,struct ip_addr ipaddr);

struct icmp_echo_hdr {
  uint16_t _type_code;
  uint16_t chksum;
  uint16_t id;
  uint16_t seqno;
};

struct icmp_dur_hdr {
  uint16_t _type_code;
  uint16_t chksum;
  uint32_t unused;
};

struct icmp_te_hdr {
  uint16_t _type_code;
  uint16_t chksum;
  uint32_t unused;
};

#define ICMPH_TYPE(hdr) (ntohs((hdr)->_type_code) >> 8)
#define ICMPH_CODE(hdr) (ntohs((hdr)->_type_code) & 0xff)

#define ICMPH_TYPE_SET(hdr,type) ((hdr)->_type_code = htons(ICMPH_CODE(hdr) \
                                                      | ((type) << 8)))
#define ICMPH_CODE_SET(hdr,code) ((hdr)->_type_code = htons((code) \
                                                   | (ICMPH_TYPE(hdr) << 8)))

#endif /*_ICMP_H_*/
