/*
 * Copyright (C) 1998, 1999, 2001, Jonathan S. Shapiro.
 * Copyright (C) 2005, 2006, 2007, 2008, Strawberry Development Group.
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

#include <string.h>
#include <kerninc/kernel.h>
#include <kerninc/Key.h>
#include <kerninc/Activity.h>
#include <kerninc/Invocation.h>
#include <kerninc/Node.h>
#include <eros/Invoke.h>
#include <eros/StdKeyType.h>
#include <kerninc/Key-inline.h>

#include <idl/capros/key.h>
#include <idl/capros/KeyBits.h>

/* WHAT KeyBits_Translate RETURNS:
 * 
 * There are two design issues:
 *   1. What should /KeyBits_Translate/ return for invalid keys
 *   2. Should /KeyBits_Translate/ expose the allocation/call count?
 *   3. Should /KeyBits_Translate/ return this information in some
 *      well-canonicalized form?
 * 
 * For invalid keys, we have chosen to have /KeyBits_Translate/ return
 * a void key.  This is accomplished by first *preparing* the
 * key we are to analyze (which renders it into canonical form) and
 * then hand-depreparing it into a KeyBits structure.
 * 
 * As to the allocation count, here we run into a question.  At the
 * moment, the only object that uses KeyBits is KID, and this does not
 * really require the allocation/call count.  For the moment, I am
 * exposing them, though I strongly suspect this is the wrong thing to
 * do.  The argument in favor is that watching the counts roll over is
 * a help to debuggers...
 * 
 * I am also extending KeyBits to return information concerning
 * whether the key in question is in fact valid.  In the future, if we
 * elect to let stale keys retain their information for the sake of
 * debuggers, this may prove useful.
 */

/* May Yield. */
void
KeyBitsKey(Invocation* inv /*@ not null @*/)
{
  inv_GetReturnee(inv);

  switch(inv->entry.code) {
  case OC_capros_key_getType:
    COMMIT_POINT();
    inv->exit.code = RC_OK;
    inv->exit.w1 = AKT_KeyBits;
    break;

  case OC_capros_KeyBits_get:
    {
      struct capros_KeyBits_info kbi;
      KeyBits dupKey;

      proc_SetupExitString(inv->invokee, inv, sizeof(kbi));

      /* This is dirty - we need to arrive at the *unprepared* version
       * of the key, but without creaming the actual key:
       */

      kbi.version = capros_KeyBits_VERSION;
      kbi.valid = 1;

      Key * k = inv->entry.key[0];
      key_Prepare(k);
      
      COMMIT_POINT();
      
      key_MakeUnpreparedCopy(&dupKey, k);
      /* Note that we do NOT expose hazard bits or prepared bits! */

      memcpy(&kbi.w, &dupKey, sizeof(kbi.w));

      inv_CopyOut(inv, sizeof(kbi), &kbi);

      inv->exit.code = RC_OK;
      break;
    }
  default:
    COMMIT_POINT();
    inv->exit.code = RC_capros_key_UnknownRequest;
    break;
  }

  ReturnMessage(inv);
}
