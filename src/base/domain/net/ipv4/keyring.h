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

#ifndef _KEYRING_H_
#define _KEYRING_H_

/* Since the whole network stack is a monolithic process, including this
 * file saves us the work of passing around the key slots while calling 
 * functions */

#include <domain/Runtime.h>


#define KR_OSTREAM      KR_APP(0)  /* verbose output */

#define KR_PARK_NODE    KR_APP(1)  /* key to wrapper node to which retrying */
#define KR_PARK_WRAP    KR_APP(2)  /* clients will be redirected onto this */

#define KR_DEVPRIVS     KR_APP(3)  /* talk to devices, make i/o calls */
#define KR_PHYSRANGE    KR_APP(4)  /* key to ranges */

#define KR_MEMMAP_C     KR_APP(5)  /* key to memmap domain */

#define KR_PCI_PROBE_C  KR_APP(6)  /* key to the pci_probe domain */

#define KR_HELPER       KR_APP(7)  /* key to lance helper (looks for IRQ 5) */

#define KR_ADDRSPC      KR_APP(8) /* addrspace key */
#define KR_SLEEP        KR_APP(9) /* Sleep key */

/* Key to session creator interface*/
#define KR_SESSION_CREATOR  KR_APP(10)

/* Key to every session, clients get a wrapper key to this */
#define KR_SESSION_TYPE     KR_APP(11)

#define KR_TIMEOUT_AGENT_TYPE KR_APP(12) /* start key to the timeout agent */

#define KR_NEW_SESSION  KR_APP(13) /* new unique session key (for clients) */
#define KR_NEW_NODE     KR_APP(14) /* temp reg. for a new node */     

#define KR_HELPER_TYPE  KR_APP(15) /* Start key to helper */
#define KR_START        KR_APP(16) /* Start key */

/* our Alarm timer constructor and start key */
#define KR_TIMEOUT_AGENT        KR_APP(17)

/* Console stream key */
#define KR_CONSTREAM            KR_APP(18)

#define KR_REGS         KR_APP(19)
/* The key to the client's bank */
#define KR_CLIENT_BANK  KR_ARG(0)

#define KR_SCRATCH      KR_APP(20) /* Scratch pad */


/* The client's space bank key will be stashed in the node allocated
 * from that bank in the following slot */
#define STASH_CLIENT_BANK  21

#endif /* _KEYRING_H_ */
