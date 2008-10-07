/*
 * Copyright (C) 2008, Strawberry Development Group.
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

// Constituents:
#define KC_VOLSIZE 0

// KR_IPAddrs initially has a number cap with the IP address, mask, and gateway.
#define KR_IPAddrs	KR_APP2(0)
#define KR_DeviceEntry	KR_APP2(1)	// Only interrupt thread needs this

/* Each socket has several capability variables allocated in the keystore.
 * The address in the keystore is the same as the address of the socket. 
 * (We depend on the fact that sco_numSlots <= sizeof(TCPSocket).)
 * The offsets from that address are used as follows: */
#define sco_forwarder 0 // a non-opaque capability to the forwarder for
			// this socket
#define sco_connecter 1	// a resume capability to the connecting process
// NOTE the following overloads offset 1, because writing and connecting
// cannot be done simultaneously.
#define sco_writer 1	// a resume capability to the writing process
#define sco_reader 2	// a resume capability to the reading process
#define sco_numSlots 3
