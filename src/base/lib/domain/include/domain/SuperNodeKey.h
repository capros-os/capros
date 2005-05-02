#ifndef __SUPERNODEKEY_H__
#define __SUPERNODEKEY_H__

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

#define OC_SuperNode_Copy		64
#define OC_SuperNode_Swap		65

/* #define OC_SuperNode_MakeNodeKey	32 */
#define OC_SuperNode_MakeFetchKey	33

#define OC_SuperNode_CompareKey      48
#define OC_SuperNode_Zero	     49
#define OC_SuperNode_Datauint8_t        50

#ifndef __ASSEMBLER__
uint32_t supernode_copy(uint32_t krSnode, uint32_t ndx, uint32_t krOut);
uint32_t supernode_swap(uint32_t krSnode, uint32_t ndx, uint32_t krIn, uint32_t krOut);
uint32_t supernode_zero(uint32_t krSnode);
uint32_t supernode_make_fetch_key(uint32_t krSnode, uint32_t krOut);
#endif

#endif /* __SUPERNODEKEY_H__ */
