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

/** This table maps ring addresses to virtual addresses 
 */

#ifndef __MAPPING_TABLE_H__
#define __MAPPING_TABLE_H__

typedef uint8_t sector;

#define XMIT_CLIENT_SPACE     0
#define RECV_CLIENT_SPACE     2

struct mapping_table {
  sector sector;
  uint32_t start_address;
  uint32_t size;
  int cur_p;           /* The current pointer in the ring buffer */
};

#endif /*__MAPPING_TABLE_H__*/
