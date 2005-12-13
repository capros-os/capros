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

#include <kerninc/kernel.h>
#include <kerninc/Key.h>
#include <kerninc/Activity.h>
#include <kerninc/Invocation.h>
#include <kerninc/Machine.h>

#include <eros/PageKey.h>
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

  case OC_Page_Zero:		/* zero page */
  case OC_Page_Clone:
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
