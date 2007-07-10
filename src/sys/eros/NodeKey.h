#ifndef __NODEKEY_H__
#define __NODEKEY_H__

/*
 * Copyright (C) 1998, 1999, Jonathan S. Shapiro.
 * Copyright (C) 2006, Strawberry Development Group.
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

#ifndef __STDKEYTYPE_H__
#include <eros/StdKeyType.h>
#endif

/*
 * This file resides in eros/ because the kernel and the invocation
 * library various must agree on the values.
 */

/* Local Variables: */
/* comment-column:34 */
/* End: */

/* ORDER and RESULT code values: */

#define OC_Node_Copy             0
#define OC_Node_Swap             1
#define OC_Node_Extended_Copy    2
#define OC_Node_Extended_Swap    3
#define OC_Node_LssAndPerms      4 /* must match OC_Page_LssAndPerms */
#define OC_Node_WriteNumber      96

#define OC_Node_MakeNodeKey      64
#define OC_Node_MakeSegmentKey   65
#define OC_Node_MakeWrapperKey   66

#define OC_Node_CompareKey       72
#define OC_Node_Clear	         73
#if 0
#define OC_Node_WriteNumbers     75
#endif
#define OC_Node_Clone		 80


#define RC_Node_Range		 1

#ifndef __ASSEMBLER__
struct capros_Number_value;

uint32_t node_copy(uint32_t krNode, uint32_t slot, uint32_t krTo);
uint32_t node_swap(uint32_t krNode, uint32_t slot, uint32_t krFrom, 
		   uint32_t krTo);
uint32_t node_extended_copy(uint32_t krNode, uint32_t slot, uint32_t krTo);
uint32_t node_extended_swap(uint32_t krNode, uint32_t slot, uint32_t krFrom, 
			    uint32_t krTo);
uint32_t node_clone(uint32_t krNode, uint32_t krFrom);
uint32_t node_write_number(uint32_t krNode, uint32_t slot, const struct capros_Number_value *);
uint32_t node_make_node_key(uint32_t krNode, uint16_t keyData, 
			    uint8_t perms, uint32_t krTo);
uint32_t node_make_wrapper_key(uint32_t krNode, uint16_t keyData,
			       uint8_t perms, uint32_t krTo);
uint32_t node_make_segment_key(uint32_t krNode, uint16_t keyData,
			       uint8_t perms, uint32_t krTo);
uint32_t get_lss_and_perms(uint32_t krNode, uint16_t *lss,
			       uint8_t *perms);
uint32_t node_wake_some(uint32_t krNode, uint32_t andBits, 
			uint32_t orBits, uint32_t match);
uint32_t node_wake_some_no_retry(uint32_t krNode, uint32_t andBits, 
				 uint32_t orBits, uint32_t match);
#endif /* __ASSEMBLER__ */

#endif /* __NODEKEY_H__ */
