/*
 * Copyright (C) 1998, 1999, 2001, Jonathan S. Shapiro.
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

#include <kerninc/kernel.h>
#include <kerninc/Key.h>
#include <kerninc/Activity.h>
#include <kerninc/Invocation.h>
#include <eros/Invoke.h>
#include <eros/StdKeyType.h>
#include <kerninc/Key-inline.h>

#include <idl/capros/key.h>
#include <idl/capros/Discrim.h>

/* May Yield. */
void
DiscrimKey(Invocation* inv /*@ not null @*/)
{
  inv_GetReturnee(inv);

  switch(inv->entry.code) {

  case OC_capros_Discrim_classify:

    key_Prepare(inv->entry.key[0]);


    COMMIT_POINT();
  
    inv->exit.code = RC_OK;	/* until proven otherwise */

    switch(keyBits_GetType(inv->entry.key[0])) {
    case KKT_Number:
      inv->exit.w1 = capros_Discrim_clNumber;
      break;
    case KKT_Resume:
      inv->exit.w1 = capros_Discrim_clResume;
      break;
    case KKT_Page:
    case KKT_Node:
    case KKT_GPT:
      inv->exit.w1 = capros_Discrim_clMemory;
      break;
    case KKT_Sched:
      inv->exit.w1 = capros_Discrim_clSched;
      break;
    case KKT_Void:
      inv->exit.w1 = capros_Discrim_clVoid;
      break;
    default:
      inv->exit.w1 = capros_Discrim_clOther;
      break;
    }

    break;

  case OC_capros_Discrim_verify:
 
    key_Prepare(inv->entry.key[0]);
 
    COMMIT_POINT();
  
    inv->exit.code = RC_OK;
    inv->exit.w1 = 0;		/* default is not discreet */
    switch(keyBits_GetType(inv->entry.key[0])) {
    case KKT_Page:
      if (keyBits_IsReadOnly(inv->entry.key[0]))
	inv->exit.w1 = 1;
      break;
    case KKT_Node:
    case KKT_GPT:
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
    
    break;

  case OC_capros_Discrim_compare:

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

    break;
    
  case OC_capros_key_getType:
    COMMIT_POINT();
  
    inv->exit.code = RC_OK;
    inv->exit.w1 = AKT_Discrim;
    break;

  default:
    COMMIT_POINT();
  
    inv->exit.code = RC_capros_key_UnknownRequest;
    break;
  }

  ReturnMessage(inv);
}
