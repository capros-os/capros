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


/*
 * directory.h
 *
 * order/error codes for the simple directory object
 */


#define OC_Directory_Link      1
#define OC_Directory_Unlink    2
#define OC_Directory_Lookup    3

#define RC_Directory_Exists    1
#define RC_Directory_NotFound  2
#define RC_Directory_NoSpace   3

#if 0
#define OC_Directory_Get    1
#define OC_Directory_Set    2
#define OC_Directory_Clear  3

#define DI_CODE_ERR         1
#define DI_MULT_ENTRIES_ERR 2
#define DI_NO_MATCH_ERR     3
#define DI_MANY_MATCHES_ERR 4
#define DI_EXISTS_ERR       5
#define DI_NO_SPACE_ERR     6
#define DI_ERR              7
#define DI_InitError        8
#endif

#ifndef __ASSEMBLER__
uint32_t directory_lookup(uint32_t krDir, const uint8_t *str, uint32_t strlen, uint32_t krOut);
uint32_t directory_link(uint32_t krDir, const uint8_t *str, uint32_t strlen, uint32_t krIn);
#endif
