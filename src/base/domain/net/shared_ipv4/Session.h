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

#include <eros/NodeKey.h>

#include "include/udp.h"
#include "include/tcp.h"
#include "mapping_table.h"

#define MAX_SESSIONS      3
#define MAX_ICMP_SESSIONS 5
#define MAX_BUF_SIZE      4096
#define MAX_PORTS         65536

#define UDP_SESSION    1
#define TCP_SESSION    2
#define IP_SESSION     3

struct session{
  int stackid; 
  int  ssid; /* Session Id to identify our client connection id */
  struct mapping_table mt[4];
};

struct IcmpSession{
  uint32_t ssid;
  uint8_t listening;
};

/* Initialize the session manager */
void init_sessions();

/* Check if there exists a icmp session by ssid */
uint32_t find_icmpsession(uint32_t);

/* Activate the session */
uint32_t activate_session(int16_t ssid,uint32_t xmit_client_buffer,
			  uint32_t xmit_stack_buffer,
			  uint32_t recv_client_buffer,
			  uint32_t recv_stack_buffer);

/* Tear down session */
void teardown_session(uint32_t ssid);

/* Check if there exists a session by ssid */
int find_session(uint32_t ssid);

/* Returns a unique sesssion id */
uint32_t new_session(); 

/* Main dispatching code */
uint32_t SessionRequest(Message *msg);

/* Keep updating the session receive timeouts */
void session_timeouts_update();



/* Park client */
#define parkSession(msg) {\
  msg->invType = IT_Retry;\
  msg->snd_w1 = RETRY_SET_LIK|RETRY_SET_WAKEINFO;\
  msg->snd_w2 = msg->rcv_w1; /* wakeinfo value */\
  msg->snd_key0 = KR_PARK_WRAP;\
}

/* retry the client and release it */
#define wakeSession(parkingNo) {\
  node_wake_some_no_retry(KR_PARK_NODE,0,0,parkingNo);\
}

uint32_t IsIcmpListening(int index);
void	setIcmpListening(int index);


/* Debugging aid */
void debug_active_sessions();

#endif /*__SESSION_H__*/
