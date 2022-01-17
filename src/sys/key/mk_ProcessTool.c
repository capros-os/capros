/*
 * Copyright (C) 1998, 1999, 2001, Jonathan S. Shapiro.
 * Copyright (C) 2006-2008, 2011, Strawberry Development Group.
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
#include <kerninc/GPT.h>
#include <disk/Forwarder.h>
#include <eros/Invoke.h>
#include <eros/StdKeyType.h>
#include <kerninc/Key-inline.h>

#include <idl/capros/key.h>
#include <idl/capros/ProcTool.h>
#include <idl/capros/Forwarder.h>
#include <idl/capros/GPT.h>

/* CompareBrand(inv, pDomKey, pBrand) returns TRUE if the brand of the
   passed pDomKey is equal to the passed pBrand.
   Otherwise, it returns FALSE.

   Iff CompareBrand returns TRUE, then it also updates inv.exit.w1 and
   inv.exit.w2 to hold the capability type (start or resume) and the
   key info field, respectively,
   and returns a node key in exit key 0. */
/* May Yield. */
static bool
CompareBrand(Invocation* inv /*@ not null @*/, Key* pDomKey, Key* pBrand)
{
  assert(keyBits_IsPrepared(pDomKey));
  assert(keyBits_IsProcessType(pDomKey));

  key_Prepare(pBrand);

  Node * procRoot = pDomKey->u.gk.pContext->procRoot;
  Key * otherBrand = node_GetKeyAtSlot(procRoot, ProcBrand);

  key_Prepare(otherBrand);

  COMMIT_POINT();
  
  /* Expensive comparison, primarily because we do not want to
   * unprepare the keys (unpreparing them would bump allocation counts
   * unnecessarily).
   */

  if ( keyBits_GetType(pBrand) != keyBits_GetType(otherBrand) 
       /* Do not compare flags field -- that is purely internal. */
       || pBrand->keyData != otherBrand->keyData )
    return false;

  /* If they *did* prepare, and they are object keys, then the object
   * pointers will be the same.
   */
  if ( keyBits_IsObjectKey(pBrand) ) {
    if (pBrand->u.ok.pObj != otherBrand->u.ok.pObj)
      return false;
  }
  else {
    if ( pBrand->u.nk.value[0] != otherBrand->u.nk.value[0]
	 || pBrand->u.nk.value[1] != otherBrand->u.nk.value[1]
	 || pBrand->u.nk.value[2] != otherBrand->u.nk.value[2] )
      return false;
  }

  // The keys match.
  
  inv->exit.w1 = keyBits_GetType(pDomKey);
  inv->exit.w2 = pDomKey->keyData;
  
  Key * k = inv->exit.pKey[0];
  if (k) {
    key_NH_Unchain(k);
    key_SetToObj(k, node_ToObj(procRoot), KKT_Node, 0, 0);
  }

  return true;
}

/* May Yield. */
void
ProcessToolKey(Invocation* inv /*@ not null @*/)
{
  inv_GetReturnee(inv);

#if 0
  Key& arg0Key = inv.entry.key[0]; /* user-provided key arg 0 */
  Key& arg1Key = inv.entry.key[1]; /* user-provided brand key */
  Key& domKey = arg0Key;	/* key to domain */
#endif
  
  /* Until proven otherwise: */
  inv->exit.code = RC_capros_key_RequestError;

  /* All of these operations return a single key, which is void until
   * proven otherwise:
   */

  switch (inv->entry.code) {
  case OC_capros_ProcTool_makeProcess:
    {
      Key * key0 = inv->entry.key[0];
      key_Prepare(key0);

      if (! keyBits_IsType(key0, KKT_Node)
          || keyBits_IsReadOnly(key0)
          || keyBits_IsNoCall(key0) ) {
#if 0
        dprintf(true, "domtool: entry key is not valid\n");
#endif
        COMMIT_POINT();

	break;
      }

      assert(! keyBits_IsHazard(key0));
      Node * pNode = objH_ToNode(key0->u.ok.pObj);
      Process * proc = node_GetProcess(pNode);

      COMMIT_POINT();

      inv->exit.code = RC_OK;

      Key * k = inv->exit.pKey[0];
      if (k) {	// the key is being received
        key_NH_Unchain(k);
        key_SetToProcess(k, proc, KKT_Process, 0);         
      }
      break;
    }
  case OC_capros_ProcTool_canOpener:
    {
      bool isResume;
      key_Prepare(inv->entry.key[0]);

      if (! keyBits_IsGateKey(inv->entry.key[0])) {
	COMMIT_POINT();
        inv->exit.code = RC_capros_key_NoAccess;
	break;
      }

      isResume = keyBits_IsType(inv->entry.key[0], KKT_Resume);

      if (! CompareBrand(inv, inv->entry.key[0], inv->entry.key[1])) {
        inv->exit.code = RC_capros_key_NoAccess;
	break;
      }
      
      inv->exit.code = RC_OK;

      if (isResume) {
        inv->exit.w1 = 2;
        inv->exit.w2 = 0;
      } else {
        inv->exit.w1 = 1;
	inv->exit.w2 = inv->entry.key[0]->keyData;
      }

      Key * key = inv->exit.pKey[0];
      if (key) {
        key_NH_Unchain(key);
        key_SetToObj(key,
                     node_ToObj(inv->entry.key[0]->u.gk.pContext->procRoot),
                     KKT_Node, 0, 0);
      }

      break;
    }

  case OC_capros_ProcTool_identifyProcess:
    {
      Key * key0 = inv->entry.key[0];

      key_Prepare(key0);

      if (! keyBits_IsType(key0, KKT_Process)) {

	COMMIT_POINT();
        inv->exit.code = RC_capros_key_NoAccess;
	break;
      }

      if (CompareBrand(inv, key0, inv->entry.key[1])) {
        inv->exit.w1 = inv->exit.w2 = 0;	// no information returned here
        inv->exit.code = RC_OK;
      } else {
        inv->exit.code = RC_capros_key_NoAccess;
      }

      break;
    }

  case OC_capros_ProcTool_identGPTKeeper:
    {
      key_Prepare(inv->entry.key[0]);

      if (! keyBits_IsType(inv->entry.key[0], KKT_GPT)
          || keyBits_IsNoCall(inv->entry.key[0]) ) {
	COMMIT_POINT();
        inv->exit.code = RC_capros_key_NoAccess;
	break;
      }

      GPT * theGPT = (GPT *) key_GetObjectPtr(inv->entry.key[0]);

      /* If no keeper key: */
      if (! (gpt_GetL2vField(theGPT) & GPT_KEEPER)) {
	COMMIT_POINT();
        inv->exit.code = RC_capros_key_NoAccess;
	break;
      }

      Key * domKey = node_GetKeyAtSlot(theGPT, capros_GPT_keeperSlot);

      key_Prepare(domKey);
      
      if (! keyBits_IsGateKey(domKey)) {
	COMMIT_POINT();
        inv->exit.code = RC_capros_key_NoAccess;
	break;
      }

      bool isResume = keyBits_IsType(domKey, KKT_Resume);
      
      if (! CompareBrand(inv, domKey, inv->entry.key[1])) {
        inv->exit.code = RC_capros_key_NoAccess;
	break;
      }
      
      inv->exit.code = RC_OK;

      /* Must be either start or resume, by virtue of test above. */
      inv->exit.w1 = BoolToBit(isResume);

      if ( isResume )
	inv->exit.w2 = 0;
      else
	inv->exit.w2 = inv->entry.key[0]->keyData;

      if (inv->exit.pKey[0]) {
	inv_SetExitKey(inv, 0, inv->entry.key[0]);
	inv->exit.pKey[0]->keyPerms &= ~ capros_Memory_opaque;
      }

      break;
    }
  case OC_capros_ProcTool_identForwarderTarget:
    {
      key_Prepare(inv->entry.key[0]);

      if (! keyBits_IsType(inv->entry.key[0], KKT_Forwarder)
#if 0	// disabled so spacebank will work
          || ! (inv->entry.key[0]->keyData & capros_Forwarder_sendCap)
#endif
         ) {
	COMMIT_POINT();
        inv->exit.code = RC_capros_key_NoAccess;
	break;
      }

      Node * theNode = objH_ToNode(key_GetObjectPtr(inv->entry.key[0]));
      Key * domKey = node_GetKeyAtSlot(theNode, ForwarderTargetSlot);

      key_Prepare(domKey);
      
      if (! keyBits_IsGateKey(domKey)) {
        COMMIT_POINT();
        inv->exit.code = RC_capros_key_NoAccess;
	break;
      }
      
      if (! CompareBrand(inv, domKey, inv->entry.key[1])) {
        inv->exit.code = RC_capros_key_NoAccess;
	break;
      }

      inv->exit.code = RC_OK;

      if (keyBits_IsType(domKey, KKT_Resume)) {
        inv->exit.w1 = 2;
        inv->exit.w2 = 0;
      } else {
	inv->exit.w2 = domKey->keyData;
        inv->exit.w1 = 1;
      }

      if (inv->exit.pKey[0]) {
	inv_SetExitKey(inv, 0, inv->entry.key[0]);
	keyBits_SetType(inv->exit.pKey[0], KKT_Forwarder);
	inv->exit.pKey[0]->keyData = 0;	// not opaque
      }

      break;
    }

  case OC_capros_key_getType:
    COMMIT_POINT();

    inv->exit.code = RC_OK;
    inv->exit.w1 = AKT_ProcessTool;
    break;
  default:
    COMMIT_POINT();

    inv->exit.code = RC_capros_key_UnknownRequest;
    break;
  }
  ReturnMessage(inv);
}
