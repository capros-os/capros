/*
 * Copyright (C) 2008, 2010, Strawberry Development Group.
 *
 * This file is part of the CapROS Operating System.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */
/* This material is based upon work supported by the US Defense Advanced
Research Projects Agency under Contract No. W31P4Q-07-C-0070.
Approved for public release, distribution unlimited. */

/* Each socket has several capability variables allocated in the keystore.
 * The address in the keystore is the same as the address of the socket. 
 * (We depend on the fact that sco_numSlots <= sizeof(TCPSocket).)
 * The offsets from that address are used as follows: */
#define sco_forwarder 0 // a non-opaque capability to the forwarder for
			// this socket
#define sco_connecter 1	// a resume capability to the connecting process
// NOTE the following overloads offset 1, because sending and connecting
// cannot be done simultaneously.
#define sco_sender 1	// a resume capability to the sending process
#define sco_receiver 2	// a resume capability to the receiving process
#define sco_numSlots 3

/* Each UDP port has the following capability variables in the keystore: */
#define udp_forwarder 0	// a non-opaque capability to the forwarder for
			// this port
#define udp_receiver 1	// a resume capability to the receiving process
#define udp_numSlots 2

/* Each ListenSocket has several capability variables allocated in the keystore.
 * The address in the keystore is the same as the address of the socket. 
 * (We depend on the fact that ls_numSlots <= sizeof(TCPListenSocket).)
 * The offsets from that address are used as follows: */
#define ls_forwarder 0 // a non-opaque capability to the forwarder for
			// this ListenSocket
#define ls_accepter 1	// a resume capability to the accepting process
#define ls_numSlots 2

// KeyInfo values for start capabilities to this process:
#define keyInfo_IP 0
#define keyInfo_Timer 1
#define keyInfo_Device 2
#define keyInfo_Socket 3
#define keyInfo_ListenSocket 4
#define keyInfo_UDPPort 5

#define MMS_LIMIT 4096	// max size of a UDP datagram

#ifndef __ASSEMBLER__

#include <ipv4/lwip/ip_addr.h>

struct Message;

struct IPConfigv4 {
  struct ip_addr addr;
  struct ip_addr mask;
  struct ip_addr gw;
};

NORETURN void cap_main(struct IPConfigv4 * ipconf);

void UDPGetMaxSizes(struct Message * msg);
void UDPCreate(struct Message * msg);
void UDPDestroy(struct Message * msg);
void UDPReceive(struct Message * msg);
void UDPSend(struct Message * msg);

#endif // __ASSEMBLER__
