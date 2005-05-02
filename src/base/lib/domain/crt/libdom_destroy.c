/*
 * Copyright (C) 1998, 1999, Jonathan S. Shapiro.
 *
 * This file is part of the EROS Operating System runtime library.
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

#include <eros/target.h>
#include <eros/Invoke.h>

/* Setup and teardown for destroying standard domains */

void
__domain_init()
{
  /* Algorithm:

     1. Generate all of the factory products.
     2. Buy a node to wrap the real segment.
     3. Return excess storage.
     4. Switch to actual domain program code and run it.

     Buy a node to wrap the real segment and wrap it.

     */
     
  /* buy a node as the new segment, and stash the domain creator
     and destroy segments within it.  If we cannot buy the node,
     return a suitable result to the caller.  An issue here is that if
     the buy fails we may end up leaving factory products
     undestroyed.  I now understand why in keykos the factory product
     construction was done by the domain itself.
     */
}

void
__domain_destroy()
{
  Message msg;

  msg.snd_key0 = KR_VOID;
  msg.snd_key1 = KR_VOID;
  msg.snd_key2 = KR_VOID;
  msg.snd_rsmkey = KR_VOID;
  msg.snd_len = 0;

  msg.rcv_key0 = 12;
  msg.rcv_key1 = 13;
  msg.rcv_key2 = 14;
  msg.rcv_rsmkey = 15;
  msg.rcv_limit = 0;		/* no data returned */

  msg.snd_code = 1;
  msg.snd_invKey = KR_VOID;
  (void) RETURN(&msg);
}

