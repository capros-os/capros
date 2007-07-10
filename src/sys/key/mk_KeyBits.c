/*
 * Copyright (C) 1998, 1999, 2001, Jonathan S. Shapiro.
 * Copyright (C) 2005, 2006, 2007, Strawberry Development Group.
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

#include <string.h>
#include <kerninc/kernel.h>
#include <kerninc/Key.h>
#include <kerninc/Activity.h>
#include <kerninc/Invocation.h>
#include <eros/Invoke.h>
#include <eros/StdKeyType.h>

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
  switch(inv->entry.code) {
  case OC_capros_key_getType:
    COMMIT_POINT();
    inv->exit.code = RC_OK;
    inv->exit.w1 = AKT_KeyBits;
    return;

  case OC_capros_KeyBits_get:
    {
      uint8_t kt;
      struct capros_KeyBits_info kbi;
      KeyBits dupKey;

      proc_SetupExitString(inv->invokee, inv, sizeof(kbi));

      /* This is dirty - we need to arrive at the *unprepared* version
       * of the key, but without creaming the actual key:
       */

      kbi.version = capros_KeyBits_VERSION;
      kbi.valid = 1;

      key_Prepare(inv->entry.key[0]);
      
      COMMIT_POINT();
      
      kt = keyBits_GetType(inv->entry.key[0]);
      
      /* Note that we do NOT expose hazard bits or prepared bits! */
      keyBits_InitType(&dupKey, kt);
      keyBits_SetUnprepared(&dupKey);
      dupKey.keyFlags = 0;
      dupKey.keyPerms = inv->entry.key[0]->keyPerms;
      dupKey.keyData = inv->entry.key[0]->keyData;

      if ( keyBits_IsObjectKey(inv->entry.key[0]) ) {
	ObjectHeader *pObj = inv->entry.key[0]->u.ok.pObj;

	if ( keyBits_IsGateKey(inv->entry.key[0]) )
	  pObj = DOWNCAST(inv->entry.key[0]->u.gk.pContext->procRoot, ObjectHeader);

	if ( keyBits_IsType(inv->entry.key[0], KKT_Resume) )
	  dupKey.u.unprep.count = objH_ToNode(pObj)->callCount;
	else
	  dupKey.u.unprep.count = pObj->allocCount;

	dupKey.u.unprep.oid = pObj->oid;
      }
      else {
	dupKey.u.nk.value[0] = inv->entry.key[0]->u.nk.value[0];
	dupKey.u.nk.value[1] = inv->entry.key[0]->u.nk.value[1];
	dupKey.u.nk.value[2] = inv->entry.key[0]->u.nk.value[2];
      }


      memcpy(&kbi.w, &dupKey, sizeof(kbi.w));

      inv_CopyOut(inv, sizeof(kbi), &kbi);

      inv->exit.code = RC_OK;
      return;
    }
  default:
    COMMIT_POINT();
    inv->exit.code = RC_capros_key_UnknownRequest;
    return;
  }
}
