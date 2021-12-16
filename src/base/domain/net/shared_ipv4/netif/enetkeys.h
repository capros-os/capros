/*
 * Copyright (C) 2002, Jonathan S. Shapiro.
 * Copyright (C) 2007, Strawberry Development Group.
 *
 * This file is part of the CapROS Operating System distribution,
 * and is derived from the EROS Operating System distribution.
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

#ifndef _ENETKEYS_H_
#define _ENETKEYS_H_

/* All of enet pieces use these slots for their keys */

#include <domain/Runtime.h>

#define KR_OSTREAM      KR_APP(0)  /* verbose output */

#define KR_PARK_NODE    KR_APP(1)  /* key to wrapper node to which retrying */
#define KR_PARK_WRAP    KR_APP(2)  /* clients will be redirected onto this */

#define KR_DEVPRIVS     KR_APP(3)  /* talk to devices, make i/o calls */
#define KR_PHYSRANGE    KR_APP(4)  /* key to ranges */
#define KR_MEMMAP_C     KR_APP(5)  /* key to memmap domain */
#define KR_PCI_PROBE_C  KR_APP(6)  /* key to the pci_probe domain */

#define KR_SLEEP        KR_APP(7)  /* Sleep key */

#define KR_NEW_NODE     KR_APP(9) /* temp reg. for a new node */     

#define KR_START        KR_APP(10) /* Start key */

#define KR_HELPER       KR_APP(11)  /* key to helper (looks for IRQ) */

#define KR_BLOCKER      KR_APP(12)  /* blocked Wrapper key */

#define KR_XMIT_CLIENT_BUF KR_APP(13)  /* key to new address space for 
					* client transmit buf */ 
#define KR_XMIT_STACK_BUF  KR_APP(14)  /* key to new address space for 
					* stack transmit buf */ 

#define KR_RCV_CLIENT_BUF  KR_APP(15)  /* key to new address space for client
					* receive buf */
#define KR_RCV_STACK_BUF   KR_APP(16)  /* key to new address space for stack
					* receive buf */
#define KR_REGS         KR_APP(17)     /* memory mapped device registers */
#define KR_DMA          KR_APP(18)     /* memmapped DMA region */

#define KR_CONSTREAM    KR_APP(19)
#define KR_SCRATCH      KR_APP(20) /* Scratch pad */

/* The key to the client's bank */
#define KR_CLIENT_BANK  KR_ARG(0)

/* The client's space bank key will be stashed in the node allocated
 * from that bank in the following slot */
#define STASH_CLIENT_BANK  1

#endif /* _ENETKEYS_H_ */
