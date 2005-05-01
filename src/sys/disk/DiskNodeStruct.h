#ifndef __DISK_DISKNODESTRUCT_HXX__
#define __DISK_DISKNODESTRUCT_HXX__
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

/* This structure defines the *layout* of the disk node structure so
 * that various elements of the kernel can fetch things from ROM and
 * RAM nodes. 
 *
 * For the moment, the kernel still makes the assumption that nodes on
 * disk are gathered into node pots -- even in ROM and RAM
 * images. This should perhaps be reconsidered, so as to isolate the
 * kernel from unnecessary knowledge.
 */

#include "KeyStruct.h"

typedef struct DiskNodeStruct DiskNodeStruct;
struct DiskNodeStruct {
  ObCount allocCount;
  ObCount callCount;
  OID oid;

  KeyBits slot[EROS_NODE_SIZE];
} ;

#define DISK_NODES_PER_PAGE (EROS_PAGE_SIZE / sizeof(DiskNodeStruct))

#endif /* __DISK_DISKNODESTRUCT_HXX__ */
