#ifndef __DISKKEY_H__
#define __DISKKEY_H__
/*
 * Copyright (C) 1998, 1999, 2001, 2002, Jonathan S. Shapiro.
 * Copyright (C) 2007, 2008, Strawberry Development Group.
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

/* DiskKey.h: Declaration of a the Disk format of a Key.
 * 
 * A DiskKey is the form that a Key takes when it appears on disk.
 * DiskKeys live in DiskNodes in the same way that Keys live in Nodes.
 * 
 * DiskKeys are VERY simple objects.  They are just smart enough to
 * convert themselves to/from ordinary Keys, and to assign from each
 * other.
 */


#include <eros/target.h>
#include <disk/KeyStruct.h>
#include <disk/Key-inline.h>
#include <disk/DiskNode.h>

#ifdef __cplusplus
extern "C" {
#endif

void init_DataPageKey(KeyBits *, OID, bool readOnly);
void init_StartKey(KeyBits *, OID, uint16_t keyData);
void init_ResumeKey(KeyBits *, OID);
void init_ProcessKey(KeyBits *, OID);
void init_NodeKey(KeyBits *, OID, uint16_t keyData);
void init_RangeKey(KeyBits *, OID oidlo, OID oidhi);
void init_NumberKey(KeyBits *, uint32_t first, 
		    uint32_t second, uint32_t third);
void init_SchedKey(KeyBits *, uint16_t prio);
void init_MiscKey(KeyBits *, uint16_t ty, uint32_t n0);

#ifdef __cplusplus
}
#endif

INLINE
void init_SmallNumberKey(KeyBits *dk, uint32_t first)
{
  init_NumberKey(dk, first, 0, 0);
}

INLINE void
init_DiskNodeKeys(DiskNode * dn)
{
  unsigned u;

  for (u = 0; u < EROS_NODE_SIZE; u++)
    keyBits_InitToVoid(&dn->slot[u]);
}

#endif /* __DISKKEY_H__ */
