#ifndef __ADDRSPACE_H__
#define __ADDRSPACE_H__

/*
 * Copyright (C) 2003, Jonathan S. Shapiro.
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

/* Convenience routine for buying a new node for use in expanding the
   address space. */
uint32_t addrspace_new_space(uint32_t kr_bank, uint16_t lss, uint32_t kr_new);

/* Make room in this domain's address space for mapping subspaces
   corresponding to client windows */
uint32_t addrspace_prep_for_mapping(uint32_t kr_self, uint32_t kr_bank,
				    uint32_t kr_tmp, uint32_t kr_new_node);

/* Insert a local window key (lwk) referencing the segment at
   'base_slot' into 'lwk_slot' */
uint32_t addrspace_insert_lwk(cap_t node, uint32_t base_slot,
			      uint32_t lwk_slot, uint16_t lss_of_base);

/* Future: routines to actually insert subspaces and return virtual
   address */

#endif /* __ADDRSPACE_H__ */
