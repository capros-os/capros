/*
 * Copyright (C) 1998, 1999, 2001, Jonathan S. Shapiro.
 * Copyright (C) 2005, 2007, 2008, 2009, Strawberry Development Group.
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

#include <assert.h>
#include <disk/Key-inline.h>
#include <disk/DiskNode.h>

void
RelocateKey(KeyBits *key, OID nodeBase, OID pageBase,
	    uint32_t nPages)
{
  if ( keyBits_IsType(key, KKT_Page) ) {
    OID oldOid = get_target_oid(&key->u.unprep.oid);
    if (oldOid < OID_RESERVED_PHYSRANGE) {
      OID oid = pageBase + (oldOid * EROS_OBJECTS_PER_FRAME);

      if (keyBits_IsPrepared(key)) {
        keyBits_SetUnprepared(key);
        oid += (nPages * EROS_OBJECTS_PER_FRAME);
      }

      put_target_oid(&key->u.unprep.oid, oid);
    }
    // else oid is for a phys page, don't change it
  }
  else if (keyBits_IsNodeKeyType(key)) {
    OID oid = get_target_oid(&key->u.unprep.oid);
    OID frame = oid / DISK_NODES_PER_PAGE;
    OID offset = oid % DISK_NODES_PER_PAGE;
    oid = nodeBase + FrameObIndexToOID(frame, offset);

    put_target_oid(&key->u.unprep.oid, oid);
  }
}
