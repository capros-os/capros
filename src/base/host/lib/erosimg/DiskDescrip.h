#ifndef __DISKDESCRIP_H__
#define __DISKDESCRIP_H__

/*
 * Copyright (C) 1998, 1999, 2002, Jonathan S. Shapiro.
 * Copyright (C) 2008, Strawberry Development Group.
 *
 * This file is part of the CapROS Operating System,
 * and is derived from the EROS Operating System.
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
/* This material is based upon work supported by the US Defense Advanced
Research Projects Agency under Contract No. W31P4Q-07-C-0070.
Approved for public release, distribution unlimited. */

#include <eros/target.h>

typedef struct DiskDescrip DiskDescrip;
struct DiskDescrip {
  const char *diskName;
  unsigned long secsize;
  uint32_t heads;
  uint32_t secs;
  uint32_t cyls;
  const char* imageName;
} ;

#ifdef __cplusplus
extern "C" {
#endif

INLINE uint32_t 
dd_TotSectors(DiskDescrip *dd) 
{ 
  return dd->secs * dd->heads * dd->cyls;
}
    
const DiskDescrip* dd_Lookup(const char* type);
const DiskDescrip* dd_GetDefault();

#ifdef __cplusplus
}
#endif

#endif /* __DISKDESCRIP_H__ */
