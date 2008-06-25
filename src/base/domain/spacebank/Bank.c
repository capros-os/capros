/*
 * Copyright (C) 1998, 1999, 2001, Jonathan Adams.
 * Copyright (C) 2007, 2008, Strawberry Development Group.
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


#include <stdlib.h>
#include <eros/target.h>
#include <eros/Invoke.h>

#include <idl/capros/key.h>
#include <idl/capros/Process.h>
#include <idl/capros/Range.h>
#include <idl/capros/Forwarder.h>

#include <domain/domdbg.h>
#include <domain/Runtime.h>

#include <string.h>

#include "misc.h"
#include "assert.h"
#include "spacebank.h"
#include "Bank.h"
#include "ObjSpace.h"
#include "malloc.h"
#include "debug.h"

Bank bank0;
Bank primebank;
Bank verifybank;

/* The keys given out to everyone before the spacebank starts
   running are readonly red segment key with a "bank address"
   of zero and a start key to the spacebank as the keeper.  A node
   key to the each of the premade red segments are passed in on
   startup.
   
   **DANGER** WILL ROBINSON: Every preallocated node MUST have
   the BANKPREC_NO_DELETE property, since either:
   
   A:  It is the primebank and therefore can never to deleted
   or
   B:  The preallocated segment is marked as allocated from the
      primebank, so trying to remove its key from the alloctree
      fails. */

struct premade_node_info {
  uint8_t keyReg;        /* Keyreg of preallocated node
		       *  -- KR_VOID indicates end of list
		       */
  Bank *bank;         /* Pointer to bank the preallocated node should
		       * point to.
		       */
  BankPrecludes limits;  /* Limits this key should have */
};

/* THIS MUST MATCH primebank.map!!! */
static struct premade_node_info premade_keys[] =
{
  {KR_PRIMEBANK, &primebank,BANKPREC_NO_DESTROY|
                       BANKPREC_NO_LIMITMODS},
  {KR_VERIFIER, &verifybank,BANKPREC_NO_DESTROY|
                        BANKPREC_NO_LIMITMODS},
  /* end marker */
  {KR_VOID, NULL, 0u}
};

static Bank* alloc_bank(void);
static void free_bank(Bank *);

static uint32_t
bank_containsOID(Bank *bank, uint8_t type, OID oid);

void
bank_initializeBank(Bank *bank,
		    Bank *parent,
		    uint64_t limit);
/* bank_initializeBank:
 *     Initializes /bank/ to be an empty bank with parent /parent/ and
 *   initial allocation limit /limit/.
 *
 *     Always succeeds.
 */

struct Bank_MOFrame *
bank_getTypeFrame(Bank *bank, uint8_t type);
/*    If /type/ is a multiple-object-per-frame type, returns the
 *  address of /bank/'s frameMap and frameOid cache for that type
 *  in *(/frameMap/) and *(/frameOid/), respectively.
 *    If /type/ is a single-object-per-frame type, returns NULL.
 *
 *    Implemented as a macro for speed.
 */
#define bank_getTypeFrame(bank, type) \
   (((type) == capros_Range_otNode)? (&(bank)->nodeFrame) \
      : (struct Bank_MOFrame *)NULL)

static uint32_t
bank_initKeyNode(uint32_t krNode, Bank *bank, BankPrecludes limits);
/* bank_initKeyNode:
 *     Takes the node /krNode/ and sets it up as a key to /bank/ with
 *   the restrictions outlined in /limits/.
 *
 *     If /krNode/ is already being used as a the node for a red
 *   segment key to a bank, bank_initKeyNode makes sure it remains
 *   valid while being changed.
 *
 *     Returns RC_OK on success, other on failure.
 */
     
static uint32_t
bank_initKeyNode(uint32_t krForwarder, Bank * bank, BankPrecludes limits)
{
  uint32_t retval;
  uint32_t oldWord;

  assert(limits < BANKPREC_NUM_PRECLUDES); /* make sure limits is valid */

  retval = capros_Forwarder_swapDataWord(krForwarder,
             (uint32_t)bank,	// pointer to /bank/
             &oldWord);
  assert ( "writing word into forwarder" && retval == RC_OK );

  /* use my domain key to make a start key with keyInfo /limits/ */
  retval = capros_Process_makeStartKey(KR_SELF, limits, KR_TMP);
  assert ( "making start key" && retval == RC_OK );

  retval = capros_Forwarder_swapTarget(krForwarder, KR_TMP, KR_VOID);
  assert ( "swapping start key into forwarder" && retval == RC_OK );

  return RC_OK;
}

void
bank_initializeBank(Bank *bank,
		    Bank *parent,
		    uint64_t limit)
{
  uint32_t i;
#ifdef PARANOID
  if (parent == NULL && bank != &bank0) {
    kpanic(KR_OSTREAM,
	   "Spacebank: bank_initializeBank: attempt to initialize a "
	   "bank with no parent!\n");
  }
#endif
  
  bank->parent = parent;
  if (parent) {
    bank->nextSibling = parent->firstChild;
    parent->firstChild = bank;
  } else {
    bank->nextSibling = NULL;
  }

  bank->limit = limit;
  bank->allocCount = 0;

  for (i = 0; i < BANKPREC_NUM_PRECLUDES; i++)
    bank->exists[i] = false;
  
  allocTree_Init(&bank->allocTree);
  bank->nodeFrame.frameMap = 0u;
  bank->nodeFrame.frameOid = 0ull;
  
  for (i = 0; i < capros_Range_otNUM_TYPES; i++) {
    bank->allocs[i]   = 0u;
    bank->deallocs[i] = 0u;
  }
  return;
}
			 
void
bank_init(void)
{
  struct premade_node_info *curInfo;

  /* first, initialize the precreated banks */
  bank_initializeBank(&bank0,      /* bank0 */
		      NULL,        /* bank0 has no parent */
		      0);          /* bank0 starts with a 0 limit */
    

  bank_initializeBank(&primebank,   /* primebank */
		      &bank0,	    /* primebank's parent is bank0 */
		      UINT64_MAX);  /* primebank is not limited */
    
  bank_initializeBank(&verifybank, /* verifybank */
		      &primebank,  /* verifybank's parent is primebank */
		      0);          /* verifybank cannot be allocated
				      from. */

  /* See the comment by the premade_keys stuff for info on premade 
     keys.  We need to identify the oid of each of the premade key's
     associated node and store it in the premade keys's associated
     bank's limitedKey array.  We then need to update the node to
     hold the correct Bank  address and permissions using
     bank_initKeyNode. */


  for (curInfo = premade_keys;
         curInfo->keyReg != KR_VOID;
           curInfo++) {
    
    uint64_t offset;
    capros_Range_obType obType;
    uint32_t result;
    
    /* identify the node and shove it's OID into the bank's
       limitedKey array. */
    result = capros_Range_identify(KR_SRANGE,
				 curInfo->keyReg,
				 &obType,
				 &offset);
    
    if (result != RC_OK) {
      kpanic(KR_OSTREAM,
	     "Spacebank: Hey! error identifying preall node in slot "
	     "%u! (0x%08x)\n",
	     curInfo->keyReg,
	     result);
    }
    if (obType != capros_Range_otForwarder) {
      kpanic(KR_OSTREAM,
	     "Spacebank: Hey! preall key in slot %u is not a forwarder! "
	     "(is 0x%08x)\n",
	     curInfo->keyReg,
	     obType);
    }
    
    /* put it in as the correct limited key */
    curInfo->bank->limitedKey[curInfo->limits] = offset;
    curInfo->bank->exists[curInfo->limits] = true;
    
    /* now init the node correctly */
    result = bank_initKeyNode(curInfo->keyReg,
			      curInfo->bank,
			      curInfo->limits);
    
    if (result != RC_OK) {
      kpanic(KR_OSTREAM,
	     "Spacebank: Hey! bank_initKeyNode failed for the preall "
	     "key in slot %u! (0x%08x)\n",
	     curInfo->keyReg,
	     result);
    }
    
    /* The limitedkeys are included in the primebank's preallocated
       storage */
  }
}

uint32_t
bank_ReserveFrames(Bank *bank, uint32_t count)
{
  Bank *curBank = bank;

  DEBUG(limit)
    kprintf(KR_OSTREAM,
	    "SpaceBank: bank_ReserveFrames reserve %d frames for bank 0x%x\n",
	    count,
	    bank);
  /* Loop up through bank0 (which has a NULL parent), adding
   * /count/ from the bank's allocCount if that doesn't go over limit.
   *
   * If we will go over the limit, we have to go back and undo all the
   * additions we did.
   */
  for (curBank = bank; curBank != NULL; curBank = curBank->parent) {
    DEBUG(limit)
      kprintf(KR_OSTREAM,
	      "SpaceBank: bank 0x%x "
	      "limit=0x%llx allocCount=0x%llx\n",
	      bank, curBank->allocCount, curBank->limit);
    
    if (curBank->allocCount + count > curBank->limit)
      break;
    curBank->allocCount += count;
  }

  if (curBank != NULL) {
    DEBUG(limit)
      kdprintf(KR_OSTREAM,
               "SpaceBank: failed to reserve objects\n");
    
    /* we didn't make it to NULL -- loop bank up to curBank,
       unreserving as we go*/
    for ( ; bank != curBank; bank = bank->parent) {
      bank->allocCount -= count;
    }
    /* return failure */
    return RC_capros_SpaceBank_LimitReached;
  }
  return RC_OK;
}

void
bank_UnreserveFrames(Bank *bank, uint32_t count)
{
  Bank *curBank;

  DEBUG(limit)
    kprintf(KR_OSTREAM,
	    "SpaceBank: bank_UnreserveFrames unreserving %d frames "
	    "from bank 0x%08x\n",count, bank);

  for (curBank = bank; curBank != NULL; curBank = curBank->parent) {
    DEBUG(limit)
      kprintf(KR_OSTREAM,
              "SpaceBank: bank %08x limit=0x"DW_HEX" "
              "allocCount=0x"DW_HEX"\n",
              curBank,
              DW_HEX_ARG(curBank->limit),
              DW_HEX_ARG(curBank->allocCount));
#ifdef PARANOID
    if (curBank->allocCount < (uint64_t)count)
      kpanic(KR_OSTREAM,
             "SpaceBank: Unreserving made allocCount < 0!\n");
#endif
    curBank->allocCount -= (uint64_t)count;
  }
}

uint32_t
BankSetLimits(Bank * bank, const capros_SpaceBank_limits * newLimits)
{
  bank->limit = newLimits->frameLimit;
  return RC_OK;
}

uint32_t
BankGetLimits(Bank * bank, /*OUT*/ capros_SpaceBank_limits * getLimits)
{
  uint64_t effFrameLimit = UINT64_MAX;
  uint64_t effAllocLimit = UINT64_MAX;

  getLimits->frameLimit = bank->limit;
  getLimits->allocCount = bank->allocCount;

  for ( ; bank != NULL; bank = bank->parent) {
    if (bank->limit < effFrameLimit) {
      effFrameLimit = bank->limit;
    }

    if (bank->limit < bank->allocCount) {
      effAllocLimit = 0u;
    } else if (bank->limit - bank->allocCount < effAllocLimit) {
      effAllocLimit = bank->limit - bank->allocCount;
    }
  }
  getLimits->effFrameLimit = effFrameLimit;
  getLimits->effAllocLimit = effAllocLimit;

  {
    int i;
    for (i = 0; i < capros_Range_otNUM_TYPES; i++) {
      getLimits->allocs[i]   = bank->allocs[i];
      getLimits->reclaims[i] = bank->deallocs[i];
    }
  }

  return RC_OK;
}

result_t
GetCapAndRetype(capros_Range_obType obType,
  OID offset, cap_t kr)
{
  result_t retval;

  capros_Range_obType currentType;
  retval = capros_Range_getCap(KR_SRANGE, obType, offset,
             &currentType, kr);
  if (retval != RC_OK)
    return retval;
  if (currentType != capros_Range_otNone) {
    assert(currentType <= capros_Range_otNode);
    // Need to retype this frame.
    DEBUG(retype) kprintf(KR_OSTREAM, "Retyping oidOfs=%#llx, old=%d new=%d",
                          offset, currentType, obType);
    OID firstOffset = FrameToOID(OIDToFrame(offset));
    OID curOffset;
    ObCount maxCount = 0;
    int i;
    for (i = 0, curOffset = firstOffset;
         i < objects_per_frame[currentType];
         i++, curOffset++) {
      capros_Range_count_t allocationCount;
      retval = capros_Range_getFrameCounts(KR_SRANGE, curOffset,
                 &allocationCount);
      assert(retval == RC_OK);
      if (allocationCount > maxCount)	// take max
        maxCount = allocationCount;
    }

    retval = capros_Range_retypeFrame(KR_SRANGE,
               firstOffset, currentType, typeToBaseType[obType], maxCount);
    assert(retval == RC_OK);

    // getCap should now succeed with the right type:
    retval = capros_Range_getCap(KR_SRANGE, obType, offset,
             &currentType, kr);
    if (retval != RC_OK)
      return retval;
    assert(currentType == capros_Range_otNone);
  }
  return RC_OK;
}

uint32_t
BankCreateKey(Bank *bank, BankPrecludes limits, uint32_t kr)
{
  uint32_t retval = RC_OK;

  assert(limits < BANKPREC_NUM_PRECLUDES);
  
  if (bank->exists[limits]) {
    /* we've already fab'ed the node, just make a new forwarder key */
    
    retval = GetCapAndRetype(capros_Range_otForwarder,
			       bank->limitedKey[limits],
			       kr);
    if (retval != RC_OK) return retval;
  }
  else {
    OID oid;
    /* Fabricate a new node. */

    retval = BankAllocObject(bank, capros_Range_otForwarder, kr, &oid);

    if (retval != RC_OK)
      return retval;

    retval = bank_initKeyNode(kr, bank, limits);
    assert("calling bank_initKeyNode on alloced forwarder"
           && retval == RC_OK );

    bank->exists[limits] = true;
    bank->limitedKey[limits] = oid;
  }
  
  /* now we have everything set up -- make the segment key */
    
  /* FIX: This is wrong -- what about permissions? */

  retval = capros_Forwarder_getOpaqueForwarder(kr,
             capros_Forwarder_sendWord, kr);
  assert( retval == RC_OK );

  return RC_OK;    
}

uint32_t
BankCreateChild(Bank *parent, uint32_t kr)
{
  Bank *newBank = alloc_bank();
  uint32_t retval;
  
  if (!newBank) return RC_capros_SpaceBank_LimitReached;

  bank_initializeBank(newBank,     /* newBank */
		      parent,      /* newBank's parent is /parent/ */
		      UINT64_MAX); /* newBank starts out unlimited */

  retval = BankCreateKey(newBank, 0u /* no limits */, kr);

  if (retval != RC_OK) {
    /* Couldn't create the key -- gotta return the child */
    assert ( parent->firstChild == newBank );

    /* unlink the child */
    parent->firstChild = newBank->nextSibling;
    
    free_bank(newBank);
  }      
  return retval;
}

static void
FlushBankCache(Bank * bank)
{
  /* move the node/proc cache into the b-tree */
  if (bank->nodeFrame.frameMap) {
    int idx;

    /* deallocate any unused items */
    for (idx = 0; idx < objects_per_frame[capros_Range_otNode]; idx++) {
      if (bank->nodeFrame.frameMap & (1 << idx)) {
	allocTree_removeOID(&bank->allocTree,
			    bank,
			    capros_Range_otNode,
			    bank->nodeFrame.frameOid | idx);
      }
    }
    bank->nodeFrame.frameOid = 0llu;
    bank->nodeFrame.frameMap = 0u;
  }
}

static void
DestroyStorage(Bank * bank)
{
#ifdef NEW_DESTROY_LOGIC
  OID oid;
  uint8_t type;

  while(allocTree_findOID(&bank->allocTree, &oid, &type)) {
    uint32_t result;

    kprintf(KR_OSTREAM,
	    "Rescinding object (type %d) "
	    "oid 0x"DW_HEX"\n",
	    type,
	    DW_HEX_ARG(oid));

    result = range_getobjectkey(KR_SRANGE,
				type,
				oid,
				KR_TMP);

    if (result != RC_OK) {
      /* DEBUG(children) */
      kprintf(KR_OSTREAM,
	      "SpaceBank: Unable to create key to object "
	      "oid 0x"DW_HEX" (0x%08x)\n",
	      DW_HEX_ARG(oid),
	      result);
    }
	    
    /* It will do no harm to rescind it if we didn't get it */
    result = range_rescind(KR_SRANGE, KR_TMP);

    if (result != RC_OK) {
      /* DEBUG(children) */
      kprintf(KR_OSTREAM,
	      "SpaceBank: Unable to rescind key to object "
	      "oid 0x"DW_HEX" (0x%08x)\n",
	      DW_HEX_ARG(oid),
	      result);
    }
	    
    result = allocTree_removeOID(&bank->allocTree,
				 bank,
				 type,
				 oid);

    if (result == 0) {
      /* failed */
      /* DEBUG(children) */
      kprintf(KR_OSTREAM,
	      "SpaceBank: Unable to remove allocTree entry of "
	      "oid 0x"DW_HEX"\n",
	      DW_HEX_ARG(oid));
    }
  }
#else
  /* blow it all away */
  OID curFrame;

  /* use the incrementalDestroy to remove one frame at a time from
   * the tree
       */
  while (allocTree_IncrementalDestroy(&bank->allocTree,
				      &curFrame)) {
    uint32_t retVal;
    
    /* BIG UGLY HACK!
     *
     * This code takes advantage of the fact that if you ask for a key
     * of a different type than is in the frame, all of the objects in
     * the frame are rescinded.
     *
     * So by creating a key to the page for this frame, then
     * rescinding it, the code guarentees that no matter what the
     * frame was filled with, all of the objects in the frame are
     * rescinded. */
    // FIXME: the above is no longer true.
    // Try getting a page key. If succeed, rescind it.
    // If fail, we are told the type of the frame; rescind all objects in it.

    DEBUG(children)
      kprintf(KR_OSTREAM,
	      "DestroyStorage: Destroying frame 0x"DW_HEX"\n",
	      DW_HEX_ARG(curFrame));

    retVal = GetCapAndRetype(capros_Range_otPage, curFrame, KR_TMP);
    if (retVal != RC_OK) {
      kdprintf(KR_OSTREAM,
	       "DestroyStorage: Error getting page key to "
	       "0x"DW_HEX"\n",
	       DW_HEX_ARG(curFrame));
    }
    
    retVal = capros_Range_rescind(KR_SRANGE, KR_TMP);
    if (retVal != RC_OK) {
      kdprintf(KR_OSTREAM,
	       "DestroyStorage: Error rescinding page key to "
	       "0x"DW_HEX"\n",
	       DW_HEX_ARG(curFrame));
    }

    /* unreserve the frame */
    bank_UnreserveFrames(bank, 1u);
    /* now mark it free in the object space */

    /* SHAP: this is correct, but inefficient -- the objects properly
       ought to go back according to frame type, but the following
       will suffice until I have time to figure out what to do about
       that. */
    ob_ReleasePageFrame(bank,curFrame);
  }
#endif
}

uint32_t
BankDestroyBankAndStorage(Bank *bank, bool andStorage)
{
  /* the way this works is a depth-first deallocation: we first delete
   * (effectively recursively, actually in-place) all of the children
   * of a bank, then delete the bank itself.  Of course, if one of the
   * children of that bank has children, those get deleted first,
   * etc.  The deletion stops when we delete /bank/.
   *
   * NOTE that there is a perverse piece of nonsense that we are
   * taking advantage of here: the space occupied by the node(s) of a
   * given bank are part of that bank's allocated tree.
   */

#ifdef PARANOID
  if (bank == &primebank || bank == &bank0)
    kpanic(KR_OSTREAM, "spacebank: attempt to destroy primebank/bank0\n");
#endif

  DEBUG(children)
    kprintf(KR_OSTREAM,
	    "DestroyBank: Destroying bank 0x%08x%s\n",
	    bank,
	    (andStorage)?" and storage":" and returning storage to parent");

  /* destroy this bank and all of its children */
  {
    Bank *curBank = 0;
    Bank *parentBank = bank->parent;
    Bank *next;

    while (bank) {
      if (curBank == 0)
	curBank = bank;
    
      while (curBank->firstChild)
	curBank = curBank->firstChild;

      /* curbank now points to a leftmost child */
      next = curBank->nextSibling;

      FlushBankCache(curBank);

      /* rescind all outstanding red segment nodes that point to this
	 bank */
      {
	/* FIX: There is a HUGE problem here -- if the underlying
	   range is unavailable (e.g. dismounted) then this operation
	   will fail! */
	
	int i;
	for (i = 0; i < BANKPREC_NUM_PRECLUDES; i++) {
	  if (curBank->exists[i]) {
	    uint32_t result;
	    result = GetCapAndRetype(capros_Range_otNode,
				     curBank->limitedKey[i],
				     KR_TMP);

	    if (result != RC_OK) {
	      DEBUG(children)
		kprintf(KR_OSTREAM,
			"SpaceBank: Unable to create key to limited "
			"node %d oid 0x"DW_HEX" (0x%08x)\n",
			i,
			DW_HEX_ARG(curBank->limitedKey[i]),
			result);
	    }
	    
	    /* It will do no harm to rescind it if we didn't get it */
	    result = capros_Range_rescind(KR_SRANGE, KR_TMP);

	    if (result != RC_OK) {
	      DEBUG(children)
		kprintf(KR_OSTREAM,
			"SpaceBank: Unable to create key to limited "
			"node %d oid 0x"DW_HEX" (0x%08x)\n",
			i,
			DW_HEX_ARG(curBank->limitedKey[i]),
			result);
	    }
	    
	    result = allocTree_removeOID(&curBank->allocTree,
					 curBank,
					 capros_Range_otNode,
					 curBank->limitedKey[i]);

	    if (result == 0) {
	      /* failed */
	      DEBUG(children)
		kprintf(KR_OSTREAM,
			"SpaceBank: Unable to remove allocTree entry of "
			"node %d oid 0x"DW_HEX"\n",
			i,
			DW_HEX_ARG(curBank->limitedKey[i]));
	    }
	  }
	}
      }

      if (andStorage)
	DestroyStorage(curBank);
      else
	allocTree_mergeTrees(&parentBank->allocTree,&curBank->allocTree);

      /* unlink the bank we are blowing away */
      curBank->parent->firstChild = curBank->nextSibling;

      /* blow it away */
      free_bank(curBank);
      
      if (curBank == bank)
	bank = 0;
      
      if (next)
	curBank = next;
      else
	curBank = bank;
    }
  }
  
  return RC_OK;
}

uint32_t
BankDeallocObject(Bank * bank, uint32_t kr)
{
  OID oid;
  capros_Range_obType obType;
  uint32_t code;
    
  code = capros_Range_identify(KR_SRANGE, kr, &obType, &oid);
  if (code != RC_OK) {
    DEBUG(dealloc)
	kdprintf(KR_OSTREAM,
		 "SpaceBank: range_identify failed (%u)\n",
		 code);
    return code;
  }
    
  if (! bank_containsOID(bank, obType, oid)) {
    DEBUG (dealloc)
      kdprintf(KR_OSTREAM, "Bank does not contain %s 0x"DW_HEX"\n",
	       type_name(obType),
	       DW_HEX_ARG(oid));
    return RC_capros_Range_RangeErr;
  }
  /* It's ours */
  code = capros_Range_rescind(KR_SRANGE,kr);
  if (code != RC_OK) {
     return code; /* "not a strong key" */ 
  }
  if ( ! bank_deallocOID(bank, obType, oid)) {
     kpanic(KR_OSTREAM,
            "SpaceBank: Dealloc failed after contains succeeded!\n");
  }

  DEBUG(dealloc) {
    uint32_t result, nType;

    result = capros_key_getType(kr, &nType);

    if (result != RC_OK || nType != RC_capros_key_Void) {
      /* Didn't dealloc! */
      kpanic(KR_OSTREAM,
             "spacebank: rescind successful but new keytype not Number\n"
             "           (passed in type %s, new type %d, OID "
             "0x"DW_HEX")\n",
             obType,
             nType,
             DW_HEX_ARG(oid));
    }
  }

  return RC_OK;
}

uint32_t
BankAllocObject(Bank * bank, uint8_t type, uint32_t kr, OID * oidRet)
{
  OID oid;
  int retval = 0;
  uint32_t retVal;
  uint64_t newFrame;
  const uint8_t baseType = typeToBaseType[type];

  struct Bank_MOFrame * obj_frame = bank_getTypeFrame(bank, baseType);

  if (obj_frame) {
    if (obj_frame->frameMap == 0) {
      /* need to grab another frame. -- reserve it first */
      if (bank_ReserveFrames(bank, 1) != RC_OK) {
        DEBUG(nospace)
          kdprintf(KR_OSTREAM, "spacebank: can't reserve node frame.\n");
	goto cleanup;
      }
    
      DEBUG(alloc) kprintf(KR_OSTREAM, "spacebank: allocating new frame\n");
      assert(baseType == capros_Range_otNode);
      retVal = ob_AllocNodeFrame(bank, &newFrame);
      if (retVal != RC_OK) {
        DEBUG(nospace)
          kdprintf(KR_OSTREAM, "spacebank: can't alloc node frame.\n");
	goto cleanup;
      }

      /* got some space in newFrame */
      if (bank != &bank0)
        allocTree_insertOIDs(&bank->allocTree,
			   baseType,
			   newFrame,
			   objects_per_frame[baseType]);

      /* shove the frame into the cache */
      obj_frame->frameOid = newFrame;
      obj_frame->frameMap = objects_map_mask[baseType];
    }
    uint32_t offset = ffs(obj_frame->frameMap) - 1;
    oid = obj_frame->frameOid | offset;

    obj_frame->frameMap &= ~(1u << offset);

    DEBUG(alloc)
      kprintf(KR_OSTREAM,
		   "Allocated %s oid=0x%08x%08x. Map now 0x%08x\n",
		   type_name(type),
		   (uint32_t) (oid >> 32),
		   (uint32_t) oid,
		   obj_frame->frameMap);
  } else {	// no obj_frame
    /* single frame per object type -- preallocate the object */
    if (bank_ReserveFrames(bank, 1) != RC_OK) {
      DEBUG(nospace)
        kdprintf(KR_OSTREAM, "spacebank: can't reserve page frame.\n");
      goto cleanup;
    }
    
    DEBUG(alloc) kprintf(KR_OSTREAM, "spacebank: allocating new frame\n");
    assert(baseType == capros_Range_otPage);
    retVal = ob_AllocPageFrame(bank, &newFrame);
    if (retVal != RC_OK) {
      DEBUG(nospace)
        kdprintf(KR_OSTREAM, "spacebank: can't alloc page frame.\n");
      goto cleanup;
    }

    /* got some space in newFrame */
    if (bank != &bank0)
      allocTree_insertOIDs(&bank->allocTree,
			   baseType,
			   newFrame,
			   objects_per_frame[baseType]);

    oid = newFrame;
  }
  
  /* now create the key */

  DEBUG(alloc)
    kprintf(KR_OSTREAM,
	      "spacebank: getting object key "DW_HEX"\n",
	      DW_HEX_ARG(oid));
    
  retval = GetCapAndRetype(type, oid, kr);

  DEBUG(alloc)
    kprintf(KR_OSTREAM,
	    "spacebank: got back %x\n",
	    retval);
    
  if (retval != RC_OK) {
    /* ack! we've got to undo everything!
     * note that we don't need to rescind the key we made, since it
     * points to a zero object
     */
    bank_deallocOID(bank, baseType, oid);
    DEBUG(nospace)
      kdprintf(KR_OSTREAM, "spacebank: Range cap error %#x.\n", retval);
    goto cleanup;
  }

  bank->allocs[type] ++;
  *oidRet = oid;
  return RC_OK;

cleanup:
  return RC_capros_SpaceBank_LimitReached;
}


uint32_t
bank_containsOID(Bank *bank, uint8_t type, OID oid)
{
  struct Bank_MOFrame *obj_frame;

  /* valid key slot */
  uint64_t frameOff = EROS_FRAME_FROM_OID(oid);
  uint8_t subObj = OIDToObIndex(oid);

  obj_frame = bank_getTypeFrame(bank, type);

  if (obj_frame) {
    if (obj_frame->frameMap != 0u && obj_frame->frameOid == frameOff) {
      /* bit is set implies object is free */
      return !(obj_frame->frameMap & (1<<subObj));
    }
  }  
  return allocTree_checkForOID(&bank->allocTree,oid);
}

uint32_t
bank_deallocOID(Bank *bank, uint8_t type, OID oid)
{
  struct Bank_MOFrame *obj_frame;

  /* valid key slot */
  uint64_t frameOff = EROS_FRAME_FROM_OID(oid);
  uint8_t subObj = OIDToObIndex(oid);

  obj_frame = bank_getTypeFrame(bank, type);
  
  if (obj_frame) {
    if (obj_frame->frameMap != 0u && obj_frame->frameOid == frameOff) {

      if (obj_frame->frameMap & (1<<subObj))
	kpanic(KR_OSTREAM,
		 "Spacebank: (frame)dealloc passed already "
		 "unallocated object!\n");

      obj_frame->frameMap |= (1<<subObj); /* mark free */

      if (obj_frame->frameMap == objects_map_mask[type]) {
	/* now the frame is empty -- return it */
	uint32_t x;

	DEBUG(dealloc)
	  kprintf(KR_OSTREAM, "Returning empty %s frame\n",type_name(type));

	for (x = 0; x < objects_per_frame[type]; x++) {
	  allocTree_removeOID(&bank->allocTree,
			      bank,
			      type,
			      obj_frame->frameOid | x);
	  obj_frame->frameMap &= ~(1u << x);
	}
	/* no need to return the frame, as allocTree_removeOID takes
	   care of all of that. */
	obj_frame->frameOid = 0ull;
      }
      bank->deallocs[type]++;
      return 1;
    } /* if (obj_frame->frameMap != 0u && frameOid == frameOff) */
    
  } /* if (obj_frame) ... */

  /* not in the frame cache -- try the tree */
  if (! allocTree_removeOID(&bank->allocTree, bank, type, oid) ) {
    /* failed */
    kprintf(KR_OSTREAM,
	    "spacebank: oid 0x"DW_HEX" (%s) not in bank "
	    "to dealloc\n",
	    DW_HEX_ARG(oid),
	    type_name(oid)
	   );
    return 0;
  } else {
    bank->deallocs[type]++;
    return 1;
  }
}

void
BankPreallType(Bank *bank,
	       uint32_t type,
	       uint64_t startOID,
	       uint64_t number)
{
  OID oid = startOID;
  uint32_t residual;
  uint32_t i;  
  uint32_t obPerFrame = objects_per_frame[type];

  DEBUG(init)
    kprintf(KR_OSTREAM,
	    "Adding 0x%08x%08x %s\n",
	    (uint32_t) (number>>32),
	    (uint32_t) number,
	    type_name(type));

  residual = number;
    
  for (i = 0; i < number; i += obPerFrame, oid += EROS_OBJECTS_PER_FRAME) {
      
    uint32_t count = MIN(residual, obPerFrame);
    OID top = oid + obPerFrame;

    if (bank_ReserveFrames(bank, 1u) != RC_OK) {
      kpanic(KR_OSTREAM,
	     "SpaceBank: Hit limit while preallocating space!\n");
    }
    DEBUG(init)
      kprintf(KR_OSTREAM,
	       "Adding oids [0x"DW_HEX":0x"DW_HEX") resid %d count %d\n",
	       DW_HEX_ARG(oid),
	       DW_HEX_ARG(top),
	       residual,
	       count);
    
    allocTree_insertOIDs(&bank->allocTree, type, oid, obPerFrame);

    /* If last frame, set up residual in active alloc word
     * Test can only be true if obPerFrame > 1.
     */
    if (count && count < obPerFrame) {
      struct Bank_MOFrame *obj_frame;

      uint32_t obMask;
      
      /* get the type frame for the residual */
      obj_frame = bank_getTypeFrame(bank, type);
  
      if (!obj_frame) {
	/* not a recognized multi-page object */
	kpanic(KR_OSTREAM,
	       "Spacebank: Fatal error: bank_preall_type got "
	       "unsupported >1 per page type (%i)!\n",
	       type);
	       
	break; /* BREAK */
      }

      if (obj_frame->frameMap != 0u) {
	uint32_t x;
	
	/* deallocate old stuff */
	while (obj_frame->frameMap != 0u) {
	  x = ffs(obj_frame->frameMap);

	  allocTree_removeOID(&bank->allocTree,
			      bank,
			      type,
			      obj_frame->frameOid | x);
	  obj_frame->frameMap &= ~(1u << x);
	}
      }

      obMask = (1u << obPerFrame) - 1;

      obj_frame->frameOid = oid;

      /* now mark the first /count/ objects allocated */
      obj_frame->frameMap = ~( (1u << count) - 1 );
      /* mask out the extraneous bits */
      obj_frame->frameMap &= obMask;

      DEBUG(init)
	kdprintf(KR_OSTREAM,
		 "Residual %s frame: oid=0x"DW_HEX" map: 0x%08x\n",
		 type_name(type),
		 DW_HEX_ARG(obj_frame->frameOid),
		 obj_frame->frameMap);
    }
    residual -= count;
  }
  return;
}


static Bank *FreeBanks = 0;

Bank *
alloc_bank(void)
{
  Bank *newBank;
  
  if (FreeBanks) {
    newBank = FreeBanks;
    FreeBanks = *((Bank**) FreeBanks);
  }
  else
    newBank = (Bank *) malloc(sizeof(Bank));

  if (!newBank) return NULL;
  
  bzero(newBank, sizeof(Bank*));

  return newBank;
}

void
free_bank(Bank * b)
{
  *((Bank **)b) = FreeBanks;
  FreeBanks = b;
}
