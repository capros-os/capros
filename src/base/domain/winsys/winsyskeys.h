/*
 * Copyright (C) 2003, Jonathan S. Shapiro.
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

/*  This code implements the "window system" domain.  This domain
    manages graphical windows and dispatches user input events to
    window clients.  It is served by the Display Manager domain.
*/

#include <domain/Runtime.h>

#define KR_OSTREAM       KR_APP(0) /* For debugging output via kprintf*/

/* This domain's runtime keys: */

/* Start keys that get wrapped */
#define KR_SESSION_TYPE         KR_APP(3) /* start key for session
					     interface */
#define KR_TRUSTED_SESSION_TYPE KR_APP(4) /* start key for trusted
					     session interface */

/* Keys that this domain hands out */
#define KR_START           KR_APP(5) /* generic start key for this domain */
#define KR_SESSION_CREATOR KR_APP(6) /* start key for session creator
					interface */
#define KR_TRUSTED_SESSION_CREATOR KR_APP(7) /* start key for trusted
					        session creator interface */
#define KR_NEW_SESSION          KR_APP(8) /* new unique session key
					     (for clients) */

/* Key registers for temporary storage: */
#define KR_NEW_NODE      KR_APP(9) /* temp reg for a new node */
#define KR_SCRATCH       KR_APP(10) /* used for temporary key storage
				       */
/* Key to a forwarder upon which clients can be redirected if this
   domain can't service them promptly */
#define KR_PARK_NODE     KR_APP(11) /* key to a forwarder to which
				       retrying clients will be
				       redirected */
#define KR_PARK_WRAP     KR_APP(12)

#define KR_PKEEPER       KR_APP(13) /* key to process keeper */

#define KR_ZSC           KR_APP(14) /* key to Zero Space Constructor
				       */
#define KR_NEW_WINDOW    KR_APP(15) /* key to new address space for
				       client window content */

/* Key registers for video driver */
#define KR_DEVPRIVS      KR_APP(16)
#define KR_PHYSRANGE     KR_APP(17)
#define KR_FRAMEBUF      KR_APP(18)
#define KR_FIFO          KR_APP(19)
#define KR_MEMMAP        KR_APP(20)

/* Support for cut/copy/paste authorities */
#define KR_PASTE_CONTENT   KR_APP(1)
#define KR_PASTE_CONVERTER KR_APP(2)

/* Client must provide space bank key for some operations */
#define KR_CLIENT_BANK   KR_ARG(0)

/* The client's space bank key will be stashed in the forwarder allocated
   from that bank in the following slot */
#define STASH_CLIENT_BANK  0

