#ifndef __FRAMEINFO_HXX__
#define __FRAMEINFO_HXX__
/*
 * Copyright (C) 1998, 1999, 2001, Jonathan S. Shapiro.
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

struct FrameInfo {
  CoreDivision *cd;
  OID oid;
  OID obFrameOid;
  uint32_t obFrameNdx;
  uint32_t relObFrame;
  uint32_t clusterNo;
  OID tagFrameOid;
  uint32_t relTagFrame;
  uint32_t tagEntry;

  FrameInfo(OID oid);
} ;

#endif /* __FRAMEINFO_HXX__ */
