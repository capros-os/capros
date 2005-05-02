#ifndef __CONSTRUCTOR_H__
#define __CONSTRUCTOR_H__

/*
 * Copyright (C) 1998, 1999, 2001, Jonathan S. Shapiro.
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

#define OC_Constructor_IsDiscreet             1
#define OC_Constructor_Request                2
#define OC_Constructor_Seal                   3
#define OC_Constructor_Insert_Constituent     4

#define OC_Constructor_Insert_Keeper          5
#define OC_Constructor_Insert_AddrSpace       6
#define OC_Constructor_Insert_Symtab          7
#define OC_Constructor_Insert_PC              8

/* Also add #defines for the error return values... */
#define RC_Constructor_Indiscreet             1
#define RC_Constructor_NotSealed              2
#define RC_Constructor_NotBuilder             3

#ifndef __ASSEMBLER__
uint32_t constructor_request(uint32_t krConstructor, uint32_t krBank,
			     uint32_t krSched, uint32_t krArg0,
			     uint32_t krProduct /* OUT */);
uint32_t constructor_is_discreet(uint32_t krConstructor, uint32_t *isDiscreet);
uint32_t constructor_seal(uint32_t krConstructor, uint32_t krRequestor);
uint32_t constructor_insert_constituent(uint32_t krConstructor, uint32_t ndx,
					uint32_t krConstit);

uint32_t constructor_insert_keeper(uint32_t krConstructor, 
				   uint32_t krKeeper);
uint32_t constructor_insert_addrspace(uint32_t krConstructor, 
				      uint32_t krAddrSpace);
uint32_t constructor_insert_symtab(uint32_t krConstructor, 
				   uint32_t krSymtab);
uint32_t constructor_insert_pc(uint32_t krConstructor, 
			       uint32_t krPC);
#endif

#endif /* __CONSTRUCTOR_H__ */

