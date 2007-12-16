/*
 * Copyright (C) 1998, 1999, Jonathan S. Shapiro.
 * Copyright (C) 2006, 2007, Strawberry Development Group.
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
/* This material is based upon work supported by the US Defense Advanced
Research Projects Agency under Contract No. W31P4Q-07-C-0070.
Approved for public release, distribution unlimited. */

#include <kerninc/kernel.h>
#include <kerninc/Process.h>
#include <disk/DiskNodeStruct.h>
#include <kerninc/Key.h>
#include <kerninc/ObjectHeader.h>
#include <kerninc/Node.h>
#include <kerninc/Activity.h>

void 
proc_SetFault(Process * thisPtr, uint32_t code, uint32_t info)
{
  assert(code);

  thisPtr->faultCode = code;
  thisPtr->faultInfo = info;
  thisPtr->processFlags |= capros_Process_PF_FaultToProcessKeeper;
  
#ifdef OPTION_DDB
  if (thisPtr->kernelFlags & KF_DDBTRAP)
    dprintf(true, "Process 0x%08x has trap code set\n", thisPtr);
#endif
#if 0	// if failing fast
  // Halt if no keeper:
  Key * key = & thisPtr->procRoot->slot[ProcKeeper];
  assert(keyBits_IsType(key, KKT_Start));
#endif
}

void 
proc_FlushKeyRegs(Process * thisPtr)
{
  uint32_t k;
  assert(thisPtr->procRoot);
  assert(thisPtr->keysNode);
  assert((thisPtr->hazards & hz_KeyRegs) == 0);
  assert(objH_IsDirty(node_ToObj(thisPtr->keysNode)));
  assert(node_Validate(thisPtr->keysNode));

#if 0
  printf("Flushing key regs on ctxt=0x%08x\n", this);
  if (inv.IsActive() && inv.invokee == this)
    dprintf(true,"THAT WAS INVOKEE!\n");
#endif

  for (k = 0; k < EROS_NODE_SIZE; k++) {
    Key * key = node_GetKeyAtSlot(thisPtr->keysNode, k);
    keyBits_UnHazard(key);
    key_NH_Set(key, &thisPtr->keyReg[k]);

    /* Not hazarded because key register */
    key_NH_SetToVoid(&thisPtr->keyReg[k]);

    /* We know that the context structure key registers are unhazarded
     * and unlinked by virtue of the fact that they are unloaded.
     */
  }

  thisPtr->keysNode->node_ObjHdr.prep_u.context = 0;
  thisPtr->keysNode->node_ObjHdr.obType = ot_NtUnprepared;
  thisPtr->keysNode = 0;

  thisPtr->hazards |= hz_KeyRegs;

  keyBits_UnHazard(&thisPtr->procRoot->slot[ProcGenKeys]);
}

/* May Yield. */
void 
proc_LoadKeyRegs(Process* thisPtr)
{
  Node *kn = 0;
  uint32_t k;
  assert (thisPtr->hazards & hz_KeyRegs);
  assert (keyBits_IsHazard(&thisPtr->procRoot->slot[ProcGenKeys]) == false);


  if (keyBits_IsType(&thisPtr->procRoot->slot[ProcGenKeys], KKT_Node) == false) {
    proc_SetMalformed(thisPtr);
    return;
  }

  key_Prepare(&thisPtr->procRoot->slot[ProcGenKeys]);
  kn = (Node *) key_GetObjectPtr(&thisPtr->procRoot->slot[ProcGenKeys]);
  assertex(kn,objH_IsUserPinned(DOWNCAST(kn, ObjectHeader)));
  

  assert ( node_Validate(kn) );
  node_Unprepare(kn, false);

  if (kn->node_ObjHdr.obType != ot_NtUnprepared || kn == thisPtr->procRoot) {
    proc_SetMalformed(thisPtr);
    return;
  }

  node_MakeDirty(kn);

  for (k = 0; k < EROS_NODE_SIZE; k++) {
#ifndef NDEBUG
    if ( keyBits_IsHazard(node_GetKeyAtSlot(kn, k)))
      dprintf(true, "Key register slot %d is hazarded in node 0x%08x%08x\n",
		      k,
		      (uint32_t) (kn->node_ObjHdr.oid >> 32),
                      (uint32_t) kn->node_ObjHdr.oid);

    /* We know that the context structure key registers are unhazarded
     * and unprepared by virtue of the fact that they are unloaded,
     * but check here just in case:
     */
    if ( keyBits_IsHazard(&thisPtr->keyReg[k]) )
      dprintf(true, "Key register %d is hazarded in Process 0x%08x\n",
		      k, thisPtr);

    if ( keyBits_IsUnprepared(&thisPtr->keyReg[k]) == false )
      dprintf(true, "Key register %d is prepared in Process 0x%08x\n",
		      k, thisPtr);
#endif

    if (k == 0)
      key_NH_SetToVoid(&thisPtr->keyReg[0]);	/* key register 0 is always void */
    else
      key_NH_Set(&thisPtr->keyReg[k], &kn->slot[k]);

    keyBits_SetRwHazard(node_GetKeyAtSlot(kn ,k));
  }
  
  /* Node is now known to be valid... */
  kn->node_ObjHdr.obType = ot_NtKeyRegs;
  kn->node_ObjHdr.prep_u.context = thisPtr;
  thisPtr->keysNode = kn;

  keyBits_SetWrHazard(&thisPtr->procRoot->slot[ProcGenKeys]);

  assert(thisPtr->keysNode);

  thisPtr->hazards &= ~hz_KeyRegs;
}

/* Rewrite the process key back to our current activity.  Note that
 * the activity's process key is not reliable unless this unload has
 * been performed.
 */
void
proc_SyncActivity(Process * thisPtr)
{
  Key * procKey = 0;
  assert(thisPtr->curActivity);
  assert(thisPtr->procRoot);
  assert(thisPtr->curActivity->context == thisPtr);
  
  procKey /*@ not null @*/ = &thisPtr->curActivity->processKey;

  assert (keyBits_IsHazard(procKey) == false);

  /* Not hazarded because activity key */
  key_NH_Unchain(procKey);

  keyBits_InitType(procKey, KKT_Process);
  procKey->u.unprep.oid = thisPtr->procRoot->node_ObjHdr.oid;
  procKey->u.unprep.count = thisPtr->procRoot->node_ObjHdr.allocCount; 
}

void
proc_SetCommonRegs32(Process * thisPtr,
  struct capros_Process_CommonRegisters32 * regs)
{
  assert (proc_IsRunnable(thisPtr));

  assert(! (thisPtr->hazards & hz_DomRoot));
  
  /* Ignore len, architecture */
  proc_SetCommonRegs32MD(thisPtr, regs);

  thisPtr->processFlags = regs->procFlags;

  if (regs->faultCode == capros_Process_FC_NoFault) {
    proc_ClearFault(thisPtr);
  } else {
    thisPtr->faultCode        = regs->faultCode;
    thisPtr->faultInfo        = regs->faultInfo;
  }
}

#ifdef OPTION_DDB
void
proc_WriteBackKeySlot(Process* thisPtr, uint32_t k)
{
  /* Write back a single key in support of DDB getting the display correct */
  assert ((thisPtr->hazards & hz_KeyRegs) == 0);
  assert (objH_IsDirty(node_ToObj(thisPtr->keysNode)));

  keyBits_UnHazard(&thisPtr->keysNode->slot[k]);
  key_NH_Set(&thisPtr->keysNode->slot[k], &thisPtr->keyReg[k]);

  keyBits_SetRwHazard(&thisPtr->keysNode->slot[k]);
}
#endif

#ifndef NDEBUG
bool
ValidCtxtPtr(const Process *ctxt)
{
  uint32_t offset;
  if ( ((uint32_t) ctxt < (uint32_t) proc_ContextCache ) || 
       ((uint32_t) ctxt >= (uint32_t)
	&proc_ContextCache[KTUNE_NCONTEXT]) )
    return false;

  offset = ((uint32_t) ctxt) - ((uint32_t) proc_ContextCache);
  offset %= sizeof(Process);

  if (offset == 0)
    return true;
  return false;
}

bool 
proc_ValidKeyReg(const Key *pKey)
{
  uint32_t ctxt;
  Process *p = 0;
  uint32_t offset;
  if ( ((uint32_t) pKey < (uint32_t) proc_ContextCache ) || 
       ((uint32_t) pKey >= (uint32_t)
	&proc_ContextCache[KTUNE_NCONTEXT]) )
    return false;

  /* Find the containing context: */
  ctxt = ((uint32_t) pKey) - ((uint32_t) proc_ContextCache);
  ctxt /= sizeof(Process);

  p = &proc_ContextCache[ctxt];
  
  if ( ((uint32_t) pKey < (uint32_t) &p->keyReg[0] ) || 
       ((uint32_t) pKey >= (uint32_t) &p->keyReg[EROS_PROCESS_KEYREGS]) )
    return false;

  offset = ((uint32_t) pKey) - ((uint32_t) &p->keyReg[0]);

  offset %= sizeof(Key);
  if (offset == 0)
    return true;
  
  return false;
}

bool
ValidCtxtKeyRingPtr(const KeyRing * kr)
{
  uint32_t const kri = (uint32_t)kr;
  if (kri < (uint32_t)&proc_ContextCache[0])
    return false;		// before the table
  if (kri >= (uint32_t)&proc_ContextCache[KTUNE_NCONTEXT])
    return false;		// after the table
  if ((kri - (uint32_t)&proc_ContextCache[0].keyRing) % sizeof(Process))
     return false;		// not a keyRing pointer
  return true;
}
#endif	// NDEBUG
