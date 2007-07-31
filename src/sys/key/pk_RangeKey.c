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

#include <kerninc/kernel.h>
#include <kerninc/Key.h>
#include <kerninc/Invocation.h>
#include <kerninc/ObjectCache.h>
#include <kerninc/Node.h>
#include <kerninc/GPT.h>
#include <disk/DiskNodeStruct.h>
#include <disk/Forwarder.h>

#include <eros/Invoke.h>
#include <eros/StdKeyType.h>

#include <idl/capros/key.h>
#include <idl/capros/Range.h>
#include <idl/capros/Memory.h>
#include <idl/capros/Forwarder.h>

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
  else return -1;
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
  bool wait, ObType obType, uint8_t kkt)
{
  uint32_t obNdx;
  ObjectHeader *pObj = 0;

  /* Figure out the OID for the new key: */
  OID oid = rngStart + offset;

  if (oid >= rngEnd) {
    dprintf(true, "oid 0x%X top 0x%X\n", oid, rngEnd);
    COMMIT_POINT();
    inv->exit.code = RC_capros_Range_RangeErr;
    return;
  }

  obNdx = offset % EROS_OBJECTS_PER_FRAME;

  inv->flags |= INV_EXITKEY0;

  if (! wait && ! objC_HaveSource(oid)) {
    COMMIT_POINT();
    inv->exit.code = RC_capros_Range_RangeErr;
    return;
  }

  const unsigned int objPerPage =
    obType == ot_PtDataPage ? 1 : DISK_NODES_PER_PAGE;
  if (obNdx >= objPerPage) {
    COMMIT_POINT();
    inv->exit.code = RC_capros_Range_RangeErr;
    return;
  }

  /* If we don't get the object back, it's because of bad frame
   * type:
   */

  pObj = objC_GetObject(oid, obType, 0, false);

  assert(inv_CanCommit());
  assert(pObj);

  /* It's definitely an object key.  Pin the object it names. */

  objH_TransLock(pObj);

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
    /* Unchain the old key so we can overwrite it... */
    key_NH_Unchain(key);

    keyBits_InitType(key, kkt);
  
    /* Link as next key after object */
    key->u.ok.pObj = pObj;
    link_insertAfter(&pObj->keyRing, &key->u.ok.kr);
    keyBits_SetPrepared(key);
  
    if (kkt == KKT_Page)
      // Default l2g for memory keys is 64 to disable guard test.
      keyBits_SetL2g(key, 64);
    else if (kkt == KKT_GPT) {
      // Default l2g for memory keys is 64 to disable guard test.
      keyBits_SetL2g(key, 64);
      // Ensure the l2v is valid.
      GPT * theGPT = objH_ToNode(pObj);
      uint8_t l2vField = gpt_GetL2vField(theGPT);
      uint8_t oldL2v = l2vField & GPT_L2V_MASK;
      if (oldL2v < EROS_PAGE_LGSIZE)
        gpt_SetL2vField(theGPT, l2vField - oldL2v + EROS_PAGE_LGSIZE);
    }
  }

#ifdef DEBUG_PAGERANGEKEY
  dprintf(true, "pObject is 0x%08x\n", pObj);
#endif

  inv->exit.code = RC_OK;  /* set the exit code */
  
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

/* May Yield. */
void
RangeKey(Invocation* inv /*@ not null @*/)
{
  bool waitFlag;

  capros_Range_off_t range;
  
  rngEnd = key_GetRange(inv->key, &rngStart);
  
  range = rngEnd - rngStart;


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
    return;

  case OC_capros_Range_query:
    {
      COMMIT_POINT();

      inv->exit.w1 = range;
      inv->exit.w2 = (fixreg_t) (range >> 32);

      inv->exit.code = RC_OK;

      return;
    }
    
  case OC_capros_Range_nextSubrange:
    {
      OID subStart;
      OID subEnd;

      COMMIT_POINT();

      OID startOffset = w1w2Offset(inv) + rngStart;

      if (startOffset >= rngEnd) {
	inv->exit.code = RC_capros_Range_RangeErr;
	return;
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

      return;
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
	return;
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

      return;
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
	return;
      }

      inv->exit.w1 = t;

      OID oid = key_GetKeyOid(key);

      inv->exit.code = RC_OK;	// default
      
      if ( oid < rngStart || oid >= rngEnd ) {
	inv->exit.code = RC_capros_Range_RangeErr;
        return;
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
      
      return;
    }
  case OC_capros_Range_rescind:
    {
      Key * key /*@ not null @*/ = inv->entry.key[0];
   
      key_Prepare(key);

      inv->exit.code = RC_OK;
      
      if (ValidateKey(key) < 0) {
	inv->exit.code = RC_capros_Range_RangeErr;
	COMMIT_POINT();

	return;
      }

      OID oid = key_GetKeyOid(key);
      
#ifdef DEBUG_PAGERANGEKEY
      dprintf(true, "Rescinding OID 0x%08x%08x\n",
		      (uint32_t) (oid>>32),
		      (uint32_t) oid);
#endif
      
      if ( oid < rngStart || oid >= rngEnd ) {
	inv->exit.code = RC_capros_Range_RangeErr;
	COMMIT_POINT();

	return;
      }

      ObjectHeader * pObject = key_GetObjectPtr(key);

      objH_FlushIfCkpt(pObject);

      COMMIT_POINT();
      
      objH_Rescind(pObject);

#ifdef DBG_WILD_PTR
      if (dbg_wild_ptr)
	check_Consistency("Object rescind()");
#endif
      return;
    }

  case OC_capros_Range_waitPageKey:
    MakeObjectKey(inv, w1w2Offset(inv),
      true, ot_PtDataPage, KKT_Page);
    return;

  case OC_capros_Range_getPageKey:
    MakeObjectKey(inv, w1w2Offset(inv),
      false, ot_PtDataPage, KKT_Page);
    return;

  case OC_capros_Range_waitNodeKey:
    MakeObjectKey(inv, w1w2Offset(inv),
      true, ot_NtUnprepared, KKT_Node);
    return;

  case OC_capros_Range_getNodeKey:
    MakeObjectKey(inv, w1w2Offset(inv),
      false, ot_NtUnprepared, KKT_Node);
    return;

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
        return;
      }

      static ObType baseType[capros_Range_otNUM_TYPES] = {
        [capros_Range_otPage]=ot_PtDataPage,
        [capros_Range_otNode]=ot_NtUnprepared,
        [capros_Range_otForwarder]=ot_NtUnprepared,
        [capros_Range_otGPT]=ot_NtUnprepared,
      };
      static uint8_t obKKT[capros_Range_otNUM_TYPES] = {
        [capros_Range_otPage]=KKT_Page,
        [capros_Range_otNode]=KKT_Node,
        [capros_Range_otForwarder]=KKT_Forwarder,
        [capros_Range_otGPT]=KKT_GPT,
      };

      MakeObjectKey(inv, w2w3Offset(inv),
        waitFlag, baseType[ot], obKKT[ot]);

      return;
    }

  case OC_capros_Range_compare:
    {
      Key* key /*@ not null @*/ = inv->entry.key[0];

      key_Prepare(key);


      COMMIT_POINT();

      inv->exit.code = RC_OK;
      inv->exit.w1 = 0;		/* no overlap until proven otherwise */

      if (!keyBits_IsType(key, KKT_Range) && !keyBits_IsType(key, KKT_PrimeRange))
	return;			/* RC_OK in this case probably wrong thing. */

      OID kstart;
      OID kend = key_GetRange(key, &kstart);
  
      if (kstart >= rngEnd || kend <= rngStart)
	return;

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
      return;
    }
  default:
    COMMIT_POINT();

    inv->exit.code = RC_capros_key_UnknownRequest;
    return;
  }

  return;
}
