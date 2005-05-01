#ifndef __DISK_PAGEPOT_HXX__
#define __DISK_PAGEPOT_HXX__
/*
 * Copyright (C) 1998, 1999, Jonathan S. Shapiro.
 *
 * This file is part of the EROS Operating System.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2,
 * or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include <disk/ErosTypes.h>
#include <disk/DiskFrame.h>

/* Definition of the frame tag/allocation pot page structure. */

#define DATA_PAGES_PER_PAGE_CLUSTER (EROS_PAGE_SIZE / (sizeof(ObCount)+sizeof(uint8_t)))

struct PagePot {
  ObCount count[DATA_PAGES_PER_PAGE_CLUSTER];
  uint8_t    type[DATA_PAGES_PER_PAGE_CLUSTER];
};

typedef struct PagePot PagePot;
#define PAGES_PER_PAGE_CLUSTER (DATA_PAGES_PER_PAGE_CLUSTER + 1)

#endif /* __DISK_PAGEPOT_HXX__ */
