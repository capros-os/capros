#ifndef __SEGWALK_H__
#define __SEGWALK_H__
/*
 * Copyright (C) 1998, 1999, 2001, Jonathan S. Shapiro.
 * Copyright (C) 2007, Strawberry Development Group.
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
/* This material is based upon work supported by the US Defense Advanced
Research Projects Agency under Contract No. W31P4Q-07-C-0070.
Approved for public release, distribution unlimited. */

#if 0
#define SW_ISWRITE     0x1u
#define SW_ISPROMPT    0x2u
#define SW_CANCALL     0x4u
#define SW_CANWRITE    0x8u
#endif
/* #define SW_CANTRAVERSE 0x10u */

struct SegWalk {
  uint32_t frameBits;	/* number of bits in frame offset */
  uva_t vaddr;		/* in case invoking domain keeper */

  Key * pSegKey;
  uint32_t segBlss;
  struct ObjectHeader * segObj;
  bool segObjIsWrapper;
  uint64_t offset;
  Node * redSeg;
  uint64_t redSegOffset;
  uint32_t redSpanBlss;	/* blss of segment spanned by red seg */

  bool writeAccess;
  bool canCall;
  bool canWrite;
  bool canFullFetch;	/* not a sensory path */
  bool canCache;	/* cache disable handling */
  bool invokeKeeperOK;
  bool invokeProcessKeeperOK;

  bool wantLeafNode;

  uint32_t   faultCode;
  uint32_t   traverseCount;
};
  
void segwalk_init(SegWalk *wi /*@ not null @*/, Key *pSegKey);

bool
WalkSeg(SegWalk* wi /*@ NOT NULL @*/, uint32_t stopBlss,
	void * pPTE, int mapLevel);

#endif /* __SEGWALK_H__ */
