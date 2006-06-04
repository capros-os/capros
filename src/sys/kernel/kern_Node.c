/*
 * Copyright (C) 1998, 1999, 2001, Jonathan S. Shapiro.
 * Copyright (C) 2006, Strawberry Development Group.
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
#include <kerninc/Node.h>
#include <kerninc/util.h>
#include <kerninc/Check.h>
#include <kerninc/Process.h>
#include <kerninc/ObjectHeader.h>
#include <kerninc/Activity.h>
#include <kerninc/Depend.h>
#include <kerninc/Invocation.h>
#include <kerninc/IRQ.h>
#include <kerninc/ObjectCache.h>

#define PREPDEBUG

#include <eros/Invoke.h>

/*#include <disk/DiskNode.hxx>*/
     
void
node_ClearHazard(Node* thisPtr, uint32_t ndx)
{
  if (keyBits_IsHazard(&thisPtr->slot[ndx]) == false)
    return;

  /* Could be processes blocked on a wrapper node: */
  if (ndx == WrapperFormat)
    sq_WakeAll(ObjectStallQueueFromObHdr(&thisPtr->node_ObjHdr), false);

  switch(thisPtr->node_ObjHdr.obType) {
  case ot_NtUnprepared:
      /* If this is read hazard, the world is in a very very
       * inconsistent state.
       */
    if (ndx != WrapperFormat)
      fatal("Unprepared Node 0x%08x%08x Corrupted (slot %d).\n",
	    (uint32_t) (thisPtr->node_ObjHdr.oid>>32), 
	    (uint32_t) thisPtr->node_ObjHdr.oid, ndx);

    keyBits_UnHazard(&thisPtr->slot[ndx]);
    break;
    
  case ot_NtSegment:
    if ( keyBits_IsRdHazard(&thisPtr->slot[ndx]) )
      fatal("Segment Node Corrupted!\n");

    Depend_InvalidateKey(&thisPtr->slot[ndx]);
    keyBits_UnHazard(&thisPtr->slot[ndx]);
    break;
  case ot_NtKeyRegs:
    /* If this is write hazard, the world is in a very very
     * inconsistent state.
     */
    proc_FlushKeyRegs(thisPtr->node_ObjHdr.prep_u.context);
    break;
      
  case ot_NtProcessRoot:
    /* TRY to Flush just the registers back out of the context
     * structure to clear the write hazard.  That is the common case,
     * and the less we flush the happier we will be:
     */

    if (ndx == ProcAddrSpace) {
      Depend_InvalidateKey(&thisPtr->slot[ndx]);
      keyBits_UnHazard(&thisPtr->slot[ndx]);
    }
    else if ( ndx == ProcGenKeys ) {
      /* This hazard exists to ensure that the domain remains well
       * formed.  In order to clear it we must decache the entire
       * domain from the context cache and revert this domain back to
       * the NtUnprepared form.  We do NOT need to unprepare the
       * auxiliary nodes.
       */

      node_Unprepare(thisPtr, true);
      assert( thisPtr->node_ObjHdr.obType == ot_NtUnprepared );
      assert( keyBits_IsHazard(&thisPtr->slot[ProcGenKeys]) == false );
    }
    else {
      /* FIX: If the slot is not smashed with a number key... */
      proc_FlushProcessSlot(thisPtr->node_ObjHdr.prep_u.context, ndx);
    }
    break;
  default:
    fatal("Clear hazard on unknown type\n");
    break;
  }

  if (keyBits_IsHazard(&thisPtr->slot[ndx]) != false)
    fatal("Error. Node ty=%d slot=%d still hazarded\n",
		  thisPtr->node_ObjHdr.obType, ndx);
}

/* CAREFUL -- this operation can have the side effect of blowing away
 * the current thread!
 */
void
node_DoClearThisNode(Node* thisPtr)
{
  uint32_t k = 0;

  assert (InvocationCommitted);
  
  node_Unprepare(thisPtr, true);

  for (k = 0; k < EROS_NODE_SIZE; k++) {
    assert (keyBits_IsHazard(&thisPtr->slot[k]) == false); /* node is unprepared! */
    key_NH_SetToVoid(&thisPtr->slot[k]);
  }

  /* FIX: Not sure this is really necessary, but I think it is a good
   * idea.
   */
  objH_InvalidateProducts(DOWNCAST(thisPtr, ObjectHeader));

#ifdef DBG_WILD_PTR
  if (dbg_wild_ptr)
    check_Consistency("DoClearThisNode");
#endif
}

/* UNPREPARE AND RESCIND (and object relocation) OF HAZARDED SLOTS.
 * 
 * The catch is that we don't want to unload the current (or invokee)
 * contexts unnecessarily.  If this slot contains a key that chances
 * to be in a capability register of the current (invokee) context,
 * clearing the hazard will have the effect of rendering the current
 * (invokee) context unrunnable by virtue of having forced an unload
 * of the capability registers node.
 * 
 * If the capability that we are depreparing/rescinding is in a memory
 * context, then we really do need to clear the hazard in order to
 * force the corresponding PTEs to get invalidated.  If, however, the
 * capability that we are clearing is in a NtKeyRegs node, we can
 * cheat.  Ultimately, this capability will get overwritten anyway,
 * and if the capability in the actual context structure is still the
 * same as this one, it will be getting unprepared/rescinded as well
 * as we traverse the key ring.
 * 
 * Note that the last is true only because this function is called
 * ONLY from within KeyRing::RescindAll or KeyRing::UnprepareAll
 * (according to operation).
 */
void
node_UnprepareHazardedSlot(Node* thisPtr, uint32_t ndx)
{
  Key* key /*@ not null @*/ = &thisPtr->slot[ndx];
  
  assert(keyBits_IsPrepared(key));
  
  if (thisPtr->node_ObjHdr.obType == ot_NtKeyRegs) {
    uint8_t oflags = key->keyFlags;

    /* TEMPORARILY clear the hazard: */
    key->keyFlags &= ~KFL_HAZARD_BITS;

    key_NH_Unprepare(key);
    
    /* RESET the hazard: */
    key->keyFlags = oflags;
  }
  else {
    node_ClearHazard(thisPtr, ndx);
    
    key_NH_Unprepare(key);
  }

#ifdef OPTION_OB_MOD_CHECK
  if (!objH_IsDirty(DOWNCAST(thisPtr, ObjectHeader)))
    thisPtr->node_ObjHdr.check = objH_CalcCheck(DOWNCAST(thisPtr, ObjectHeader));
#endif
}

/* NOTE: Off the top of my head, I can think of no reason why this
 * capability should NOT be invalidated in place.  The mustUnprepare flag
 * gets set when the object in question already has on-disk
 * capabilities, but what relevance this carries for in-place
 * capabilities is decidedly NOT clear to me.
 */
void 
node_RescindHazardedSlot(Node* thisPtr, uint32_t ndx, bool mustUnprepare)
{
  Key* key /*@ not null @*/ = &thisPtr->slot[ndx];
  
  assert(keyBits_IsPrepared(key));
  
  if (thisPtr->node_ObjHdr.obType == ot_NtKeyRegs) {
    uint8_t oflags = key->keyFlags;

    /* TEMPORARILY clear the hazard: */
    key->keyFlags &= ~KFL_HAZARD_BITS;

    if (mustUnprepare)
      key_NH_Unprepare(key);
    else
      key_NH_RescindKey(key);
    
    /* RESET the hazard: */
    key->keyFlags = oflags;
  }
  else {
    node_ClearHazard(thisPtr, ndx);

    if (mustUnprepare)
      key_NH_Unprepare(key);
    else
      key_NH_RescindKey(key);
  }
  
#ifdef OPTION_OB_MOD_CHECK
  if (!objH_IsDirty(DOWNCAST(thisPtr, ObjectHeader)))
    thisPtr->node_ObjHdr.check = objH_CalcCheck(DOWNCAST(thisPtr, ObjectHeader));
#endif
}

#if 0
void
Node::ObMovedHazardedSlot(uint32_t ndx, ObjectHeader *pNewLoc)
{
  Key& key = slot[ndx];
  
  assert(key.IsPrepared());
  
  ClearHazard(ndx);

  key.ok.pObj = pNewLoc;

#ifdef OPTION_OB_MOD_CHECK
  if (!IsDirty())
    ob.check = CalcCheck();
#endif
}
#endif

Process *
node_GetDomainContext(Node* thisPtr)
{
  if (thisPtr->node_ObjHdr.obType == ot_NtProcessRoot && thisPtr->node_ObjHdr.prep_u.context)
    return thisPtr->node_ObjHdr.prep_u.context;

  node_PrepAsDomain(thisPtr);

  proc_Load(thisPtr);


#if 0
  printf("return ctxt 0x%08x\n", context);
#endif
  return thisPtr->node_ObjHdr.prep_u.context;
}

/* The domain preparation logic has several possible outcomes:
 * 
 * 1. It succeeds - all is well, and a node pointer is returned.
 * 
 * 2. It must be retried because I/O is required, in which case it
 * initiates the I/O on behalf of the calling thread and causes a new
 * thread to be scheduled, resulting in an unwind.
 * 
 * 3. It fails because the domain is malformed.  In this case, it
 * endeavours to run the keeper of the domain that could not be
 * prepared, if any.  There may not (transitively) be one to run, in
 * which case we end up with a thread occupying a busted domain, Stick
 * the thread on the busted domain stall queue, and force a
 * reschedule.
 */

/* PrepAsDomain marks the domain components dirty as a side effect.
 * Strictly speaking, this is not necessary, but the number of domains
 * that get prepared without modification closely approximates 0, and
 * it's easier to do it here than in the context prepare logic.  Also,
 * doing the marking here makes it unnecessary to test in the message
 * transfer path.
 */

void
node_PrepAsDomain(Node* thisPtr)
{
  if (thisPtr->node_ObjHdr.obType == ot_NtProcessRoot)
    return;

  node_MakeDirty(thisPtr);

  node_Unprepare(thisPtr, false);

  thisPtr->node_ObjHdr.obType = ot_NtProcessRoot;
  thisPtr->node_ObjHdr.prep_u.context = 0;
}

bool
node_PrepAsDomainSubnode(Node* thisPtr, ObType nt, Process *ctxt)
{
  uint32_t i = 0;

  assert(objH_IsDirty(DOWNCAST(thisPtr, ObjectHeader)));

#if 0
  dprintf(false, "Preparing OID=0x%08x%08x as domsubnode ty %d\n",
		  (uint32_t) (oid>>32), (uint32_t) oid, nt);
#endif
  
  if (nt == thisPtr->node_ObjHdr.obType && thisPtr->node_ObjHdr.prep_u.context == ctxt)
    return true;

  if (nt != ot_NtUnprepared)
    if (!node_Unprepare(thisPtr, false))
      return false;

  if (nt == ot_NtRegAnnex) {
    for (i = 0; i < EROS_NODE_SIZE; i++) {
      if ( key_PrepareWithType(&thisPtr->slot[i], KKT_Number) == false )
	return false;
    }
  }

  thisPtr->node_ObjHdr.obType = nt;
  thisPtr->node_ObjHdr.prep_u.context = ctxt;

  return true;
}

void
node_SetSlot(Node* thisPtr, int ndx, Node* node, uint32_t otherSlot)
{
  assert (InvocationCommitted);
  
  assert (objH_IsDirty(DOWNCAST(thisPtr, ObjectHeader)));
  node_ClearHazard(thisPtr, ndx);


  /* If writing a non-number key into a general registers node, domain
   * must be deprepared.
   */
 
  if (thisPtr->node_ObjHdr.obType == ot_NtRegAnnex &&
      keyBits_GetType(&node->slot[ndx]) != KKT_Number)
    node_Unprepare(thisPtr, false);
 

  if ( keyBits_IsRdHazard(node_GetKeyAtSlot(node, otherSlot) ))
    node_ClearHazard(node, otherSlot);

  /* hazard has been cleared */
  key_NH_Set(&thisPtr->slot[ndx], node_GetKeyAtSlot(node, otherSlot));
  assert ( keyBits_IsHazard(&thisPtr->slot[ndx]) == false );
}

bool
node_PrepAsSegment(Node* thisPtr)
{
  uint8_t ot;

  assert(thisPtr);
  if (thisPtr->node_ObjHdr.obType == ot_NtSegment)
    return true;
	  
#if 0
  printf("Preparing oid=");
  print(oid);
  printf(" as segment node\n");
#endif

  ot = thisPtr->node_ObjHdr.obType;
  
  if(!node_Unprepare(thisPtr, false)) {
    /* FIX: this is temporary! */
    fatal("Couldn't unprepare oid 0x%08x%08x. Was ot=%d\n",
          (uint32_t) (thisPtr->node_ObjHdr.oid>>32), 
          (uint32_t) thisPtr->node_ObjHdr.oid, ot);
    return false;
  }

  thisPtr->node_ObjHdr.obType = ot_NtSegment;
  thisPtr->node_ObjHdr.prep_u.products = 0;
  thisPtr->node_ObjHdr.ssid = 0;	/* machine-dependent! */

  return true;
}

inline bool
node_IsCurrentDomain(Node* thisPtr)
{
  /* Note we do NOT use curcontext(), as there is
   * presently an assert in Activity.hxx, and if we are trying to
   * prepare a new thread to run (ageing can be called from trying
   * to prepare a thread, which can call us), the current thread may
   * indeed not have a context.
   */

  /* Thread::Current() changed to act_Current() */
  if (thisPtr->node_ObjHdr.prep_u.context && thisPtr->node_ObjHdr.prep_u.context == act_Current()->context)
    return true;

  return false;
}

bool
node_Unprepare(Node* thisPtr, bool zapMe)
{
  uint32_t k = 0;

  if (thisPtr->node_ObjHdr.obType == ot_NtUnprepared)
    return true;

  if (thisPtr->node_ObjHdr.obType == ot_NtProcessRoot) {
    /* First check to make sure we don't deprepare ourselves if we
     * shouldn't.
     */

    if (zapMe == false && node_IsCurrentDomain(thisPtr)) {
      dprintf(true, "(0x%08x) Domroot 0x%08x%08x no zapme\n",
              thisPtr,
              (uint32_t) (thisPtr->node_ObjHdr.oid >> 32), 
              (uint32_t) thisPtr->node_ObjHdr.oid);
      return false;
    }

      
    /* Invalidate mapping dependencies on the address space slot: */
    Depend_InvalidateKey(&thisPtr->slot[ProcAddrSpace]);

    if (thisPtr->node_ObjHdr.prep_u.context)
      proc_Unload(thisPtr->node_ObjHdr.prep_u.context);
    assert (thisPtr->node_ObjHdr.prep_u.context == 0);
  }
  else if (thisPtr->node_ObjHdr.obType == ot_NtKeyRegs || thisPtr->node_ObjHdr.obType == ot_NtRegAnnex) {
    /* First check to make sure we don't deprepare ourselves if we
     * shouldn't.
     */
#if 1

    if (thisPtr->node_ObjHdr.prep_u.context && inv_IsActive(&inv) && thisPtr->node_ObjHdr.prep_u.context == inv.invokee) {
      dprintf(true, "zapping keys/annex of invokee nd=0x%08x"
		      " ctxt=0x%08x\n", thisPtr, thisPtr->node_ObjHdr.prep_u.context);
    }

#endif


    if (zapMe == false && node_IsCurrentDomain(thisPtr)) {
      dprintf(true, "(0x%08x) keys/annex 0x%08x%08x no zapme\n",
		      thisPtr,
		      (uint32_t) (thisPtr->node_ObjHdr.oid >> 32),
                      (uint32_t) thisPtr->node_ObjHdr.oid);
      return false;
    }

      

    if (thisPtr->node_ObjHdr.prep_u.context)
      proc_Unload(thisPtr->node_ObjHdr.prep_u.context);
    assert (thisPtr->node_ObjHdr.prep_u.context == 0);
  }
  else if (thisPtr->node_ObjHdr.obType == ot_NtSegment) {
#ifdef DBG_WILD_PTR
    if (dbg_wild_ptr)
      if (check_Contexts("pre key zap") == false)
	halt('a');
#endif

    for (k = 0; k < EROS_NODE_SIZE; k++) {
      if ( keyBits_IsHazard(&thisPtr->slot[k]) ) {
	assert ( keyBits_IsType(&thisPtr->slot[k], KKT_Void) == false );

	Depend_InvalidateKey(&thisPtr->slot[k]);
	keyBits_UnHazard(&thisPtr->slot[k]);
      }
    }

#ifdef DBG_WILD_PTR
    if (dbg_wild_ptr)
      if (check_Contexts("post key zap") == false)
	halt('b');
#endif
  
    objH_InvalidateProducts(DOWNCAST(thisPtr, ObjectHeader));
  }

  thisPtr->node_ObjHdr.obType = ot_NtUnprepared;
#if 0
  dprintf(true, "Node deprepared okay\n");
#endif
  return true;
}

bool
node_Validate(Node* thisPtr)
{
  uint32_t i = 0;
  uint32_t k = 0;

  if ( thisPtr->node_ObjHdr.obType > ot_NtLAST_NODE_TYPE) {
    printf("Node 0x%08x has bad object type\n", thisPtr);
    return false;
  }
  assert (thisPtr->node_ObjHdr.obType <= ot_NtLAST_NODE_TYPE);
  
  if (thisPtr->node_ObjHdr.obType == ot_NtFreeFrame) {
    for (i = 0; i < EROS_NODE_SIZE; i++) {
      if (keyBits_IsUnprepared(&thisPtr->slot[i]) == false) {
	dprintf(true, "Free node 0x%08x has prepared slot %d\n",
			thisPtr, i);
	return false;
      }
    }

    return true;
  }
  
#ifndef NDEBUG
  if (keyR_IsValid(&thisPtr->node_ObjHdr.keyRing, thisPtr) == false)
    return false;
#endif
  
#ifdef OPTION_OB_MOD_CHECK
  if (objH_IsDirty(DOWNCAST(thisPtr, ObjectHeader)) == false) {
    uint32_t chk = objH_CalcCheck(DOWNCAST(thisPtr, ObjectHeader));
  
    if ( thisPtr->node_ObjHdr.check != chk ) {
      printf("Invalid Frame 0x%08x Chk=0x%x CalcCheck=0x%x on node ",
		     thisPtr, thisPtr->node_ObjHdr.check, chk);
      printOid(thisPtr->node_ObjHdr.oid);
      printf("\n");

#if 0
      for (uint32_t i = 0; i < EROS_NODE_SIZE; i++)
	slot[i].Print();
#endif

      return false;
    }
  }  
#endif

  for (k = 0; k < EROS_NODE_SIZE; k++) {
    /* const Key& key = thisPtr->slot[k]; */
    Key* key /*@ not null @*/ = &thisPtr->slot[k];

#ifndef NDEBUG
    if (key_IsValid(key) == false) {
      printf("Key %d is bad in node 0x%x\n", k, thisPtr);
      key_Print(key);
      return false;
    }
#endif
    
    if (keyBits_IsHazard(key) && k != WrapperFormat &&
	thisPtr->node_ObjHdr.obType == ot_NtUnprepared) {
      printf("Unprepared node contains hazarded key\n");
      return false;
    }

    /* For now, do not check device keys. */
    if ( keyBits_IsObjectKey(key) == false )
      continue;
  }

  return true;
}
