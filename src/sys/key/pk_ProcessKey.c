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
#include <kerninc/Process.h>
#include <arch-kerninc/Process-inline.h>
#include <kerninc/Invocation.h>
#include <kerninc/Node.h>
#include <kerninc/Activity.h>
#include <eros/Invoke.h>
#include <eros/StdKeyType.h>
#include <eros/ProcessKey.h>
#include <eros/machine/Registers.h>

#include <idl/eros/key.h>

/* #define DEBUG */

void
ProcessKey(Invocation* inv /*@ not null @*/)
{

  Node *theNode = (Node *) key_GetObjectPtr(inv->key);


  if (inv->invokee && theNode == inv->invokee->procRoot
      && OC_Process_Swap == inv->entry.code
      && inv->entry.w1 != 2
      && inv->entry.w1 != 4)
    dprintf(false, "Modifying invokee domain root\n");

  if (inv->invokee && theNode == inv->invokee->keysNode
      && OC_Process_SwapKeyReg == inv->entry.code)
    dprintf(false, "Modifying invokee keys node\n");

  switch (inv->entry.code) {
  case OC_eros_key_getType:
    {
      COMMIT_POINT();

      inv->exit.code = RC_OK;
      inv->exit.w1 = AKT_Process;
      return;
    }

  case OC_Process_Copy:
    {
      uint32_t slot = inv->entry.w1;

      COMMIT_POINT();

      if (slot >= EROS_NODE_SIZE)
	dprintf(true, "Copy slot out of range\n");

      if (slot == ProcBrand || slot == ProcLastInvokedKey ||
	  slot >= EROS_NODE_SIZE) {
	inv->exit.code = RC_eros_key_RequestError;
	return;
      }

      /* All of these complete ok. */
      inv->exit.code = RC_OK;


      if (keyBits_IsRdHazard(node_GetKeyAtSlot(theNode, slot)))
	node_ClearHazard(theNode, slot);
      inv_SetExitKey(inv, 0, &theNode->slot[slot]);


      return;
    }      

  case OC_Process_Swap:
    {
      uint32_t slot;
      if (theNode == act_CurContext()->keysNode)
	dprintf(true, "Swap involving sender keys\n");


      slot = inv->entry.w1;

      if (slot == ProcBrand || slot == ProcLastInvokedKey
	  || slot >= EROS_NODE_SIZE) {
	COMMIT_POINT();

	inv->exit.code = RC_eros_key_RequestError;
	return;
      }

      /* All of these complete ok. */
      inv->exit.code = RC_OK;
      

      objH_MakeObjectDirty(DOWNCAST(theNode, ObjectHeader));


      COMMIT_POINT();
      

      node_ClearHazard(theNode, slot);

      {
	Key k;			/* temporary in case send and receive */
				/* slots are the same. */
        
        keyBits_InitToVoid(&k);
	key_NH_Set(&k, &theNode->slot[slot]);
	

	key_NH_Set(node_GetKeyAtSlot(theNode, slot), inv->entry.key[0]);
	inv_SetExitKey(inv, 0, &k);
        
	/* Unchain, but do not unprepare -- the objects do not have
	 * on-disk keys. 
	 */
	key_NH_Unchain(&k);

      }

      /* Thread::Current() changed to act_Current() */
      act_Prepare(act_Current());
      
      return;
    }      

  case OC_Process_CopyKeyReg:
    {
      uint32_t slot;
      Process* ac = node_GetDomainContext(theNode);
      proc_Prepare(ac);

      COMMIT_POINT();

      slot = inv->entry.w1;

      if (slot < EROS_NODE_SIZE) {
	inv_SetExitKey(inv, 0, &ac->keyReg[slot]);
	inv->exit.code = RC_OK;
      }
      else {
	inv->exit.code = RC_eros_key_RequestError;
      }

      return;
    }

  case OC_Process_SwapKeyReg:
    {
      Process* ac = 0;
      uint32_t slot;
      if (theNode == act_CurContext()->keysNode)
	dprintf(true, "Swap involving sender keys\n");

      ac = node_GetDomainContext(theNode);
      proc_Prepare(ac);

      COMMIT_POINT();

      slot = inv->entry.w1;

      if (slot >= EROS_NODE_SIZE) {
	inv->exit.code = RC_eros_key_RequestError;
	return;
      }
      

      inv_SetExitKey(inv, 0, &ac->keyReg[slot]);


      if (slot != 0) {
	/* FIX: verify that the damn thing HAD key registers?? */

	key_NH_Set(&ac->keyReg[slot], inv->entry.key[0]);


#if 0
	printf("set key reg slot %d to \n", slot);
	inv.entry.key[0].Print();
#endif
      }

      inv->exit.code = RC_OK;
      return;
    }
#if 0
  case OC_Process_GetCtrlInfo32:
    {
      DomCtlInfo32_s info;

      Node *domRoot = (Node *) inv.key->GetObjectPtr();
      
      inv.exit.code = RC_OK;

      ArchContext* ac = domRoot->GetDomainContext();
      ac->Prepare();
      
      COMMIT_POINT();
      
      if (ac->GetControlInfo32(info) == false)
	inv.exit.code = RC_Process_Malformed;

      inv.CopyOut(sizeof(info), &info);
      return;
    }
#endif    
  case OC_Process_GetRegs32:
    {
      Process* ac = 0;
      Registers regs;
      
      /* GetRegs32 length is machine specific, so it does its own copyout. */
      inv->exit.code = RC_OK;

      assert( proc_IsRunnable(inv->invokee) );

#if 0
      printf("GetRegs32: invokee is 0x%08x, IsActive? %c\n",
		     inv.invokee, inv.IsActive() ? 'y' : 'n');
#endif

      ac = node_GetDomainContext(theNode);
      proc_Prepare(ac);

#ifndef OPTION_PURE_EXIT_STRINGS
      proc_SetupExitString(inv->invokee, inv, sizeof(regs));
#endif

      COMMIT_POINT();

      if (proc_GetRegs32(ac, &regs) == false)
	inv->exit.code = RC_Process_Malformed;


      inv_CopyOut(inv, sizeof(regs), &regs);
      return;
    }
#if 0    
  case OC_Process_SetCtrlInfo32:
    {
      DomCtlInfo32_s info;

      if ( inv.entry.len != sizeof(info) ) {
	inv.exit.code = RC_eros_key_RequestError;
	COMMIT_POINT();
      
	return;
      }
      
      inv.exit.code = RC_OK;
      ArchContext* ac = theNode->GetDomainContext();
      ac->Prepare();
      
#ifndef OPTION_PURE_ENTRY_STRINGS
      Thread::CurContext()->SetupEntryString(inv);
#endif

      COMMIT_POINT();

      inv.CopyIn(sizeof(info), &info);

      if (ac->SetControlInfo32(info) == false)
	inv.exit.code = RC_Process_Malformed;

      return;
    }
#endif
  
  case OC_Process_SetRegs32:
    {
      Registers regs;
      Process* ac = 0;
      
      if ( inv->entry.len != sizeof(regs) ) {
	inv->exit.code = RC_eros_key_RequestError;
	COMMIT_POINT();
      
	return;
      }
      
      /* FIX: check embedded length and arch type! */
      
      /* GetRegs32 length is machine specific, so it does its own copyout. */
      inv->exit.code = RC_OK;

      ac = node_GetDomainContext(theNode);
      proc_Prepare(ac);

#ifndef OPTION_PURE_ENTRY_STRINGS
      proc_SetupEntryString(act_CurContext(), inv);
#endif

      COMMIT_POINT();

      inv_CopyIn(inv, sizeof(regs), &regs);


      if (proc_SetRegs32(ac, &regs) == false)
	inv->exit.code = RC_Process_Malformed;

      return;
    }

  case OC_Process_SwapMemory32:
    {
      Process* ac = 0;
      inv->exit.w1 = inv->entry.w1;
      inv->exit.w2 = inv->entry.w2;
      inv->exit.w3 = inv->entry.w3;
      
      /* All of these complete ok. */
      inv->exit.code = RC_OK;
      
      ac = node_GetDomainContext(theNode);
      proc_Prepare(ac);

      COMMIT_POINT();


      proc_SetPC(ac, inv->entry.w1);
      
      node_ClearHazard(theNode, ProcAddrSpace);


      {
	Key k;

        keyBits_InitToVoid(&k);
        key_NH_Set(&k, &theNode->slot[ProcAddrSpace]);
	

	key_NH_Set(node_GetKeyAtSlot(theNode, ProcAddrSpace), inv->entry.key[0]);
	inv_SetExitKey(inv, 0, &k);

	/* Unchain, but do not unprepare -- the objects do not have
	 * on-disk keys. 
	 */
	key_NH_Unchain(&k);

      }
      
      ac->nextPC = proc_GetPC(ac);

      return;
    }

  case OC_Process_MkStartKey:
    {
      Process* p = node_GetDomainContext(theNode);
      uint32_t keyData;
      proc_Prepare(p);

      COMMIT_POINT();
      
      keyData = inv->entry.w1;
      
      if ( keyData > EROS_KEYDATA_MAX ) {
	inv->exit.code = RC_eros_key_RequestError;
	dprintf(true, "Value 0x%08x is out of range\n",	keyData);
      
	return;
      }

      /* All of these complete ok. */
      inv->exit.code = RC_OK;
      
      /* AAARRRGGGGGHHHH!!!!! This used to fabricate an unprepared
       * key, which was making things REALLY slow.  We do need to be
       * careful, as gate keys do not go on the same key chain as
       * domain keys.  Still, it was just never THAT hard to get it
       * right.  Grumble.
       */
      
      if (inv->exit.pKey[0]) {
	Key *k /*@ not null @*/ = inv->exit.pKey[0];

	key_NH_Unchain(k);
        keyBits_InitType(k, KKT_Start);
	k->keyData = keyData;
	k->u.gk.pContext = p;

	link_insertAfter(&p->keyRing, &k->u.gk.kr);

	keyBits_SetPrepared(k);
      }

      return;
    }

  /* I'm holding off on these because they require killing the
   * active thread, and I want to make sure I do that when I'm awake.
   */
  case OC_Process_MkProcessAvailable:
  case OC_Process_MkProcessWaiting:
    COMMIT_POINT();

    inv->exit.code = RC_eros_key_UnknownRequest;
    return;

  case OC_Process_MkResumeKey:
  case OC_Process_MkFaultKey:
    COMMIT_POINT();

    inv_SetExitKey(inv, 0, inv->key);
    if (inv->exit.pKey[0]) {
      key_NH_Unprepare(inv->exit.pKey[0]);
      keyBits_InitType(inv->exit.pKey[0], KKT_Resume);
      inv->exit.pKey[0]->keyPerms =
	(inv->entry.code == OC_Process_MkResumeKey) ? 0 : KPRM_FAULT;
    }

    inv->exit.code = RC_OK;

    return;

  case OC_Process_GetFloatRegs:
  case OC_Process_SetFloatRegs:
    COMMIT_POINT();
      
    fatal("Domain key order %d ot yet implemented\n",
		  inv->entry.code);
    
  default:
    COMMIT_POINT();
      
    inv->exit.code = RC_eros_key_UnknownRequest;
    return;
  }
}
