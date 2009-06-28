#ifndef __LWIP_LWIPOPTS_H__
#define __LWIP_LWIPOPTS_H__
/*
 * Copyright (C) 2008, 2009, Strawberry Development Group.
 *
 * This file is part of the CapROS Operating System.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2,
 * or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */
/* This material is based upon work supported by the US Defense Advanced
Research Projects Agency under Contract No. W31P4Q-07-C-0070.
Approved for public release, distribution unlimited. */

//#define MALLOC_DEBUG
#ifdef MALLOC_DEBUG
#define POISON1 ((void*)0xbadbad01)
#define POISON2 ((void*)0xbadbad02)
#define POISON3 ((void*)0xbadbad03)
#define POISON4 ((void*)0xbadbad04)
#define POISON5 ((void*)0xbadbad05)
#endif

#define NO_SYS 1
#define LWIP_SOCKET 0
#define LWIP_NETCONN 0

#define MEM_LIBC_MALLOC 1 // use libc's malloc
#include <stdlib.h>		// and declare it
#define MEM_ALIGNMENT 4	// correct for ARM
// IPv4 requires TCP_MSS be at least 576-20-20 = 536
#define MEMP_NUM_TCP_PCB 15
#define TCP_MSS 1500
#define TCP_SND_BUF (TCP_MSS * 4)
#define TCP_WND 4096
/* Put retransmissions on a short leash.
 * Otherwise, while waiting for acknowledgement, we can accumulate unsent data,
 * exhausting pbufs.
 * See also change to tcp_backoff. */
#define TCP_MAXRTX 4
#define PBUF_POOL_SIZE 80
// I see no harm in making the following large:
#define TCP_SND_QUEUELEN                (16 * (TCP_SND_BUF/TCP_MSS))
#define MEMP_NUM_TCP_SEG                TCP_SND_QUEUELEN

#define LWIP_DEBUG
#if 0
//#define ETHARP_DEBUG LWIP_DBG_ON
//#define IP_DEBUG    LWIP_DBG_ON
//#define TCP_DEBUG   LWIP_DBG_ON
//#define LWIP_DBG_MIN_LEVEL LWIP_DBG_LEVEL_SEVERE // leave this at the default
#else	// normally, want to see severe errors
#define TCP_OUTPUT_DEBUG LWIP_DBG_ON
#define LWIP_DBG_MIN_LEVEL LWIP_DBG_LEVEL_SEVERE
#endif

struct tcp_pcb;
void ValidatePCB(struct tcp_pcb * pcb);

#endif /* __LWIP_LWIPOPTS_H__ */
