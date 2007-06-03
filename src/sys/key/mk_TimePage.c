/*
 * Copyright (C) 1998, 1999, Jonathan S. Shapiro.
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

#include <kerninc/kernel.h>
#include <kerninc/Key.h>
#include <kerninc/Activity.h>
#include <kerninc/Invocation.h>
#include <kerninc/Machine.h>

#include <eros/Invoke.h>
#include <eros/StdKeyType.h>

#ifdef KKT_TimePage

void
TimePageKey(Invocation& inv)
{
  COMMIT_POINT();
      
  /* We honor as much of the page key protocol as we are able. */

  switch(inv.entry.code) {
  case OC_Page_MakeReadOnly:
    inv.SetExitKey(0, *inv.key);
    assert (inv.exit.pKey[0]->IsReadOnly());

    inv.exit.code = RC_OK;
    return;

  case OC_eros_Page_zero:		/* zero page */
  case OC_eros_Page_clone:
    inv.exit.code = RC_eros_key_NoAccess;
    return;
    
  case OC_eros_key_getType:
    inv.exit.code = RC_OK;
    inv.exit.w1 = AKT_TimePage;
    return;

  case OC_Page_LssAndPerms:
    {
      COMMIT_POINT();

      inv.exit.code = RC_OK;
      inv.exit.w1 = inv.key->keyData;
      inv.exit.w2 = inv.key->keyPerms;
      return;
    }

  default:
    inv.exit.code = RC_eros_key_UnknownRequest;
    return;
  }
}

#endif /* KKT_TimePage */
