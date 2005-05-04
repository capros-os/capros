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

/* UDP Protocol : rfc 768. Taken from freebsd udp.h*/

#ifndef _UDP_H_
#define _UDP_H_

#include <eros/target.h>

#include "pbuf.h"
#include "inet.h"
#include "ip.h"

#define UDP_HLEN   8   /* 8 octets of header */
#define UDP_TTL    255

struct udp_hdr {
  uint16_t src;
  uint16_t dest;       /* src/dest UDP ports */
  uint16_t len;
  uint16_t chksum;
};

#define UDP_FLAGS_NOCHKSUM  0x01u
#define UDP_FLAGS_UDPLITE   0x02u
#define UDP_FLAGS_CONNECTED 0x04u

struct udp_pcb {
  struct udp_pcb *next;
  struct ip_addr  local_ip,remote_ip;
  uint16_t local_port,remote_port;
  uint8_t  flags;
  uint16_t chksum_len;

  /* UDP clients like dhcp can fill up this field to request
   * callback after udp_input */
  void (* recv)(void *arg, struct udp_pcb *pcb, struct pbuf *p,
                struct ip_addr *addr, uint16_t port);
  void *recv_arg;

  /* The session id of the process to whom this udp session belongs */
  uint32_t ssid;
  
  /* The application process is given this pbuf when udp data is received */
  struct pbuf *pbuf;
  
  /* Listening(0) or received (1)*/
  uint8_t listening;

  /* Time to time out */
  int32_t timeout;
  
  /* Did the timeout actually occur */
  uint8_t  overtime;
  
  /* How much time is up */
  int32_t timeup;
};

/* Function prototypes */
void udp_init(void);
void             udp_remove(struct udp_pcb *pcb);
struct udp_pcb*  udp_new(void);
uint32_t udp_send(struct udp_pcb *pcb, struct pbuf *p);
uint32_t udp_bind(struct udp_pcb *pcb,struct ip_addr *ipaddr,uint16_t port);
uint32_t udp_connect(struct udp_pcb *pcb,struct ip_addr *ipaddr,uint16_t port);
uint32_t udp_send(struct udp_pcb *pcb, struct pbuf *p);
void udp_recv(struct udp_pcb *pcb,
	      void (* recv)(void *arg, struct udp_pcb *upcb,
			    struct pbuf *p, struct ip_addr *addr,
			    uint16_t port),void *recv_arg);
void  udp_input(struct pbuf *p);

/* The list of UDP PCBs */
struct udp_pcb *udp_pcbs;
struct udp_pcb *pcb_cache;

#endif /*_UDP_H_*/
