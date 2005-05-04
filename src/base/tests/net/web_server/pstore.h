/*
 * Copyright (C) 2002, Jonathan S. Shapiro.
 *
 * This file is part of the EROS Operating System distribution.
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
#ifndef _PSTORE_H_
#define _PSTORE_H_

#define PSTORE_POOL_SIZE      200
#define PSTORE_POOL_BUFSIZE   9100

#define PSTORE_UNUSED     0x0u /* buffer used up*/
#define PSTORE_USED       0x1u /* buffer unused */
#define PSTORE_READY      0x4u /* buffer ready for action */

struct pstore {
  int8_t  nextsector; /* The next pstores sector */
  int32_t nextoffset; /* The next pstores offset */
  int8_t  sector;     /* Payload starts from here */
  int32_t offset;     /* Pointer = Sector:Offset */
  uint16_t tot_len;   /* Total length of buffer+additionally chained buffers */
  uint16_t len;       /* Length of this buffer. */
  uint16_t flags;     /* Flags telling the type of pstore */
  uint8_t  status;    /* Flags telling if buffer is used up */
  int      ref;       /* The ref count always equals the number of pointer
		       * into this buffer. This can be pointers from an
		       * application, the stack itself or pstore->next
		       * pointers from a chain */
};

uint32_t pstore_fast_free(sector sect);
uint32_t read_data_from(void *d, sector sect);
struct pstore* copy_data_in(void *s, uint32_t size, sector sect);

#endif /*_PSTORE_H_*/
