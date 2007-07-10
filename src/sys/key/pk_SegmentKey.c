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

#include <kerninc/kernel.h>
#include <kerninc/Key.h>
#include <kerninc/Node.h>
#include <kerninc/Process.h>
#include <kerninc/Invocation.h>
#include <kerninc/Activity.h>
#include <eros/Invoke.h>
#include <eros/StdKeyType.h>
#include <eros/SegmentKey.h>

#include <idl/capros/key.h>

void
SegmentKey(Invocation* inv /*@ not null @*/)
{
  /* FIX: what should KT return from a red seg sans keeper? */
    
  COMMIT_POINT();

  inv->exit.code = RC_OK;

  switch (inv->entry.code) {
  case OC_Seg_MakeSpaceKey: 
    {
      uint8_t p = inv->entry.w1; /* the permissions */

      inv_SetExitKey(inv, 0, inv->key);

      if (inv->exit.pKey[0])
	inv->exit.pKey[0]->keyPerms |= p;
      return;
    }
  case OC_capros_key_getType:
    {
      inv->exit.code = RC_OK;
      inv->exit.w1 = AKT_Segment;

      return;
    }
  default:
    inv->exit.code = RC_capros_key_UnknownRequest;
    return;
  }
}
