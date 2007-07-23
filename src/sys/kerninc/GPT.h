#ifndef __GPT_H__
#define __GPT_H__
/*
 * Copyright (C) 2007, Strawberry Development Group.
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

#include "Node.h"
#include <disk/DiskNodeStruct.h>

typedef Node GPT;

INLINE unsigned int
gpt_GetL2vField(GPT * gpt)
{
  return * gpt_l2vField(& gpt->nodeData);
}

INLINE void
gpt_SetL2vField(GPT * gpt, unsigned int f)
{
  * gpt_l2vField(& gpt->nodeData) = f;
}

struct SegWalk {
  struct ObjectHeader * memObj;
  uint64_t offset;

  bool needWrite;	/* Whether we need write permission.
			This does not change as we traverse. */

  uint8_t restrictions;	/* Cumulative restrictions on the path so far.
			capros_Memory_readOnly is correctly tracked.
			capros_Memory_noCall is valid only if 
			keeperGPT != SEGWALK_GPT_UNKNOWN.
			Weak and opaque are not used. */

  /* backgroundGPT and keeperGPT are 0 if there is no such GPT,
  SEGWALK_GPT_UNKNOWN if the key is unknown. */
#define SEGWALK_GPT_UNKNOWN ((GPT *)1)
  GPT * backgroundGPT;	/* the last GPT with a background key
			that we traversed. */
  GPT * keeperGPT;	/* the last GPT with a keeper that we traversed. */
  uint64_t keeperOffset;  // valid if keeperGPT !=0 and !=SEGWALK_GPT_UNKNOWN

  uint32_t   traverseCount; /* The number of traversals we have done on this
               path.  This is not properly tracked if we
	       use the short-circuited fast walk. */

  uint32_t   faultCode;
};
  
bool segwalk_init(SegWalk * wi, Key * pSegKey, uint64_t va,
             void * pPTE, int mapLevel);

enum WalkSegRet {
  WalkSeg_OK,		// successful
  WalkSeg_Fault,	// faultCode set
  WalkSeg_NeedBG	// need background key
};
enum WalkSegRet
WalkSeg(SegWalk * wi, uint32_t stopL2v,
	void * pPTE, int mapLevel);

#endif /* __GPT_H__ */
