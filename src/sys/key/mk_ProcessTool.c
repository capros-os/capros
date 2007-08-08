/*
 * Copyright (C) 1998, 1999, 2001, Jonathan S. Shapiro.
 * Copyright (C) 2006, 2007, Strawberry Development Group.
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
#include <kerninc/Node.h>
#include <kerninc/GPT.h>
#include <disk/Forwarder.h>
#include <eros/Invoke.h>
#include <eros/StdKeyType.h>

#include <idl/capros/key.h>
#include <idl/capros/ProcTool.h>
#include <idl/capros/Forwarder.h>
#include <idl/capros/GPT.h>

/* CompareBrand(inv, pDomKey, pBrand) returns TRUE if the passed keys
 * were of the right type and it was feasible to compare their
 * brands. Otherwise, it returns FALSE.
 *
 * Iff CompareBrand returns TRUE, then it also updates inv.exit_w1 and
 * inv.exit_w2 to hold the capability type (start or resume) and the
 * key info field, respectively. */
/* May Yield. */
static bool
CompareBrand(Invocation* inv /*@ not null @*/, Key* pDomKey, Key* pBrand)
{
  /* It was some sort of identify operation -- see if we can satisfy
   * it.  Must not do this operation on prepared key, since the link
   * pointers mess things up rather badly.  Since we know that these
   * are argument keys, the deprepare will not damage any state in the
   * client key registers.
   * 
   * The brand slot never needs to be prepared anyway.
   */

  /* WE NEED TO MAKE A TEMPORARY COPY OF THESE KEYS in case one of
   * them proves to be the output key.
   */
  Node *domNode = 0;
  Key *otherBrand = 0;

  assert (keyBits_IsPrepared(pDomKey));
  key_Prepare(pBrand);

  domNode = (Node *) key_GetObjectPtr(pDomKey);
  otherBrand /*@ not null @*/ = node_GetKeyAtSlot(domNode, ProcBrand);

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
    if ( pBrand->keyData != pBrand->keyData
	 || pBrand->u.nk.value[0] != otherBrand->u.nk.value[0]
	 || pBrand->u.nk.value[1] != otherBrand->u.nk.value[1]
	 || pBrand->u.nk.value[2] != otherBrand->u.nk.value[2] )
      return false;
  }
  
  
  inv->exit.w1 = keyBits_GetType(pDomKey);
  inv->exit.w2 = pDomKey->keyData;
  
  /* Temporary keys of this form MUST NOT BE BUILT until after the
   * commit point!!!
   */
  {
    /* Unchain, but do not unprepare -- the objects do not have on-disk
     * keys. 
     */
    Process *pContext = 0;
    ObjectHeader *pObj = 0;
    if (keyBits_IsGateKey(pDomKey))
      pContext = pDomKey->u.gk.pContext;


    inv_SetExitKey(inv, 0, pDomKey);


    if (inv->exit.pKey[0]) {
      assert (keyBits_IsPrepared(inv->exit.pKey[0]));

      keyBits_InitType(inv->exit.pKey[0], KKT_Node);
      keyBits_SetPrepared(inv->exit.pKey[0]);

      /* We have really just copied the input key and smashed it for a
       * node key.  We now have a prepared node key, UNLESS the input key
       * was actually a gate key, in which event we have a prepared node
       * key on the wrong keyring.  Fix up the keyring in that case:
       */
  
      if (pContext) {

	key_NH_Unchain(inv->exit.pKey[0]);


	pObj = DOWNCAST(pContext->procRoot, ObjectHeader);
	assert (pObj);
	/* Link as next key after object */
	inv->exit.pKey[0]->u.ok.pObj = pObj;
  
	link_insertAfter(&pObj->keyRing, &inv->exit.pKey[0]->u.ok.kr);
      }
    }
  }

  return true;
}

/* May Yield. */
void
ProcessToolKey(Invocation* inv /*@ not null @*/)
{
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
      COMMIT_POINT();

      if (keyBits_IsType(inv->entry.key[0], KKT_Node) == false) {
#if 0
        dprintf(true, "domtool: entry key is not node key\n");
#endif
	return;
      }

      if ( keyBits_IsReadOnly(inv->entry.key[0]) ) {
#if 0
        printf("domtool: entry key is read only\n");
#endif
	return;
      }

      if ( keyBits_IsNoCall(inv->entry.key[0]) ) {
#if 0
        printf("domtool: entry key is no-call\n");
#endif
	return;
      }

      assert ( keyBits_IsHazard(inv->entry.key[0]) == false );

      inv->exit.code = RC_OK;
      

      inv_SetExitKey(inv, 0, inv->entry.key[0]);

      if (inv->exit.pKey[0]) {
	keyBits_SetType(inv->exit.pKey[0], KKT_Process);
	inv->exit.pKey[0]->keyData = 0;
      }
      return;
    }
  case OC_capros_ProcTool_canOpener:
    {
      bool isResume;
      bool sameBrand;
      key_Prepare(inv->entry.key[0]);

      
      if (keyBits_IsType(inv->entry.key[0], KKT_Start) == false &&
	  keyBits_IsType(inv->entry.key[0], KKT_Resume) == false) {
	COMMIT_POINT();
	return;
      }

      isResume = keyBits_IsType(inv->entry.key[0], KKT_Resume);
      
      inv->exit.code = RC_OK;

      sameBrand = CompareBrand(inv, inv->entry.key[0], inv->entry.key[1]);

      COMMIT_POINT();

      if (!sameBrand)
	return;

      /* Must be either start or resume, by virtue of test above. */
      /* FIX: yes, but why compare against exit.w1?? */
      inv->exit.w1 = (inv->exit.w1 == KKT_Start) ? 1 : 2;

      /* FIX: This seems exceptionally broken to me! */
      if ( isResume )
	inv->exit.w2 = inv->entry.key[0]->keyData;

      if (inv->exit.pKey[0]) {
	inv_SetExitKey(inv, 0, inv->entry.key[0]);

	key_NH_Unprepare(inv->exit.pKey[0]);	/* Why? */
	keyBits_SetType(inv->exit.pKey[0], KKT_Node);
	inv->exit.pKey[0]->keyData = 0;
      }

      return;
    }
#if 0
    /* Not currently used! */
  case OC_ProcTool_IdentProcessKey:
    {

      key_Prepare(inv->entry.key[0]);

      

      if (keyBits_IsType(inv->entry.key[0], KKT_Process) == false) {
	COMMIT_POINT();
	return;
      }

      inv->exit.code = RC_OK;
      inv->exit.w1 = 0;

      if ( CompareBrand(inv, inv->entry.key[0], inv->entry.key[1]) ) {
	inv->exit.w1 = 1;

	if (inv->exit.pKey[0]) {
	  inv_SetExitKey(inv, 0, inv->entry.key[0]);

	  keyBits_SetType(inv->exit.pKey[0], KKT_Node);
	  inv->exit.pKey[0]->keyData = 0;
	}
      }
      
      COMMIT_POINT();
      return;
    }
#endif
  case OC_capros_ProcTool_identGPTKeeper:
    {
      key_Prepare(inv->entry.key[0]);

      if (! keyBits_IsType(inv->entry.key[0], KKT_GPT)
          || keyBits_IsNoCall(inv->entry.key[0]) ) {
	COMMIT_POINT();
	return;
      }

      GPT * theGPT = (GPT *) key_GetObjectPtr(inv->entry.key[0]);

      /* If no keeper key: */
      if (! (gpt_GetL2vField(theGPT) & GPT_KEEPER)) {
	COMMIT_POINT();
	return;
      }

      Key * domKey = node_GetKeyAtSlot(theGPT, capros_GPT_keeperSlot);

      key_Prepare(domKey);

      bool isResume = keyBits_IsType(domKey, KKT_Resume);
      
      if (! keyBits_IsType(domKey, KKT_Start)
	  && ! isResume ) {
	COMMIT_POINT();
	return;
      }
      
      if ( CompareBrand(inv, domKey, inv->entry.key[1]) == false ) {
	COMMIT_POINT();
	return;
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

      COMMIT_POINT();
      return;
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
	return;
      }

      Node * theNode = objH_ToNode(key_GetObjectPtr(inv->entry.key[0]));
      Key * domKey = node_GetKeyAtSlot(theNode, ForwarderTargetSlot);

      key_Prepare(domKey);
      
      if (! keyBits_IsType(domKey, KKT_Start)
	  && ! keyBits_IsType(domKey, KKT_Resume) ) {
        COMMIT_POINT();
	return;
      }
      
      if (! CompareBrand(inv, domKey, inv->entry.key[1])) {
        COMMIT_POINT();
	return;
      }

      COMMIT_POINT();
      
      inv->exit.code = RC_OK;

      if (keyBits_IsType(domKey, KKT_Resume)) {
        inv->exit.w1 = 2;
      } else {
	inv->exit.w2 = domKey->keyData;
        inv->exit.w1 = 1;
      }

      if (inv->exit.pKey[0]) {
	inv_SetExitKey(inv, 0, inv->entry.key[0]);
	keyBits_SetType(inv->exit.pKey[0], KKT_Forwarder);
	inv->exit.pKey[0]->keyData = 0;	// not opaque
      }

      return;
    }

  case OC_capros_ProcTool_compareOrigins:
    {
      Node *domain0 = 0;
      Node *domain1 = 0;
      Key *brand0 = 0;
      Key *brand1 = 0;
      if ((keyBits_IsGateKey(inv->entry.key[0]) == false) &&
	  (keyBits_IsType(inv->entry.key[0], KKT_Process) == false)) {

	COMMIT_POINT();
	return;
      }

      if ((keyBits_IsGateKey(inv->entry.key[1]) == false) &&
	  (keyBits_IsType(inv->entry.key[1], KKT_Process) == false)) {

	COMMIT_POINT();
	return;
      }
      

      key_Prepare(inv->entry.key[0]);
      key_Prepare(inv->entry.key[1]);


      domain0 = (Node *) key_GetObjectPtr(inv->entry.key[0]);
      domain1 = (Node *) key_GetObjectPtr(inv->entry.key[1]);



      key_Prepare(node_GetKeyAtSlot(domain0, ProcBrand));
      key_Prepare(node_GetKeyAtSlot(domain1, ProcBrand));


      COMMIT_POINT();

      brand0 /*@ not null @*/ = node_GetKeyAtSlot(domain0, ProcBrand);
      brand1 /*@ not null @*/ = node_GetKeyAtSlot(domain1, ProcBrand);

      inv->exit.code = RC_OK;
      
      /* The following works ONLY because neither the domain brand key nor
       * a key coming from a key register can be hazarded.
       */
      if (memcmp( &brand0, &brand1, sizeof(Key)) == 0)
	inv->exit.w1 = 1;
      else
	inv->exit.w1 = 0;

      return;
    }

  case OC_capros_key_getType:
    COMMIT_POINT();

    inv->exit.code = RC_OK;
    inv->exit.w1 = AKT_ProcessTool;
    return;
  default:
    COMMIT_POINT();

    inv->exit.code = RC_capros_key_UnknownRequest;
    return;
  }
  return;
}
