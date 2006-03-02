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
#include <kerninc/Process.h>
#include <kerninc/Key.h>
#include <kerninc/ObjectHeader.h>
#include <kerninc/Node.h>

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

  /* Load the "Last Invoked Key" register value */
  key_NH_Set(&thisPtr->lastInvokedKey, 
	     &thisPtr->procRoot->slot[ProcLastInvokedKey]);
  keyBits_SetRwHazard(node_GetKeyAtSlot(thisPtr->procRoot ,ProcLastInvokedKey));

  key_Prepare(&thisPtr->procRoot->slot[ProcGenKeys]);
  kn = (Node *) key_GetObjectPtr(&thisPtr->procRoot->slot[ProcGenKeys]);
  assertex(kn,objH_IsUserPinned(DOWNCAST(kn, ObjectHeader)));
  

  assert ( node_Validate(kn) );
  node_Unprepare(kn, false);

  if (kn->node_ObjHdr.obType != ot_NtUnprepared || kn == thisPtr->procRoot) {
    proc_SetMalformed(thisPtr);
    return;
  }

  objH_MakeObjectDirty(DOWNCAST(kn, ObjectHeader));

  for (k = 0; k < EROS_NODE_SIZE; k++) {
#ifndef NDEBUG
    if ( keyBits_IsHazard(node_GetKeyAtSlot(kn, k)))
      dprintf(true, "Key register slot %d is hazarded in node 0x%08x%08x\n",
		      k,
		      (uint32_t) (kn->node_ObjHdr.kt_u.ob.oid >> 32), (uint32_t) kn->node_ObjHdr.kt_u.ob.oid);

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
  
  if ( ((uint32_t) pKey == (uint32_t) &p->lastInvokedKey) )
    goto check_offset;

  if ( ((uint32_t) pKey < (uint32_t) &p->keyReg[0] ) || 
       ((uint32_t) pKey >= (uint32_t) &p->keyReg[EROS_PROCESS_KEYREGS]) )
    return false;

 check_offset:
  offset = ((uint32_t) pKey) - ((uint32_t) &p->keyReg[0]);

  offset %= sizeof(Key);
  if (offset == 0)
    return true;
  
  return false;
}

bool
ValidCtxtKeyRingPtr(const KeyRing* kr)
{
  uint32_t c;
  for (c = 0; c < KTUNE_NCONTEXT; c++) {
    Process *ctxt = &proc_ContextCache[c];

    if (kr == &ctxt->keyRing)
      return true;
  }

  return false;
}
#endif	// NDEBUG
