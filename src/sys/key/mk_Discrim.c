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
#include <kerninc/Activity.h>
#include <kerninc/Invocation.h>
#include <eros/Invoke.h>
#include <eros/StdKeyType.h>

#include <idl/eros/key.h>
#include <idl/eros/Discrim.h>

void
DiscrimKey(Invocation* inv /*@ not null @*/)
{
  switch(inv->entry.code) {

  case OC_eros_Discrim_classify:

    key_Prepare(inv->entry.key[0]);


    COMMIT_POINT();
  
    inv->exit.code = RC_OK;	/* until proven otherwise */

    switch(keyBits_GetType(inv->entry.key[0])) {
    case KKT_Number:
      inv->exit.w1 = eros_Discrim_clNumber;
      break;
    case KKT_Resume:
      inv->exit.w1 = eros_Discrim_clResume;
      break;
    case KKT_Page:
    case KKT_Node:
    case KKT_Segment:
    case KKT_Wrapper:		/* FIX: Is this correct? */
      inv->exit.w1 = eros_Discrim_clMemory;
      break;
    case KKT_Sched:
      inv->exit.w1 = eros_Discrim_clSched;
      break;
    case KKT_Void:
      inv->exit.w1 = eros_Discrim_clVoid;
      break;
    default:
      inv->exit.w1 = eros_Discrim_clOther;
      break;
    }

    return;

  case OC_eros_Discrim_verify:
 
    key_Prepare(inv->entry.key[0]);
 

    COMMIT_POINT();
  
    inv->exit.code = RC_OK;
    inv->exit.w1 = 0;		/* default is not discreet */
    switch(keyBits_GetType(inv->entry.key[0])) {
    case KKT_Page:
      if (keyBits_IsReadOnly(inv->entry.key[0]))
	inv->exit.w1 = 1;
      break;
    case KKT_Wrapper:
    case KKT_Segment:
    case KKT_Node:
      if (keyBits_IsReadOnly(inv->entry.key[0]) &&
	  keyBits_IsNoCall(inv->entry.key[0]) &&
	  keyBits_IsWeak(inv->entry.key[0]))	  
	inv->exit.w1 = 1;
      break;
    case KKT_Number:
      inv->exit.w1 = 1;
      break;
    case KKT_Discrim:
    case KKT_Void:
      inv->exit.w1 = 1;
      break;
    default:
      break;
    }
    
    return;

  case OC_eros_Discrim_compare:

    key_Prepare(inv->entry.key[0]);
    key_Prepare(inv->entry.key[1]);

    
    COMMIT_POINT();
  
    inv->exit.code = RC_OK;
    inv->exit.w1 = 1;

    if (keyBits_GetType(inv->entry.key[0]) != keyBits_GetType(inv->entry.key[1]))
      inv->exit.w1 = 0;
    else if ( keyBits_IsObjectKey(inv->entry.key[0]) ) {
      if ( inv->entry.key[0]->u.ok.pObj != inv->entry.key[1]->u.ok.pObj )
	inv->exit.w1 = 0;
    }
    else {
      if (inv->entry.key[0]->u.nk.value[0] != inv->entry.key[0]->u.nk.value[0])
	inv->exit.w1 = 0;
      if (inv->entry.key[0]->u.nk.value[1] != inv->entry.key[0]->u.nk.value[1])
	inv->exit.w1 = 0;
      if (inv->entry.key[0]->u.nk.value[2] != inv->entry.key[0]->u.nk.value[2])
	inv->exit.w1 = 0;
    }
	     
    /* We do not compare the flags fields, as all of those bits are
     * purely kernel-internal. */

    if (inv->entry.key[0]->keyData != inv->entry.key[1]->keyData)
      inv->exit.w1 = 0;

    return;
    
  case OC_eros_key_getType:
    COMMIT_POINT();
  
    inv->exit.code = RC_OK;
    inv->exit.w1 = AKT_Discrim;
    return;
  default:
    COMMIT_POINT();
  
    break;
  }

  inv->exit.code = RC_eros_key_UnknownRequest;
  return;
}
