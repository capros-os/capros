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
#include <kerninc/Key.h>
#include <kerninc/Invocation.h>
#include <kerninc/ObjectCache.h>
#include <kerninc/ObjectSource.h>
#include <kerninc/Node.h>
#include <kerninc/GPT.h>
#include <kerninc/LogDirectory.h>
#include <disk/DiskNode.h>
#include <disk/Forwarder.h>

#include <eros/Invoke.h>
#include <eros/StdKeyType.h>
#include <kerninc/Key-inline.h>
#include <arch-kerninc/PTE.h>

#include <idl/capros/key.h>
#include <idl/capros/Range.h>
#include <idl/capros/Memory.h>
#include <idl/capros/Forwarder.h>

#ifdef DBG_WILD_PTR
#include <kerninc/Check.h>
#endif

const unsigned int objectsPerFrame[2] = {
  [capros_Range_otPage] = 1,
  [capros_Range_otNode] = DISK_NODES_PER_PAGE
};

/*
 * There is a problem with range keys that is pretty well
 * fundamental. In a nutshell, the key representation only gives us
 * 112 bits of storage (3*32 + 16 in keyData), and we need to
 * represent 128 bits of information (base and bound).
 *
 * In the final analysis, there are really only two viable solutions:
 *
 * 1. You can restrict the range of valid OIDs to something that fits.
 *    This means 56 bits/oid. Since 4 bits (minimum) are used for the
 *    object index, this would give us 2^52 page frames, or (assuming
 *    4k pages) 2^64 bytes of addressable storage.
 *
 * 2. You can restrict range keys to a representable subrange, and
 *    require some applications to manage multiple range keys. This
 *    would give base=64 bits and length=48 bits, using the
 *    keyData field for the extra 16 bits of the length. At the
 *    moment, I'm restricting things to 32 bits of representable range
 *    and avoiding the keyData field.
 *
 * 3. You can introduce a "superrange" capability that uses a
 *    distinctive key type to actually cover the full range.
 *
 * I have chosen to combine options (2) and (3), but this results in a
 * secondary problem: the "find first subrange" operation may someday
 * actually find a subrange whose length cannot be represented in 48
 * bits, and it's not immediately clear what it should do in this
 * case. I have chosen to implement "find first subrange" as "find
 * first reportable subrange". If the first subrange is 2^32 objects
 * or larger, then the reported first subrange will be of length
 * (2^32-1). The caller can recognize this case by performing:
 *
 *     top = range_query(range_key);
 *     (base, bound) = find_first_subrange(range_key);
 *     new_range_key = make_subrange(range_key, bound, top);
 *     (base2, bound2) = find_first_subrange(new_range_key);
 *
 *     check if base2 == 0.
 *
 * I cannot imagine this actually arising much in the next several
 * years, though current (year 2001) disk drives are currently
 * implementing on the close order of 2^25 pages. This takes a minimum
 * of 2^29 oids (4 bits for object index), so I expect we will blow
 * past the 2^32 oid limit within the next year or two and I will need
 * to eat into the keyData field. I'm deferring that primarily for
 * time reasons.
 */
/* (CRL) Another approach is to force the beginning and of a range
 * to align on a frame boundary (which seems reasonable), and use the fact
 * that EROS_OBJECTS_PER_FRAME is 256. Any such range can be represented
 * in 14 bytes. */

/* #define DEBUG_RANGEKEY */

OID rngStart, rngEnd;
      
// Returns object type or -1
static int
ValidateKey(Key * key)
{
  if (keyBits_IsType(key, KKT_Page))	// maybe test readOnly
    return capros_Range_otPage;
  else if (keyBits_IsType(key, KKT_Node))	// maybe test readOnly
    return capros_Range_otNode;
  else if (keyBits_IsType(key, KKT_Forwarder)
           && ! (key->keyData & capros_Forwarder_opaque) )
    return capros_Range_otForwarder;
  else if (keyBits_IsType(key, KKT_GPT)
           && ! (key->keyData & capros_Memory_opaque) )
    return capros_Range_otGPT;
  else {
    // dprintf(true, "Range got invalid key at 0x%x\n", key);
    return -1;
  }
}

static uint64_t
w1w2Offset(Invocation * inv)
{
  return (((uint64_t) inv->entry.w2) << 32) | ((uint64_t) inv->entry.w1);
}

static uint64_t
w2w3Offset(Invocation * inv)
{
  return (((uint64_t) inv->entry.w3) << 32) | ((uint64_t) inv->entry.w2);
}

static void
MakeObjectKey(Invocation * inv, uint64_t offset,
  bool wait, ObType baseType, uint8_t kkt)
{
  /* Figure out the OID for the new key: */
  OID oid = rngStart + offset;

  if (oid >= rngEnd) {
    dprintf(true, "oid 0x%X top 0x%X\n", oid, rngEnd);
    COMMIT_POINT();
    inv->exit.code = RC_capros_Range_RangeErr;
    return;
  }

  uint32_t obNdx = offset % EROS_OBJECTS_PER_FRAME;

  const unsigned int objPerPage = objectsPerFrame[baseType];

  if (obNdx >= objPerPage) {
    COMMIT_POINT();
    inv->exit.code = RC_capros_Range_RangeErr;
    return;
  }

  if (! wait && ! objC_HaveSource(oid)) {
    COMMIT_POINT();
    inv->exit.code = RC_capros_Range_RangeErr;
    return;
  }

  ObjectLocator objLoc;
  objLoc = GetObjectType(oid);

  if (objLoc.objType == capros_Range_otNone)
    objLoc.objType = baseType;	// we can pick the type to suit

  if (objLoc.objType != baseType) {
    // Need to retype the frame.
    COMMIT_POINT();
    inv->exit.code = RC_OK;
    inv->exit.w1 = objLoc.objType;
    return;
  }

  // Get the object, regardless of its allocation count:
  ObjectHeader * pObj = GetObject(oid, &objLoc);

  assert(! MapsWereInvalidated());
  assert(pObj);

  objH_TransLock(pObj);	// Pin the object.

  inv->flags |= INV_EXITKEY0;

  if (kkt == KKT_Forwarder) {
    // A Forwarder must always have a number key in slot ForwarderDataSlot.
    /* Note: We are trusting holders of a Range key not to get a Node key
    to the same object, overwrite the number key, and then use the Forwarder. */
    Key * keynum = node_GetKeyAtSlot(objH_ToNode(pObj), ForwarderDataSlot);
    if (! keyBits_IsType(keynum, KKT_Number) ) {
      node_MakeDirty(objH_ToNode(pObj));

      COMMIT_POINT();

      key_NH_Unchain(keynum);
      keyBits_InitType(keynum, KKT_Number);
      keynum->u.nk.value[0] = 0;
      keynum->u.nk.value[1] = 0;
      keynum->u.nk.value[2] = 0;
    } else {
      // it is already a number key. Preserve it and its data.

      COMMIT_POINT();
    }
  } else {

    COMMIT_POINT();
  }
  
  Key * key = inv->exit.pKey[0];
  if (key) {
    key_NH_Unchain(key);
    key_SetToObj(key, pObj, kkt, 0, 0);
  
    // Set defaults for keyData.
    switch (kkt) {
    case KKT_GPT: ;
      // Ensure the l2v is valid.
      GPT * theGPT = objH_ToNode(pObj);
      uint8_t l2vField = gpt_GetL2vField(theGPT);
      uint8_t oldL2v = l2vField & GPT_L2V_MASK;
      if (oldL2v < EROS_PAGE_LGSIZE)
        gpt_SetL2vField(theGPT, l2vField - oldL2v + EROS_PAGE_LGSIZE);
      // Fall into Page case to set l2g.
    case KKT_Page:
      // For page, l2v isn't used.
    case KKT_Node:
      // For node, any l2v is valid.
      // Default l2g for memory and node keys is 64 to disable guard test.
      keyBits_SetL2g(key, 64);
      break;
    default: break;
    }
  }

#ifdef DEBUG_PAGERANGEKEY
  dprintf(true, "pObject is 0x%08x\n", pObj);
#endif

  inv->exit.code = RC_OK;  /* set the exit code */
  inv->exit.w1 = capros_Range_otNone;
  
  return;
}

OID /* returns end of range */
key_GetRange(Key * key, /* out */ OID * rngStart)
{
  if (key->keyType == KKT_PrimeRange) {
    *rngStart = 0ll;
    return OID_RESERVED_PHYSRANGE;
  }
  else if (key->keyType == KKT_PhysRange) {
    *rngStart = OID_RESERVED_PHYSRANGE;
    return UINT64_MAX;
  }
  else {
    *rngStart = key->u.rk.oid;
    return key->u.rk.oid + key->u.rk.count;
  }
}

static void
RetypeDestroyOldObjects(unsigned int oldType, OID oid)
{
  int i;

  for (i = 0; i < objectsPerFrame[oldType]; i++, oid++) {
    // Destroy vestiges of the old type.
    // Remove it from memory.
    ObjectHeader * pObj = objH_Lookup(oid, 0);
    if (pObj) {
      objH_ClearFlags(pObj, OFLG_DIRTY);	// discard data
      if (objH_isNodeType(pObj)) {
        ReleaseNodeFrame(objH_ToNode(pObj));
      } else {
        objH_InvalidateProducts(pObj);
        keyR_UnprepareAll(&pObj->keyRing);
        ReleaseObjPageFrame(objH_ToPage(pObj));
      }
    }
    // Destroy any log directory entry.
    ////assert(!"complete");
  }
}

/* May Yield. */
void
RangeKey(Invocation* inv /*@ not null @*/)
{
  bool waitFlag;
  
  inv_GetReturnee(inv);

  rngEnd = key_GetRange(inv->key, &rngStart);
  capros_Range_off_t range = rngEnd - rngStart;

  switch(inv->entry.code) {
  case OC_capros_key_getType:
    COMMIT_POINT();

    /* Notice that this returns AKT_Range for both the prime range key
     * and the more conventional range key. This is actually correct,
     * because both keys obey identical interfaces. The prime range
     * key can be distinguished by the fact that it's reported range
     * from OC_RANGE_QUERY is large. */
    inv->exit.code = RC_OK;
    inv->exit.w1 = AKT_Range;
    break;

  case OC_capros_Range_query:
    {
      COMMIT_POINT();

      inv->exit.w1 = range;
      inv->exit.w2 = (fixreg_t) (range >> 32);
      inv->exit.code = RC_OK;
      break;
    }
    
  case OC_capros_Range_nextSubrange:
    {
      OID subStart;
      OID subEnd;
      // FIXME: this should wait until all disks are mounted.

      COMMIT_POINT();

      OID startOffset = w1w2Offset(inv) + rngStart;

      if (startOffset >= rngEnd) {
	inv->exit.code = RC_capros_Range_RangeErr;
	break;
      }

      /* FIX: This only works for 32-bit subranges */

      objC_FindFirstSubrange(startOffset, rngEnd, &subStart, &subEnd);

      range = subEnd - subStart;
      if (range >= (uint64_t) UINT32_MAX)
	range = UINT32_MAX;

      subStart -= rngStart;

      inv->exit.w1 = subStart;
      inv->exit.w2 = (fixreg_t) (subStart >> 32);
      inv->exit.w3 = range;

      inv->exit.code = RC_OK;
      break;
    }
    
  case OC_capros_Range_makeSubrange:
    {
      OID newLen;
      OID newEnd;
      Key *key = 0;

      COMMIT_POINT();

      /* This implementation allows for 64 bit offsets, but only 32
       * bit limits, which is nuts! */
      OID newStart = w1w2Offset(inv) + rngStart;

      /* This is not an issue with the broken interface, but with the
       * 48 bit length representation and a fixed interface we could
       * get caught here by a representable bounds problem. */
      newLen = inv->entry.w3;
      newLen = min(newLen, UINT32_MAX);

      newEnd = newLen + newStart;

      /* REMEMBER: malicious arithmetic might wrap! */
      if ((newEnd < newStart) ||
          (newStart < rngStart) ||
	  (newStart >= rngEnd) ||
	  (newEnd <= rngStart) ||
	  (newEnd > rngEnd)) {
	inv->exit.code = RC_capros_Range_RangeErr;
	break;
      }

      key = inv->exit.pKey[0];
      inv->flags |= INV_EXITKEY0;

      if (key) {
	/* Unchain the old key so we can overwrite it... */
       
	if (key)
	  key_NH_Unchain(key);
       

	keyBits_InitType(key, KKT_Range);
	key->u.rk.oid = newStart;
	key->u.rk.count = newLen;
      }

      inv->exit.code = RC_OK;
      break;
    }
    
  case OC_capros_Range_identify:
    {
      /* Key to identify is in slot 0 */
      Key* key /*@ not null @*/ = inv->entry.key[0];

      key_Prepare(key);

      COMMIT_POINT();

      int t = ValidateKey(key);
      if (t < 0) {
	inv->exit.code = RC_capros_Range_RangeErr;
	break;
      }

      inv->exit.w1 = t;

      OID oid = key_GetKeyOid(key);
      
      if ( oid < rngStart || oid >= rngEnd ) {
	inv->exit.code = RC_capros_Range_RangeErr;
        break;
      }
      range = oid - rngStart;
      
      /* FIX: there is a register size assumption here! */
      assert (sizeof(range) == sizeof(uint64_t));
      assert (sizeof(inv->exit.w2) == sizeof(uint32_t) ||
	      sizeof(inv->exit.w2) == sizeof(uint64_t));
      inv->exit.w2 = range;
      if (sizeof(inv->exit.w2) != sizeof(capros_Range_off_t))
	inv->exit.w3 = (range >> 32);
      else
	inv->exit.w3 = 0;

      inv->exit.code = RC_OK;
      break;
    }
  case OC_capros_Range_rescind:
    {
      Key * key /*@ not null @*/ = inv->entry.key[0];
   
      key_Prepare(key);
      
      if (ValidateKey(key) < 0) {
	COMMIT_POINT();

	inv->exit.code = RC_capros_Range_RangeErr;
	break;
      }

      OID oid = key_GetKeyOid(key);
      
#ifdef DEBUG_PAGERANGEKEY
      dprintf(true, "Rescinding OID %#llx\n", oid);
#endif
      
      if ( oid < rngStart || oid >= rngEnd ) {
	COMMIT_POINT();

	inv->exit.code = RC_capros_Range_RangeErr;
	break;
      }

      ObjectHeader * pObject = key_GetObjectPtr(key);

      objH_FlushIfCkpt(pObject);
      objH_MakeObjectDirty(pObject);	// does not reset the age

      COMMIT_POINT();
      
      objH_Rescind(pObject);

      /* Clear it, to avoid saving stale state on disk. */
      objH_ClearObj(pObject);

      if (objH_isNodeType(pObject)) {
        /* Unprepare the node, in case it is re-allocated as a
        different type (for example, process to GPT). */
        node_Unprepare(objH_ToNode(pObject));
      }

#ifdef DBG_WILD_PTR
      if (dbg_wild_ptr)
	check_Consistency("Object rescind()");
#endif

      inv->exit.code = RC_OK;
      break;
    }

  case OC_capros_Range_getCap:
    waitFlag = false;
    goto rangeGetWaitCap;

  case OC_capros_Range_waitCap:
    waitFlag = true;
rangeGetWaitCap:
    {
      uint32_t ot = inv->entry.w1;

      if (ot >= capros_Range_otNUM_TYPES) {
        COMMIT_POINT();

        inv->exit.code = RC_capros_Range_RangeErr;
        break;
      }

      static ObType baseType[capros_Range_otNUM_TYPES] = {
        [capros_Range_otPage]=capros_Range_otPage,
        [capros_Range_otNode]=capros_Range_otNode,
        [capros_Range_otForwarder]=capros_Range_otNode,
        [capros_Range_otGPT]=capros_Range_otNode,
      };
      static uint8_t obKKT[capros_Range_otNUM_TYPES] = {
        [capros_Range_otPage]=KKT_Page,
        [capros_Range_otNode]=KKT_Node,
        [capros_Range_otForwarder]=KKT_Forwarder,
        [capros_Range_otGPT]=KKT_GPT,
      };

      MakeObjectKey(inv, w2w3Offset(inv),
        waitFlag, baseType[ot], obKKT[ot]);

      break;
    }

  case OC_capros_Range_compare:
    {
      Key* key /*@ not null @*/ = inv->entry.key[0];

      key_Prepare(key);

      COMMIT_POINT();

      inv->exit.code = RC_OK;
      inv->exit.w1 = 0;		/* no overlap until proven otherwise */

      if (!keyBits_IsType(key, KKT_Range) && !keyBits_IsType(key, KKT_PrimeRange))
	break;			/* RC_OK in this case probably wrong thing. */

      OID kstart;
      OID kend = key_GetRange(key, &kstart);
  
      if (kstart >= rngEnd || kend <= rngStart)
	break;

      /* They overlap; need to figure out how. */
      inv->exit.w1 = 1;
      inv->exit.w3 = 0;
      
      if (kstart < rngStart) {
	inv->exit.w1 = 3;
	inv->exit.w2 = (fixreg_t) (rngStart - kstart);
	if (sizeof(inv->exit.w2) == sizeof(uint32_t)) /* 32 bit system */
	  inv->exit.w3 = (fixreg_t) ((rngStart - kstart) >> 32);
      }
      else if (kstart == rngStart) {
	inv->exit.w1 = 1;
	inv->exit.w2 = 0;
      }
      else {
	inv->exit.w1 = 2;
	inv->exit.w2 = (fixreg_t) (kstart - rngStart);
	if (sizeof(inv->exit.w2) == sizeof(uint32_t)) /* 32 bit system */
	  inv->exit.w3 = (fixreg_t) ((kstart - rngStart) >> 32);
      }
      break;
    }

  case OC_capros_Range_getFrameCounts:
  {
    OID oid = w1w2Offset(inv) + rngStart;
    if (oid >= rngEnd) {
      COMMIT_POINT();
      inv->exit.code = RC_capros_Range_RangeErr;
      break;
    }

    ObjectLocator objLoc;
    objLoc = GetObjectType(oid);

    assert(objLoc.objType != capros_Range_otNone);

    // Get the counts:
    ObCount allocCount = GetObjectCount(oid, &objLoc, false);
    if (objLoc.objType == capros_Range_otNode) {
      ObCount callCount = GetObjectCount(oid, &objLoc, true);
      allocCount = max(allocCount, callCount);
    }

    COMMIT_POINT();

    inv->exit.w1 = allocCount;
    inv->exit.code = RC_OK;

    break;
  }

  case OC_capros_Range_retypeFrame:
  {
    int i;
    struct {
      capros_Range_obType newType;
      uint8_t pad[3];	// align
      capros_Range_count_t allocationCount;
    } stringParams;

    if (inv->entry.len < sizeof(stringParams)) {	// string too short
      COMMIT_POINT();
      inv->exit.code = RC_capros_key_RequestError;
      break;
    }

    inv_CopyIn(inv, sizeof(stringParams), &stringParams);

    OID oid = w1w2Offset(inv) + rngStart;
    if (oid >= rngEnd
        || OIDToObIndex(oid) != 0) {
      COMMIT_POINT();
      inv->exit.code = RC_capros_Range_RangeErr;
      break;
    }

    unsigned int oldType = inv->entry.w3;

    if (oid < FIRST_PERSISTENT_OID) {
      // A non-persistent frame.
      // Ensure we can get enough new objects
      EnsureObjFrames(stringParams.newType,
                      objectsPerFrame[stringParams.newType]);

      COMMIT_POINT();

      RetypeDestroyOldObjects(oldType, oid);

      // Make new null objects in memory.
      for (i = 0; i < objectsPerFrame[stringParams.newType]; i++, oid++) {
        CreateNewNullObject(stringParams.newType,
                            oid, stringParams.allocationCount);
      }
    } else {
      // A persistent frame.
      //// get enough new dirents
      COMMIT_POINT();

      RetypeDestroyOldObjects(oldType, oid);

      // Make new null objects in the log directory.
      ObjectDescriptor objDesc = {
        .allocCount = stringParams.allocationCount,
        .callCount = stringParams.allocationCount,
        .logLoc = 0,		// object is null
        .type = stringParams.newType
      };
      for (i = 0; i < objectsPerFrame[stringParams.newType]; i++, oid++) {
        objDesc.oid = oid;
        ld_recordLocation(&objDesc, workingGenerationNumber);
      }
    }

    inv->exit.code = RC_OK;

    break;
  }

  default:
    COMMIT_POINT();

    inv->exit.code = RC_capros_key_UnknownRequest;
    break;
  }

  ReturnMessage(inv);
}
