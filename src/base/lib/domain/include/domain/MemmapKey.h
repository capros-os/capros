#ifndef __MEMMAP_KEY_H__
#define __MEMMAP_KEY_H__

/*
 * Copyright (C) 2003 Jonathan S. Shapiro.
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

#define OC_Memmap_Map            1

/* Inform the memory mapper that we need a mapped address space large
   enough to hold 'size' bytes.  The address 'base_addr' will be
   mapped to the start of this new space.  Client must provide a
   physmem range key in order for the memory mapper to obtain the
   correct page keys. Pass back the required lss for this new space so
   the client doesn't have to compute that. The client must use the
   memmap_key as the wrapper key to this new mapped space. */
uint32_t memmap_map(uint32_t memmap_key, uint32_t physrange_key,
		    uint64_t base_addr, uint64_t size,
		    /* out */ uint16_t *lss);

#endif /* __MEMMAP_KEY_H__ */
