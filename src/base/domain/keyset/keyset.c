/*
 * Copyright (C) 1998, 1999, 2001, Jonathan S. Shapiro.
 * Copyright (C) 2007, Strawberry Development Group.
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

/* Keyset object

   Holds a logical "set" of keys.  This set supports the operations:

   Adding a key to the set
   Removing a key from the set
   Checking if a key is in the set
   Emptying the set
   **Count of number of items in the set?
   **Merging the keys from another set? -- this would seem to require
                                           some way of authorizing
					   the transfer.
   
   Note that there is *NO* way to get back a key after adding it to
   the set.  You have to keep a copy of it.

   For efficiency, keys are *not* actually even held by the keyset.
   Their keybits are stored in a balanced tree for rapid searching.

   For now, keyset is mainly an *internal* object, as there are
   problems with the way it stores the keys. ( it is possible to
   create and object, rescind it, then create another object, and test
   that the new object is in the keyset.  This is bad. )
   
   **longer note on dangers / what happens if a key is rescinded?
 
   ** Add a "readonly" key?
  
   NEED MORE DESCRIPTION */

#include <stddef.h>
#include <eros/target.h>
#include <eros/Invoke.h>
#include <domain/Runtime.h>
#include <eros/KeyConst.h>

#include <idl/capros/key.h>
#include <idl/capros/KeyBits.h>
#include <idl/capros/Discrim.h>
#include <idl/capros/SpaceBank.h>
#include <idl/capros/GPT.h>
#include <idl/capros/Node.h>
#include <idl/capros/Process.h>
#include <idl/capros/ProcCre.h>

#include <domain/ProtoSpace.h>
#include <domain/ConstructorKey.h>
#include <domain/KeySetKey.h>
#include <domain/Runtime.h>

#include <domain/domdbg.h>

#include "keyset.h"
#include "constituents.h"

#define Normal_KeyData     (0u)
#define ReadOnly_KeyData   (1u)
#define GetSegment_KeyData (0xFACEu)

#define RC_Internal_KeyToSelf  (0xFACEu)

#define OC_KeySet_Internal_GetSegment (0xFEEDFACEu)

/* NOTE:
 *   The "Internal" protocol goes like this:
 * 1 Someone requests that this keyset do some operation with another
 *  keyset.  They have passed in the key to the other set.
 *
 * 2 The called keyset checks the passed in key with its domain creator
 *  to make sure that it is a valid key to a keyset.
 *
 * 3 This keyset makes a start key with a distinguished KeyData field,
 *  to validate itself to the other keyset.  It then CALLs the other
 *  keyset, using a distinguished request code, and passing in the
 *  distinguished start key.  The first keyset is now Waiting for the
 *  other keyset's response.
 *
 * 4 The other keyset now has control.  It recognizes the request code,
 *  and uses its domain creator to validate the key's authenticity and
 *  KeyData. Note that from now forward, the second keyset is *NOT*
 *  Available, and any requests to it will block.
 *
 * 5 Having validated the first keyset, the second now CALLs the resume
 *  key to the first keyset, passing with it a read-only segment key to
 *  its keylist (which is sorted first).  The second keyset is now Waiting
 *  for the first keyset to finish reading its data.
 *
 * 6 The first keyset takes the passed in segment, and shoves it into
 *  its own address space.
 *
 * 7 The first keyset then does whatever processing of the other
 *  address space that is needed.
 *
 * 8 When it is done, it removes the segment from its address space and
 *  CALLs the resume key to the second segment.  The first keyset is now
 *  Waiting for the other keyset to finish cleanup.
 *
 * 9 The second process now cleans up and goes back into its main loop,
 *  RETURNing to the first process.  The second process is now
 *  Available for requests.
 *
 *10 The first process finishes its processing and returns its results
 *  to its caller, and also becomes Available for requests
 *
 *  There is a single complication to this, which is that if some
 * bonehead asks a keyset to do an operation with itself, the right
 * thing *MUST* happen.  We *CAN'T* use this protocol, since CALLing a
 * start key to yourself means you block forever.  The solution is we
 * have a function which checks if a key is a start key to ourselves,
 * and the functions have the job of checking this *BEFORE* they start
 * the protocol, and returning the correct thing if it is a key to us.
 *
 * The protocol is implemented in four functions, plus some handling
 * code in ProcessRequest:
 *
 * VerifyAndGetSegmentOfSet:  Does steps 2, 3, and 6 of the protocol.
 *                           Stores the Return key from the second
 *                           keyset in KR_SETRESUME.  Returns
 *                           RC_Internal_KeyToSelf if the key is to me.
 *
 *            SlaveProtocol:  Does steps 4, 5, and 9 of the protocol.
 *                           Stores the new resume key in KR_RESUME
 *                           for the final RETURN, which is done in
 *                           main.
 *
 *            ReturnSegment:  Calls KR_SETRESUM, doing step 8 of the
 *                           protocol.
 *
 */
 
struct KeySetStat STAT;


/* We set up our address space with three equal-sized regions:
  slot 0 is the code and data we were constructed with. */
#define OUR_TABLE_SLOT 1
#define OTH_TABLE_SLOT 2

/* Since we divide the address space into 4 slots (only 3 are used),
the number of bits of address for each slot is: */
#define SLOT_ADDRESS_BITS (CAPROS_FAST_SPACE_LGSIZE - 2)
/* Note, there is currently no code to check for overflowing this space. */

#define SLOT_TO_ADDR(slot) ((void *)(slot << SLOT_ADDRESS_BITS))

struct table_entry * const the_table   = SLOT_TO_ADDR(OUR_TABLE_SLOT);
/* ^^^ where we installed our VCS */

struct table_entry * const other_table = SLOT_TO_ADDR(OTH_TABLE_SLOT);
/* ^^^ where we store the other guy */

void require_sorted_table(void)
{
  if (STAT.numUnsorted) {
    STAT.numSorted += STAT.numUnsorted;
    STAT.numUnsorted = 0;

    sortTable(the_table, STAT.numSorted);
  }
}

void
teardown(void);

void
Initialize(void)
{
  uint32_t result;
  uint32_t keyType;

  capros_KeyBits_info kbi;

  STAT.initstate = START;

  capros_Node_getSlot(KR_CONSTIT, KC_OSTREAM,  KR_OSTREAM);
  capros_Node_getSlot(KR_CONSTIT, KC_KEYBITS,  KR_KEYBITS);
  capros_Node_getSlot(KR_CONSTIT, KC_DISCRIM,  KR_DISCRIM);

  STAT.numSorted = 0;
  STAT.numUnsorted = 0;
  STAT.end_of_table = the_table;
  
  capros_KeyBits_get(KR_KEYBITS, KR_VOID, &kbi);

  STAT.startKeyBitsVersion = kbi.version;

  kprintf(KR_OSTREAM, "Initializing Keyset\n");

  /* Buy a new root GPT for address space: */
  result = capros_SpaceBank_alloc1(KR_BANK, capros_Range_otGPT, KR_ADDRNODE);
  if (result != RC_OK) {
    DEBUG(init) 
      kdprintf(KR_OSTREAM, "KeySet: spcbank GPTs exhausted\n", result);
    teardown();
  }

  /* make each slot the right size: */
  capros_GPT_setL2v(KR_ADDRNODE, SLOT_ADDRESS_BITS);

  DEBUG(init) kdprintf(KR_OSTREAM, "KeySet: fetch my own space\n", result);
  capros_Process_getAddrSpace(KR_SELF, KR_SCRATCH);

  DEBUG(init) kdprintf(KR_OSTREAM,
		       "KeySet: plug self spc into new spc root\n",
		       result);
  capros_GPT_setSlot(KR_ADDRNODE, 0, KR_SCRATCH);
  
  DEBUG(init) kdprintf(KR_OSTREAM, "KeySet: before lobotomy\n", result);
  capros_Process_swapAddrSpace(KR_SELF, KR_ADDRNODE, KR_VOID);
  DEBUG(init) kdprintf(KR_OSTREAM, "KeySet: post lobotomy\n", result);

  STAT.initstate = LOBOTOMIZED;

  /* Construct the ZSF that will hold the actual directory data */
  DEBUG(init) kdprintf(KR_OSTREAM, "KeySet: Building ZSF\n");

  capros_Node_getSlot(KR_CONSTIT, KC_ZSF, KR_SCRATCH);
  result = constructor_request(KR_SCRATCH,
			       KR_BANK,
			       KR_SCHED,
			       KR_VOID,
			       KR_SCRATCH);

  DEBUG(init) kdprintf(KR_OSTREAM, "KeySet: Result is: 0x%08x\n", result);

  capros_key_getType(KR_SCRATCH, &keyType);

  if (result != RC_OK
      || keyType == RC_capros_key_Void ) {
    DEBUG(init) kdprintf(KR_OSTREAM, "KeySet: Failed to build ZS.\n");
    teardown();
  }
  /* plug in newly allocated ZSF */
  DEBUG(init) kdprintf(KR_OSTREAM,
		       "KeySet: plugging zsf into new spc root\n", result);
  capros_GPT_setSlot(KR_ADDRNODE, OUR_TABLE_SLOT, KR_SCRATCH);

  /* if we add more initialization: STAT.initstate = ZSBOUGHT; */

  STAT.initstate = RUNNING;
  return;
}

void
teardown(void)
{
  uint32_t result;

  /* this is in reverse order of construction.  
     FALL THROUGHS ARE INTENTIONAL. */

  switch(STAT.initstate) {  
  case RUNNING:
  case ZSBOUGHT:
    /* destroy the vcs */
    capros_GPT_getSlot(KR_ADDRNODE, OUR_TABLE_SLOT, KR_SCRATCH);
    
    result = capros_key_destroy(KR_SCRATCH);
    if (result != RC_OK) {
      kdprintf(KR_OSTREAM,
	       "KeySet: Failed to destroy my VCS (0x%08x)!\n", result);
    }
    /* FALL THROUGH */
  case LOBOTOMIZED:
    /* first, lets get our address space back to its original form */
    capros_GPT_getSlot(KR_ADDRNODE, 0, KR_SCRATCH);
    capros_Process_swapAddrSpace(KR_SELF, KR_SCRATCH, KR_VOID);

    /* return the node */
    result = capros_SpaceBank_free1(KR_BANK, KR_ADDRNODE);
    if (result != RC_OK) {
      kdprintf(KR_OSTREAM,
	       "KeySet: Failed to return address space node (0x%08x)!\n",
	       result);
    }
    /* FALL THROUGH */
  case START:
    break;
  }

  /* now, shoot myself in the head: */
  
  /* get the protospace */
  capros_Node_getSlot(KR_CONSTIT, KC_PROTOSPC, KR_SCRATCH);

  /* destroy as small space. */
  protospace_destroy(KR_VOID, KR_SCRATCH, KR_SELF, KR_CREATOR,
		     KR_BANK, 1);
  /* NOTREACHED */
}

uint32_t
VerifyAndGetSegmentOfSet(uint32_t krOtherSet,
			 uint32_t *retNumItems)
{
  Message msg;
  uint32_t result;
  uint32_t keyInfo;
  bool isEqual;

  /* it's got to be a KEYDATA 0u start key */
  result = capros_ProcCre_amplifyGateKey(KR_CREATOR, krOtherSet, KR_SCRATCH2,
				0, &keyInfo);
  if (result != RC_OK ||
      (keyInfo != Normal_KeyData       /* writable */
       && keyInfo != ReadOnly_KeyData)) { /* read-only */
    DEBUG(protocol)
      kprintf(KR_OSTREAM,
	      "Validation failed.  Got result %08x\n",result);  
    return RC_capros_key_UnknownRequest; /* nice try, fool. */
  }

  capros_Discrim_compare(KR_DISCRIM, KR_SELF, KR_SCRATCH2, &isEqual);
  
  if (isEqual)
    return RC_Internal_KeyToSelf;

  DEBUG(protocol)
    kprintf(KR_OSTREAM,
	    "Validated.  Starting MasterProtocol.\n");

  result = capros_Process_makeStartKey(KR_SELF,
				 GetSegment_KeyData,
				 KR_SCRATCH2);

  if (result != RC_OK) {
    kdprintf(KR_OSTREAM,
	     "KeySet: Error creating Protocol Start Key. \n");
  }

  DEBUG(protocol) ShowKey(KR_OSTREAM, KR_KEYBITS, KR_SCRATCH2);

  msg.snd_w1 = 0u;
  msg.snd_w2 = 0u;
  msg.snd_w3 = 0u;
  
  msg.snd_key0 = KR_SCRATCH2;
  msg.snd_key1 = KR_VOID;
  msg.snd_key2 = KR_VOID;
  msg.snd_rsmkey = KR_VOID;
  msg.snd_len = 0;

  msg.rcv_key0 = KR_SCRATCH2;
  msg.rcv_key1 = KR_VOID;
  msg.rcv_key2 = KR_VOID;
  msg.rcv_rsmkey = KR_SETRESUME;
  msg.rcv_limit = 0;

  msg.snd_code = OC_KeySet_Internal_GetSegment;
  msg.snd_invKey = krOtherSet;
  
  result = CALL(&msg);
  if (result == RC_KeySet_SetInvalid)
    return RC_KeySet_PassedSetInvalid;
  else if (result != RC_OK) {
    kdprintf(KR_OSTREAM,
	     "KeySet1: Hey! We just failed to start Internal "
	     "protocol.(%08x)\n",
	     msg.rcv_code);
    return RC_capros_key_RequestError;
  }
  
  capros_GPT_setSlot(KR_ADDRNODE, OTH_TABLE_SLOT, KR_SCRATCH2);
  /* Now, it's mapped in memory. */

  /* give our caller the number if items in the other guy. */
  if (retNumItems) *retNumItems = msg.rcv_w1;
  
  DEBUG(protocol) 
    if (msg.rcv_w1) {  /* there is data */
      uint32_t tmp;
      kprintf(KR_OSTREAM,"KeySet1: Checking mapping\n");
      tmp = other_table[0].w[0];
      kprintf(KR_OSTREAM,"KeySet1: other_table[0].w[0] = %08x\n",tmp);
      tmp = other_table[msg.rcv_w1 - 1].w[0];
      kprintf(KR_OSTREAM,
              "KeySet1: other_table[numTheirItems].w[0] = %08x\n",tmp);
      kprintf(KR_OSTREAM,"KeySet1: It's all there.\n"); 
    }

  DEBUG(protocol) kprintf(KR_OSTREAM,"KeySet1: Segment mapped into memory\n");

  return RC_OK;
}

uint32_t
SlaveProtocol(void)
{
  Message myMsg;
  uint32_t keyInfo;
  uint32_t result = capros_ProcCre_amplifyGateKey(KR_CREATOR, KR_ARG0, KR_VOID,
					 0, &keyInfo);
  DEBUG(protocol)
    kprintf(KR_OSTREAM,
	    "2ndKeySet: Amplify Gate returned 0x%04x\n",
	    result);

  DEBUG(protocol) ShowKey(KR_OSTREAM, KR_KEYBITS, KR_ARG0);

  if (result != RC_OK || keyInfo != GetSegment_KeyData) {
    DEBUG(protocol)
      kprintf(KR_OSTREAM,
	      "Validation failed.  Returning RC_capros_key_UnknownRequest.\n");
    /* Internal Protocol? what's that? */
    return RC_capros_key_UnknownRequest;
  }
  DEBUG(protocol)
    kprintf(KR_OSTREAM,
	    "2ndKeySet: Validated.  Starting SlaveProtocol.\n");

  /* first, make sure my segment is sorted. */
  require_sorted_table();

  /* get my segment */
  capros_GPT_getSlot(KR_ADDRNODE, OUR_TABLE_SLOT, KR_SCRATCH);
  
  /* make the segment read-only 
      -- he sure doesn't need to be able to write it */
  capros_Memory_reduce(KR_SCRATCH, capros_Memory_readOnly, KR_SCRATCH);
  
  myMsg.snd_w1 = STAT.numSorted;
  myMsg.snd_w2 = 0u;
  myMsg.snd_w3 = 0u;
  
  myMsg.snd_key0 = KR_SCRATCH;
  myMsg.snd_key1 = KR_VOID;
  myMsg.snd_key2 = KR_VOID;
  myMsg.snd_rsmkey = KR_VOID;
  myMsg.snd_len = 0;
  
  myMsg.rcv_key0 = KR_VOID;
  myMsg.rcv_key1 = KR_VOID;
  myMsg.rcv_key2 = KR_VOID;
  myMsg.rcv_rsmkey = KR_RETURN; /* for their new resume key */
  myMsg.rcv_limit = 0;
  
  myMsg.snd_code = RC_OK;
  myMsg.snd_invKey = KR_RETURN;

  /* CALL them so we won't be interrupted */
  if (CALL(&myMsg) != RC_OK) {
    kdprintf(KR_OSTREAM,
	     "KeySet2: Hey! We just failed to complete Internal protocol.\n");
  }

  DEBUG(protocol)
    kprintf(KR_OSTREAM,
	    "Finished SlaveProtocol successfully.\n");
  
  /* the CALL on the passed in resume key passed us a new resume
     key, which we will return to */
  return RC_OK; /* return success to them, completing the protocol */
}

uint32_t
ReturnSegment(void)
{
  Message msg;

  DEBUG(protocol)
    kprintf(KR_OSTREAM,
	    "KeySet1: Returning segment to KeySet2\n");

  capros_GPT_setSlot(KR_ADDRNODE, OTH_TABLE_SLOT, KR_VOID);
  /* Now, it's unmapped from memory. */
  
  msg.snd_w1 = 0u;
  msg.snd_w2 = 0u;
  msg.snd_w3 = 0u;
  
  msg.snd_key0 = KR_VOID;
  msg.snd_key1 = KR_VOID;
  msg.snd_key2 = KR_VOID;
  msg.snd_rsmkey = KR_VOID;
  msg.snd_len = 0;

  msg.rcv_key0 = KR_SCRATCH2;
  msg.rcv_key1 = KR_VOID;
  msg.rcv_key2 = KR_VOID;
  msg.rcv_rsmkey = KR_SETRESUME;
  msg.rcv_limit = 0;

  msg.snd_code = RC_OK;
  msg.snd_invKey = KR_SETRESUME;

  return CALL(&msg);
}

/* FIXME: options regarding data fields? */
uint32_t
AddKeysFromSet(uint32_t krOtherSet)
{
  uint32_t theirCount;
  uint32_t myCount = STAT.numSorted;
  uint32_t result;

  struct table_entry *myEntry = the_table;
  struct table_entry *theirEntry = other_table;
  
  result = VerifyAndGetSegmentOfSet(krOtherSet, &theirCount);
  if (result == RC_Internal_KeyToSelf) 
    return RC_OK; /* I've already got all of my keys */
  else if (result == RC_KeySet_PassedSetInvalid)
    return RC_KeySet_PassedSetInvalid;
  else if (result != RC_OK) {
    /* nice try. */
    return RC_capros_key_RequestError;
  }
  /* now we've got the other segment */

  /* search through their table for items not in mine, and add them to
     the unsorted list (note that both of the lists are sorted coming
     into this code) */
  
  while (theirCount && myCount) {
    int cmpres = compare(myEntry->w, theirEntry->w);
    
    if (cmpres == 0) {
      /* We match -- increment both */
      myEntry++;
      myCount--;
      
      theirEntry++;
      theirCount--;
    } else if (cmpres > 1) {
      /* We don't contain theirEntry -- copy it into my table */
      *STAT.end_of_table = *theirEntry;
      STAT.end_of_table++;
      STAT.numUnsorted++;

      /* now increment past it */
      theirEntry++;
      theirCount--;
    } else {
      /* They don't contain our entry -- increment mine */
      myEntry++;
      myCount--;
    }
  }

  /* copy any remaining items from them */
  while (theirCount) {
    *STAT.end_of_table = *theirEntry;
    STAT.end_of_table++;
    STAT.numUnsorted++;
    
    theirEntry++;
    theirCount--;
  }

  if (STAT.numUnsorted > MAX_UNSORTED) {
    /* too many unsorted -- sort them in */
    require_sorted_table();
  }
  
  /* Now, return the segment to the other guy. */
  ReturnSegment();
  return 0;
}

uint32_t
RemoveKeysNotInSet(uint32_t krOtherSet)
{
  uint32_t theirCount;
  uint32_t myCount = STAT.numSorted;
  uint32_t result;

  struct table_entry *myEntry = the_table;
  struct table_entry *theirEntry = other_table;

  struct table_entry *myNewEnd = the_table;
  uint32_t myNewCount = 0u;
  
  result = VerifyAndGetSegmentOfSet(krOtherSet, &theirCount);
  if (result == RC_Internal_KeyToSelf) 
    return RC_OK; /* We already contain ourself */
  else if (result == RC_KeySet_PassedSetInvalid)
    return RC_KeySet_PassedSetInvalid;
  else if (result != RC_OK) {
    /* nice try. */
    return RC_capros_key_RequestError;
  }
  /* now we've got the other segment */

  /* search through my table for items not in theirs, and remove them.
     (note that both of the lists are sorted coming into this code) */
  
  while (theirCount && myCount) {
    int cmpres = compare(myEntry->w, theirEntry->w);
    
    if (cmpres == 0) {
      /* We match -- copy if necessary, then increment both */
      if (myEntry != myNewEnd) {
	/*this guy's position has changed -- copy him to new position*/

	*myNewEnd = *myEntry;	
      }
      myNewEnd++;
      myNewCount++;

      myEntry++;
      myCount--;
      
      theirEntry++;
      theirCount--;
    } else if (cmpres > 0) {
      /* We don't contain theirEntry -- skip it */
      theirEntry++;
      theirCount--;
    } else {
      /* They don't contain our entry -- skip it, removing it from our
	 ending list. */
      myEntry++;
      myCount--;
    }
  }

  STAT.end_of_table = myNewEnd;
  STAT.numSorted = myNewCount;
  STAT.numUnsorted = 0;

  /* Now, return the segment to the other guy. */
  ReturnSegment();
  return 0;
}

uint32_t
CompareSets(uint32_t krOtherSet, uint32_t compareData)
{
  uint32_t theirCount;
  uint32_t myCount = STAT.numSorted;
  uint32_t result;

  uint32_t emptyintersect = 1; /* ME intersect HIM == 0 */
  uint32_t othermissing = 0;   /* he's missing something I have */
  uint32_t memissing = 0;      /* I'm missing something he has */
  uint32_t datamismatch = 0;   /* data fields mismatch -- only if
				  compareData */

  struct table_entry *myEntry = the_table;
  struct table_entry *theirEntry = other_table;

  result = VerifyAndGetSegmentOfSet(krOtherSet, &theirCount);
  if (result == RC_Internal_KeyToSelf)
    return RC_KeySet_SetsEqual; /* We equal ourself, thank you */
  else if (result == RC_KeySet_PassedSetInvalid)
    return RC_KeySet_PassedSetInvalid;
  else if (result != RC_OK) {
    /* nice try. */
    return RC_capros_key_RequestError;
  }
  /* now we've got the other segment */

  /* walk through both (sorted) tables, comparing the current pointers in
     each:
     If the keys are equal, we increment both counters to get past them.
     If *myEntry < *theirEntry, increment myEntry.
     if *myEntry > *theirEntry, we can't contain them, so break.
   */
  
  while (theirCount && myCount) {
    int cmpres;
    
#if 0
    kdprintf(KR_OSTREAM,
	     "Comparing 0x%08x %08x %08x %08x\n"
	     "       to 0x%08x %08x %08x %08x\n",
	     myEntry->w[0], myEntry->w[1], myEntry->w[2], myEntry->w[3],
	     theirEntry->w[0], theirEntry->w[1], theirEntry->w[2],
	     theirEntry->w[3]);
#endif
    
    cmpres = compare(myEntry->w, theirEntry->w);
    
    if (cmpres == 0) {
      emptyintersect = 0;
      if (compareData) {
	if (myEntry->w[4] != theirEntry->w[4]) {
	  datamismatch = 1;
	  break;
	}
      }
      /* increment both */
      myEntry++;
      myCount--;
      
      theirEntry++;
      theirCount--;
    } else if (cmpres > 0) {
      /* Can't contain their item */
      memissing = 1;

      theirEntry++;
      theirCount--;
    } else {
      /* increment mine */
      othermissing = 1;

      myEntry++;
      myCount--;
    }
  }

  if (!datamismatch) {
    if (myCount != 0)
      othermissing = 1;
    if (theirCount != 0)
      memissing = 1;
  }
  /* Now, return the segment to the other guy. */
  ReturnSegment();

#if 0
  kdprintf(KR_OSTREAM, "Contains completes. theirCount = %d\n", theirCount);
#endif
  
  if (compareData && datamismatch) 
    return RC_KeySet_DataMismatch;

  if (!memissing && !othermissing)
    return RC_KeySet_SetsEqual;

  if (emptyintersect)  /* both empty is taken care of by Equal case */
    return RC_KeySet_SetsDisjoint;

  if (!memissing && othermissing) /* I contain him */
    return RC_KeySet_SetContainsOtherSet;

  if (memissing && !othermissing) /* He contains me */
    return RC_KeySet_OtherSetContainsSet;

  /* none of the above */
  return RC_KeySet_SetsDifferent;
}

uint32_t
ProcessRequest(Message *msg)
{
  capros_KeyBits_info kbi;

  uint32_t rdOnly = (msg->rcv_keyInfo == ReadOnly_KeyData);

  uint32_t setInvalid = 0;

  capros_KeyBits_get(KR_KEYBITS, KR_ARG0, &kbi);

  if (kbi.version != STAT.startKeyBitsVersion
      && (STAT.numSorted || STAT.numUnsorted)) {
    /* we aren't empty, and keybits version changed */
    kdprintf(KR_OSTREAM, "Hey! Keybits version changed!\n");
    setInvalid = 1; /* everything should check readonly first, then
		       setInvalid */
  }


  DEBUG(cmds) {
    kprintf(KR_OSTREAM,
	    "KeySet: Got request 0x%08x, w1 = 0x%08x\n",
	    msg->rcv_code,
	    msg->rcv_w1);
    kprintf(KR_OSTREAM,
	    "Slot %d Keybits 0x%08x %08x %08x %08x\n",
	    KR_ARG0,
	    kbi.w[0],
	    kbi.w[1],
	    kbi.w[2],
	    kbi.w[3]);
  }
  
  msg->snd_code = RC_capros_key_UnknownRequest;
  msg->snd_w1 = 0;
  msg->snd_w2 = 0;
  msg->snd_w3 = 0;

  switch(msg->rcv_code) {
  case OC_KeySet_AddKey:
    {
      struct table_entry *the_entry;

      if (rdOnly) {
	msg->snd_code = RC_capros_key_UnknownRequest;
	break;
      } else if (setInvalid) {
	msg->snd_code = RC_KeySet_SetInvalid;
	break;
      }

      the_entry = FIND(the_table, kbi.w,STAT);

      DEBUG(add) kprintf(KR_OSTREAM,
			 "AddKey:  FIND returned %08x\n",
			 the_entry);
      
      if (the_entry != NULL) {
	/* already there, just update the entry. */
	msg->snd_w1 = the_entry->w[4];
	the_entry->w[4] = msg->rcv_w1;
	
	msg->snd_code = RC_OK;
      } else {
	/* not there -- insert it */
	the_entry = STAT.end_of_table;

	STAT.end_of_table++;
	STAT.numUnsorted++;
      
	DEBUG(add) kprintf(KR_OSTREAM,
			   "AddKey:  Writing new entry at 0x%08x\n",
			   the_entry);
      
	the_entry->w[0] = kbi.w[0];
	the_entry->w[1] = kbi.w[1];
	the_entry->w[2] = kbi.w[2];
	the_entry->w[3] = kbi.w[3];
	the_entry->w[4] = msg->rcv_w1; /* data field gets w1 */

	if (STAT.numUnsorted > MAX_UNSORTED) {
	  /* too many unsorted -- sort them in */
	  require_sorted_table();
	}
	msg->snd_code = RC_OK;
      }
      break;
    }
  case OC_KeySet_RemoveKey:
    {
      struct table_entry *entry;

      if (rdOnly) {
	msg->snd_code = RC_capros_key_UnknownRequest;
	break;
      } else if (setInvalid) {
	msg->snd_code = RC_KeySet_SetInvalid;
	break;
      }

      entry = FIND(the_table, kbi.w,STAT);
      if (entry == NULL) {
	msg->snd_code = RC_KeySet_KeyNotInSet;
	break;
      } else {
	uint32_t x = the_table - entry;
	uint32_t max = STAT.numSorted + STAT.numUnsorted;

	/* copy out the stored uint32_t */
	msg->snd_w1 = entry->w[4];

	/* update the counts -- moving blows away x */
	if (x < STAT.numSorted) {
	  STAT.numSorted--;
	} else {
	  STAT.numUnsorted--;
	}
	
	/* move everything back 1 slot */
#if 0 /* USE_MEMCPY */	
	memcpy(&the_table[x],   /* to */
	       &the_table[x+1], /*from*/
	       ((max - x) - 1)*sizeof(struct table_entry)); /* bytes */
#else /* ! USE_MEMCPY */
	for (x++ ; x < max; x++) {
	  the_table[x-1] = the_table[x];
	}
#endif
	msg->snd_code = RC_OK;
      }
    }
  case OC_KeySet_ContainsKey:
    {
      struct table_entry *entry;
      
      if (setInvalid) {
	msg->snd_code = RC_KeySet_SetInvalid;
	break;
      }

      entry = FIND(the_table, kbi.w,STAT);
      DEBUG(contains)
	kprintf(KR_OSTREAM,
		"KeySet: FIND returns 0x%08x\n", entry);
      if (entry == NULL) {
	msg->snd_code = RC_KeySet_KeyNotInSet;
	break;
      } else {
	msg->snd_w1 = entry->w[4]; /* give back the word */
	msg->snd_code = RC_OK;
	break;
      }
    }
  case OC_KeySet_IsEmpty:
    if (setInvalid) {
      msg->snd_code = RC_capros_key_UnknownRequest;
      break;
    }
    /* return 0 if we have entries, 1 if not */
    msg->snd_code = (STAT.numSorted || STAT.numUnsorted)?0:1;

  case OC_KeySet_Empty:

    if (rdOnly) {
      msg->snd_code = RC_capros_key_UnknownRequest;
      break;
    }
    /* if we are not valid, emptying the set makes us valid, so... */
    STAT.numSorted = 0;
    STAT.numUnsorted = 0;
    
    msg->snd_code = RC_OK;
    /* if shap ever gets around to implementing FreshSpace's truncate,
       We'll use it to free all of the allocated space.  Until
       then... */
    break;
    
  case OC_KeySet_MakeReadOnlyKey:
    
    /* FIXME: check return code */
    capros_Process_makeStartKey(KR_SELF,
			  ReadOnly_KeyData,
			  KR_SCRATCH);
    
    msg->snd_key0 = KR_SCRATCH;
    msg->snd_code = RC_OK;
    break;
    
  case OC_KeySet_AddKeysFromSet:

    if (rdOnly) {
      msg->snd_code = RC_capros_key_UnknownRequest;
      break;
    } else if (setInvalid) {
      msg->snd_code = RC_KeySet_SetInvalid;
      break;
    }

    require_sorted_table();

    msg->snd_code = AddKeysFromSet(KR_ARG0);
    break;
  case OC_KeySet_RemoveKeysNotInSet:

    if (rdOnly) {
      msg->snd_code = RC_capros_key_UnknownRequest;
      break;
    } else if (setInvalid) {
      msg->snd_code = RC_KeySet_SetInvalid;
      break;
    }

    require_sorted_table();

    msg->snd_code = RemoveKeysNotInSet(KR_ARG0);
    break;

#if 1 /* IsSubsetOfSet */
  case OC_KeySet_IsSubsetOfSet:
#else
  case OC_KeySet_CompareSets: /* does expanded CompareSets imply a
				 security problem for Constructors? */
#endif
    if (setInvalid) {    
      msg->snd_code = RC_KeySet_SetInvalid;
      break;
    }

    require_sorted_table();

#if 1 /* isSubSetOfSet */
    msg->snd_code = CompareSets(KR_ARG0,
				0);
    msg->snd_code = (msg->snd_code == RC_KeySet_SetsEqual
		     || msg->snd_code == RC_KeySet_SetContainsOtherSet
		    ) ? RC_OK : 1;
#else
    msg->snd_code = CompareSets(KR_ARG0,
				msg->rcv_w1);
#endif

    break;
  case OC_KeySet_Internal_GetSegment:
    
    if (setInvalid) {
      msg->snd_code = RC_KeySet_SetInvalid;
      break;
    }

    msg->snd_code = SlaveProtocol();
    break;
  case OC_capros_key_destroy:
    if (rdOnly) {
      msg->snd_code = RC_capros_key_UnknownRequest;
      break;
    }
    teardown();
    return 0; /* CAN'T HAPPEN */
  }
  DEBUG(cmds)
    kprintf(KR_OSTREAM,
	    "KeySet: Returning result 0x%08x\n",
	    msg->snd_code);
  /* update keyData field, if appropriate (i.e. we were empty and
     something was added to us) -- This happens if the keyDatas don't
     match, we are valid, and we are now non-empty */

  if (kbi.version != STAT.startKeyBitsVersion
      && !setInvalid
      && (STAT.numSorted || STAT.numUnsorted)) {
    STAT.startKeyBitsVersion = kbi.version;
  }
  return 1;
}

int
main()
{
  Message msg;
  
  Initialize();

  /* make a write key and return it. */
  capros_Process_makeStartKey(KR_SELF, Normal_KeyData, KR_ARG0);

  DEBUG(init) kdprintf(KR_OSTREAM,
		       "KeySet: Got start key. Ready to rock and roll\n");

  msg.snd_invKey = KR_RETURN;
  msg.snd_key0 = KR_ARG0;
  msg.snd_key1 = KR_VOID;
  msg.snd_key2 = KR_VOID;
  msg.snd_rsmkey = KR_VOID;
  msg.snd_data = 0;
  msg.snd_len = 0;
  msg.snd_code = 0;
  msg.snd_w1 = 0;
  msg.snd_w2 = 0;
  msg.snd_w3 = 0;

  msg.rcv_key0 = KR_ARG0;
  msg.rcv_key1 = KR_VOID;
  msg.rcv_key2 = KR_VOID;
  msg.rcv_rsmkey = KR_RETURN;
  msg.rcv_limit = 0;

  do {
    RETURN(&msg);

    /* clear all of the slots we might use for sending */
    msg.snd_key0 = KR_VOID;              /* until otherwise proven */

    ProcessRequest(&msg);
  } while(1); /*forever*/
}

#ifndef NDEBUG
int __assert(const char *expr, const char *file, int line)
{
  kdprintf(KR_OSTREAM, "%s:%d: Assertion failed: '%s'\n",
           file, line, expr);
  return 0;
}
#endif

