#ifndef __DISK_TAGPOT_H__
#define __DISK_TAGPOT_H__
/*
 * Copyright (C) 2008, Strawberry Development Group.
 *
 * This file is part of the CapROS Operating System.
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

#include <disk/ErosTypes.h>

/* A "Frame" aka object frame is a page-sized area of a range
 * that holds a page or node pot.
 * It is identified by an OID. */
/* A "RangeLoc" is the location of a tag pot or object frame,
 * measured from the beginning of a range. */

#define FramesPerCluster (EROS_PAGE_SIZE / (sizeof(ObCount)+sizeof(uint8_t)))

#define RangeLocsPerCluster (FramesPerCluster + 1)

INLINE frame_t
FrameToCluster(frame_t relFrame)
{
  return relFrame / FramesPerCluster;
}

INLINE unsigned int
FrameIndexInCluster(frame_t relFrame)
{
  return relFrame % FramesPerCluster;
}

INLINE frame_t
FrameToRangeLoc(frame_t relFrame)
{
#if 0	// This calculation makes the most sense:
  return FrameToCluster(relFrame) * RangeLocsPerCluster	// preceding clusters
         + 1	// for the tag pot in this cluster
         + FrameIndexInCluster(relFrame);
#endif
  // This calculation is equivalent and slightly faster.
  return relFrame + FrameToCluster(relFrame) + 1;
}

/* The "relative ID" of a tag pot is the relative OID of the first
 * frame in its cluster. */
INLINE OID
FrameToTagPotRelID(frame_t relFrame)
{
  return FrameToCluster(relFrame) * FramesPerCluster;
}

/* typeAndAllocCountUsed[i] has: */
#define TagAllocCountUsedMask 0x80
#define TagTypeMask           0x7f

// Frame types:
#define FRM_TYPE_ZDPAGE		0 /* zero page/empty frame */
#define FRM_TYPE_DPAGE		1
#define FRM_TYPE_ZNODE		2 /* zero node -- only in ckpt log */
#define FRM_TYPE_NODE		3

typedef struct TagPot {
  ObCount count[FramesPerCluster];
  uint8_t typeAndAllocCountUsed[FramesPerCluster];
} TagPot;

#endif /* __DISK_TAGPOT_H__ */
