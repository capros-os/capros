#ifndef __WRAPPER_H__
#define __WRAPPER_H__

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

/* These routines make it easy to fabricate and modify wrapper nodes. */

/*
  wrapper_create

  Purpose: Fabricate a wrapper node and return the "wrapper key".  

  Input:
    bank  -  register containing key to space bank (from which to buy node)
    wrapper_key - register to store the newly created "wrapper key"
    node_key    - register to store the key to the "raw node" (needed for
                  later calls to "block" and "unblock")
    key_to_wrap - register containing key to wrap in the new node (usually
                  a start key)
    flags       - flags for the wrapper key (eg. WRAPPER_SEND_WORD, etc.)
    value1      - 1st 32-bit value to store in the wrapper node's number key
    value2      - 2nd 32-bit value to store in the wrapper node's number key

  Returns:  RC_OK or RC_RequestError
*/
uint32_t wrapper_create(uint32_t bank, uint32_t wrapper_key, 
			uint32_t node_key, uint32_t key_to_wrap, 
			uint32_t flags, uint32_t value1, uint32_t value2);

/*
  wrapper_modify

  Purpose:  Modify the bit flags of a wrapper node (eg. to block or unblock)

  Input:
    node_key - key to the "raw node" (*NOT* the wrapper key)
    flags    - new flags for the wrapper key:
               To block clients: set to WRAPPER_BLOCKED
               To unblock clients:  set to any combination that does *not*
               include WRAPPER_BLOCKED (eg. WRAPPER_SEND_WORD)
    value1   - first 32-bit value to store in the wrapper node's number key
    value2   - second 32-bit value to store in the wrapper node's number key

  Returns:  RC_OK or RC_RequestError
*/
uint32_t wrapper_modify(uint32_t node_key, uint32_t flags, uint32_t value1,
			uint32_t value2);

#endif
