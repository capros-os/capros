/*
 * Copyright (C) 2003, Jonathan S. Shapiro.
 *
 * This file is part of the EROS Operating System runtime distribution.
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

#ifndef __SESSION_H__
#define __SESSION_H__

#include "include/udp.h"
#include "include/tcp.h"

#define MAX_SESSIONS      10
#define MAX_ICMP_SESSIONS 5
#define MAX_BUF_SIZE      15000
#define MAX_PORTS         65536

#define UDP_SESSION    1
#define TCP_SESSION    2
#define IP_SESSION     3

struct session{
  union {
    struct udp_pcb *udp;
    struct tcp_pcb *tcp;
  }pcb;
  uint32_t ssid; /* Session Id to identify our client connection id */
  uint8_t  type; /* type of session whether udp.tcp or raw ip */
};

struct IcmpSession{
  uint32_t ssid;
  uint8_t listening;
};

/* Initialize the session manager */
void init_sessions();

/* Check if there exists a icmp session by ssid */
uint32_t find_icmpsession(uint32_t);

/* Ask the client to retry */
void wakeSession(uint32_t Sessionno);

/* Returns a unique sesssion id */
uint32_t new_session(); 

/* Main dispatching code */
uint32_t SessionRequest(Message *msg);

/* Keep updating the session receive timeouts */
void session_timeouts_update();

uint32_t IsIcmpListening(int index);
void	setIcmpListening(int index);

#endif /*__SESSION_H__*/
