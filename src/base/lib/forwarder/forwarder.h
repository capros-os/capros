#ifndef __FORWARDER_H__
#define __FORWARDER_H__

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

/* This routine make it easy to fabricate forwarders. */

#include <idl/capros/Forwarder.h>	// to define capros_Forwarder_sendCap etc.

/*
  forwarder_create

  Input:
    bank          - register containing key to space bank
                    (from which to buy the forwarder)
    opaque_key    - register to store the newly created opaque forwarder key
                    (the one that forwards to the target key)
    nonopaque_key - register to store the non-opaque forwarder key
                    (needed for later calls to "block" and "unblock")
    target_key    - register containing key to wrap (a start key)
    flags         - flags for the opaque key. Any of:
                      capros_Forwarder_sendCap
                      capros_Forwarder_sendWord
    value         - 32-bit value to transmit
                    (if flag capros_Forwarder_sendWord is used)

  Returns:  RC_OK or an error.
*/
uint32_t forwarder_create(uint32_t bank, uint32_t opaque_key, 
		          uint32_t nonopaque_key, uint32_t target_key, 
		          uint32_t flags, uint32_t value);

#endif
