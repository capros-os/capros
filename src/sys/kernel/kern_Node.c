/*
 * Copyright (C) 1998, 1999, 2001, Jonathan S. Shapiro.
 * Copyright (C) 2006, 2007, 2008, Strawberry Development Group.
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
#include <kerninc/GPT.h>
#include <kerninc/ObjH-inline.h>

#define PREPDEBUG

#include <eros/Invoke.h>

#ifdef OPTION_OB_MOD_CHECK
uint32_t
node_CalcCheck(const Node * pNode)
{
  unsigned int i;
  uint32_t ck = 0;
  
  // Compute XOR including allocation count, call counts, and key slots.

  ck ^= objH_GetAllocCount(&pNode->node_ObjHdr);
  ck ^= node_GetCallCount(pNode);

  for (i = 0; i < EROS_NODE_SIZE; i++)
    ck ^= key_CalcCheck(&pNode->slot[i]);

  return ck;
}
#endif

void
node_ClearHazard(Node* thisPtr, uint32_t ndx)
{
  Key * pKey = &thisPtr->slot[ndx];

  if (! keyBits_IsHazard(pKey))
    return;

  switch(thisPtr->node_ObjHdr.obType) {
  case ot_NtSegment:
    if (keyBits_IsRdHazard(pKey))
      fatal("Segment Node Corrupted!\n");

    node_ClearGPTHazard(thisPtr, ndx);
    break;

  case ot_NtProcessRoot:
    /* TRY to Flush just the registers back out of the context
     * structure to clear the write hazard.  That is the common case,
     * and the less we flush the happier we will be:
     */

    if (ndx == ProcAddrSpace) {
      Depend_InvalidateKey(pKey);
    }
    else if ( ndx == ProcGenKeys ) {
  case ot_NtKeyRegs:
      proc_FlushKeyRegs(thisPtr->node_ObjHdr.prep_u.context);
    }
    else {
      /* FIX: If the slot is not smashed with a number key... */
      proc_FlushProcessSlot(thisPtr->node_ObjHdr.prep_u.context, ndx);
    }
    break;

  case ot_NtUnprepared:
  default:
    fatal("Clear hazard on unknown type\n");
    break;
  }

  assert(! keyBits_IsHazard(pKey));
}
     
void
node_ClearAllHazards(Node * thisPtr)
{
  unsigned int k;
  for (k = 0; k < EROS_NODE_SIZE; k++) {
    node_ClearHazard(thisPtr, k);
  }
}

bool
node_IsNull(Node * pNode)
{
  if (pNode->nodeData)
    return false;
  unsigned int i;
  for (i = 0; i < EROS_NODE_SIZE; i++) {
    node_ClearHazard(pNode, i);
    Key * key = node_GetKeyAtSlot(pNode, i);
    assert(! keyBits_IsHazard(key)); /* node is unprepared! */
    if (keyBits_GetType(key) != KKT_Void)
      return false;
  }
  return true;
}

// Caller must make the node dirty.
/* CAREFUL -- this operation can have the side effect of blowing away
 * the current thread!
 */
void
node_DoClearThisNode(Node* thisPtr)
{
  uint32_t k = 0;

  assert (InvocationCommitted);
  
  node_ClearAllHazards(thisPtr);

  for (k = 0; k < EROS_NODE_SIZE; k++) {
    assert (keyBits_IsHazard(&thisPtr->slot[k]) == false); /* node is unprepared! */
    key_NH_SetToVoid(&thisPtr->slot[k]);
  }
  thisPtr->nodeData = 0;

  /* FIX: Not sure this is really necessary, but I think it is a good
   * idea.
   */
  objH_InvalidateProducts(DOWNCAST(thisPtr, ObjectHeader));

#ifdef DBG_WILD_PTR
  if (dbg_wild_ptr)
    check_Consistency("DoClearThisNode");
#endif
}

static void
node_DoPrepareProcess(Node * pNode)
{
  assert(pNode);

  /* If it is prepared as anything else, the space bank or process creator
  messed up. */
  assert(pNode->node_ObjHdr.obType == ot_NtUnprepared);

  /* We mark the domain components dirty as a side effect.
   * Strictly speaking, this is not necessary, but the number of domains
   * that get prepared without modification closely approximates 0, and
   * it's easier to do it here than in the context prepare logic.  Also,
   * doing the marking here makes it unnecessary to test in the message
   * transfer path.
   */

  node_EnsureWritable(pNode);

  Process * p = proc_allocate(true);

  p->procRoot = pNode;

  pNode->node_ObjHdr.prep_u.context = p; 
  pNode->node_ObjHdr.obType = ot_NtProcessRoot;
}

Process *
node_GetProcess(Node * pNode)
{
  if (pNode->node_ObjHdr.obType != ot_NtProcessRoot)
    node_DoPrepareProcess(pNode);

  assert(pNode->node_ObjHdr.prep_u.context);

  return pNode->node_ObjHdr.prep_u.context;
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

void
node_SetSlot(Node * thisPtr, uint32_t slot, Invocation * inv)
{
  node_EnsureWritable(thisPtr);

  COMMIT_POINT();
  
  /* Following will not cause dirty node because we forced it
   * dirty above the commit point.
   */
  node_ClearHazard(thisPtr, slot);

  key_NH_Set(node_GetKeyAtSlot(thisPtr, slot), inv->entry.key[0]);

  inv->exit.code = RC_OK;
}

bool
node_PrepAsSegment(Node* thisPtr)
{
  assert(thisPtr);
  if (thisPtr->node_ObjHdr.obType == ot_NtSegment)
    return true;

  /* If it is prepared as anything else, the space bank or process creator
  messed up. */
  assert(thisPtr->node_ObjHdr.obType == ot_NtUnprepared);
	  
  thisPtr->node_ObjHdr.obType = ot_NtSegment;
  thisPtr->node_ObjHdr.prep_u.products = 0;

  return true;
}

inline bool
node_IsCurrentDomain(Node* thisPtr)
{
  /* Note proc_Current() may be NULL.  If we are trying to
   * prepare a new thread to run (ageing can be called from trying
   * to prepare a thread, which can call us), the current thread may
   * indeed not have a context.
   */

  if (thisPtr->node_ObjHdr.prep_u.context == proc_Current())
    return true;

  return false;
}

void
node_Unprepare(Node* thisPtr)
{
#ifndef NDEBUG
  unsigned int originalObType = thisPtr->node_ObjHdr.obType;
  assert(originalObType <= ot_NtLAST_NODE_TYPE);
#endif

  switch (thisPtr->node_ObjHdr.obType) {
  case ot_NtUnprepared:
    return;

  case ot_NtProcessRoot:
  case ot_NtKeyRegs:
  case ot_NtRegAnnex:
    proc_Unload(thisPtr->node_ObjHdr.prep_u.context);
    assert(thisPtr->node_ObjHdr.obType == ot_NtUnprepared);
    break;

  case ot_NtSegment:
    GPT_Unload(thisPtr);
    thisPtr->node_ObjHdr.obType = ot_NtUnprepared;
    break;

  default: ;
    assert(false);	// should not be trying to unprepare ot_NtFreeFrame
  } // end of switch on obType

#ifndef NDEBUG
  // All slots should now be unhazarded.
  unsigned int k;
  for (k = 0; k < EROS_NODE_SIZE; k++) {
    if (keyBits_IsHazard(node_GetKeyAtSlot(thisPtr, k))) {
      dprintf(true, "Hazard after unprepared! slot %d, ot=%d\n",
              k, originalObType);
    }
  }
#endif
}

/* Given that pKey is a valid key pointer,
 * if pKey is a pointer to a key in a node,
 * return a pointer to the node, else NULL. */
Node *
node_ContainingNodeIfNodeKeyPtr(const Key * pKey)
{
  char * wobj = (char *) pKey;
  char * wbase = (char *) objC_nodeTable;
  if (wobj < wbase)
    return NULL;
  if (wobj >= (char *) (objC_nodeTable + objC_nNodes))
    return NULL;

  // It's in the right range to be a pointer into a node.
  return objC_nodeTable + ((wobj - wbase) / sizeof(Node));
}

#ifndef NDEBUG
/* If pKey is a valid pointer to a key in a node,
 * return a pointer to the node, else NULL. */
Node *
node_ValidNodeKeyPtr(const Key * pKey)
{
  Node * pNode = node_ContainingNodeIfNodeKeyPtr(pKey);

  if (pNode) {
    // See if it's at a valid slot:
    ptrdiff_t delta = (char *)pKey - (char *)&pNode->slot[0];
    if (delta < 0			// in the header
        || delta % sizeof(Key))	// or not on a Key boundary
      return NULL;
  }

  return pNode;
}
#endif

Node *
node_ContainingNode(const Key * pKey)
{
  assert(node_ValidNodeKeyPtr(pKey));
  ptrdiff_t nchars = (char *)pKey - (char *)objC_nodeTable;
  return &objC_nodeTable[nchars/sizeof(Node)];
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
  if (objH_IsUnwriteable(node_ToObj(thisPtr))) {
    uint32_t chk = node_CalcCheck(thisPtr);
  
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
    
    if (keyBits_IsHazard(key) &&
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
