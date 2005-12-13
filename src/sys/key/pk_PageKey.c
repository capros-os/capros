/*
 * Copyright (C) 1998, 1999, Jonathan S. Shapiro.
 * Copyright (C) 2005, Strawberry Development Group.
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

#include <string.h>
#include <kerninc/kernel.h>
#include <kerninc/Key.h>
#include <kerninc/Activity.h>
#include <kerninc/Invocation.h>
#include <kerninc/ObjectCache.h>
#include <eros/Invoke.h>
#include <eros/StdKeyType.h>
#include <eros/PageKey.h>

#include <idl/eros/key.h>

void
PageKey(Invocation* inv /*@ not null @*/)
{
  kva_t invokedPage;
  kva_t copiedPage;
  kva_t pageAddress =
    objC_ObHdrToPage(key_GetObjectPtr(inv->key));


  /* First handle the Read and Write order codes */

  /* There are too many page write OC's to put them all in the switch
   * statment. Consistency, remember, is the hobgoblin of little minds.
   */

  switch(inv->entry.code) {
  case OC_eros_key_getType:
    COMMIT_POINT();

    inv->exit.code = RC_OK;
    inv->exit.w1 = AKT_Page;
    return;

  case OC_Page_LssAndPerms:
    {
      COMMIT_POINT();

      inv->exit.code = RC_OK;
      inv->exit.w1 = inv->key->keyData;
      inv->exit.w2 = inv->key->keyPerms;
      return;
    }

  case OC_Page_MakeReadOnly:	/* Make RO page key */
    COMMIT_POINT();

    /* No problem with overwriting original key, since in that event
     * we would be overwriting it with itself anyway.
     */


    inv_SetExitKey(inv, 0, inv->key);

    if (inv->exit.pKey[0])
      keyBits_SetReadOnly(inv->exit.pKey[0]);
    
    inv->exit.code = RC_OK;
    return;

  case OC_Page_Zero:		/* zero page */
    if (keyBits_IsReadOnly(inv->key)) {
      inv->exit.code = RC_eros_key_NoAccess;
      return;
    }

    /* Mark the object dirty. */

    objH_MakeObjectDirty(key_GetObjectPtr(inv->key));


    COMMIT_POINT();

    bzero((void*)pageAddress, EROS_PAGE_SIZE);

    inv->exit.code = RC_OK;
    return;
    
  case OC_Page_Clone:
    {
      /* copy content of page key in arg0 to current page */

      if (keyBits_IsReadOnly(inv->key)) {
	COMMIT_POINT();
	inv->exit.code = RC_eros_key_NoAccess;
	return;
      }

      /* FIX: This is now wrong: phys pages, time page */
      if (keyBits_IsType(inv->entry.key[0], KKT_Page) == false) {
	COMMIT_POINT();
	inv->exit.code = RC_eros_key_RequestError;
	return;
      }

      /* Mark the object dirty. */
    
      objH_MakeObjectDirty(key_GetObjectPtr(inv->key));

      key_Prepare(inv->entry.key[0]);

      assert(keyBits_IsPrepared(inv->key));

      COMMIT_POINT();


      invokedPage = objC_ObHdrToPage(key_GetObjectPtr(inv->key));
      copiedPage = objC_ObHdrToPage(key_GetObjectPtr(inv->entry.key[0]));


      memcpy((void *) invokedPage, (void *) copiedPage, EROS_PAGE_SIZE);
						    
      inv->exit.code = RC_OK;
      return;
    }

  default:
    COMMIT_POINT();

    break;
  }

  inv->exit.code = RC_eros_key_UnknownRequest;
  return;
}
