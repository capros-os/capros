/*
 * Copyright (C) 2002, Jonathan S. Shapiro.
 * Copyright (C) 2007, Strawberry Development Group.
 *
 * This file is part of the CapROS Operating System distribution.
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
/* This material is based upon work supported by the US Defense Advanced
Research Projects Agency under Contract No. W31P4Q-07-C-0070.
Approved for public release, distribution unlimited. */

#ifndef _NETSYSKEYS_H_
#define _NETSYSKEYS_H_

/* Since the whole network stack is a monolithic process, including this
 * file saves us the work of passing around the key slots while calling 
 * functions */

#include <domain/Runtime.h>

#define KR_OSTREAM      KR_APP(0)  /* verbose output */

#define KR_PARK_NODE    KR_APP(1)  /* key to wrapper node to which retrying */
#define KR_PARK_WRAP    KR_APP(2)  /* clients will be redirected onto this */

#define KR_MEMMAP_C     KR_APP(3)  /* key to memmap domain */
#define KR_ZSC          KR_APP(4)  /* key to Zero Space Constructor */

#define KR_SLEEP        KR_APP(5)  /* Sleep key */

#define KR_NEW_SESSION  KR_APP(6)  /* new unique session key (for clients) */
#define KR_NEW_NODE     KR_APP(7) /* temp reg. for a new node */     

#define KR_START        KR_APP(8) /* Start key */

/* our Alarm timer constructor and start key */
#define KR_TIMEOUT_AGENT KR_APP(9)
#define KR_XMIT_HELPER   KR_APP(10)  /* key to the xmit helper */
#define KR_RECV_HELPER   KR_APP(11)  /* key to the recv helper */

#define KR_ENET          KR_APP(12)  /* key to the driver */
#define KR_ENET_BLOCKED  KR_APP(13)  /* A retry-park capable key */

/* Console stream key for debugging purposes */
#define KR_CONSTREAM    KR_APP(14)

#define KR_XMIT_CLIENT_BUF KR_APP(15)  /* key to new address space for 
					* client transmit buf */ 
#define KR_XMIT_STACK_BUF KR_APP(16)  /* key to new address space for 
					* stack transmit buf */ 

#define KR_RCV_CLIENT_BUF  KR_APP(17)  /* key to new address space for client
					* receive buf */
#define KR_RCV_STACK_BUF  KR_APP(18)  /* key to new address space for stack
				       * receive buf */

#define KR_SCRATCH2     KR_APP(19)
#define KR_SCRATCH      KR_APP(20) /* Scratch pad */

/* The key to the client's bank */
#define KR_CLIENT_BANK  KR_ARG(0)

/* The client's space bank key will be stashed in the node allocated
 * from that bank in the following slot */
#define STASH_CLIENT_BANK  1


#endif /* _NETSYSKEYS_H_ */
