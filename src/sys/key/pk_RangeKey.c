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
#include <kerninc/Check.h>
#include <kerninc/Key.h>
/*#include <kerninc/BlockDev.h>*/
#include <kerninc/Activity.h>
#include <kerninc/Invocation.h>
#include <kerninc/ObjectCache.h>
#include <disk/DiskNodeStruct.h>
#include <disk/DiskFrame.h>

#include <eros/Invoke.h>
#include <eros/StdKeyType.h>

#include <idl/eros/key.h>
#include <idl/eros/Range.h>

/*
 * There is a problem with range keys that is pretty well
 * fundamental. In a nutshell, the key representation only gives us
 * 112 bits of storage (3*32 + 16 in keyInfo), and we need to
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
 *    keyInfo field for the extra 16 bits of the length. At the
 *    moment, I'm restricting things to 32 bits of representable range
 *    and avoiding the keyInfo field.
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
 * of 2^29 oids (16 bits for object index), so I expect we will blow
 * past the 2^32 oid limit within the next year or two and I will need
 * to eat into the keyInfo field. I'm deferring that primarily for
 * time reasons.
 */

/* #define DEBUG_RANGEKEY */

void
RangeKey(Invocation* inv /*@ not null @*/)
{
  OID start = inv->key->u.rk.oid;
  OID end = inv->key->u.rk.oid + inv->key->u.rk.count;
  eros_Range_off_t range;
  
  if (inv->key->keyType == KKT_PrimeRange) {
    start = 0ll;
    end = (UINT64_MAX * EROS_OBJECTS_PER_FRAME) + EROS_NODES_PER_FRAME;
  }
  else if (inv->key->keyType == KKT_PhysRange) {
    start = OID_RESERVED_PHYSRANGE;
    end = (UINT64_MAX * EROS_OBJECTS_PER_FRAME);
  }
  
  range = end - start;


  switch(inv->entry.code) {
  case OC_eros_key_getType:
    COMMIT_POINT();

    /* Notice that this returns AKT_Range for both the prime range key
     * and the more conventional range key. This is actually correct,
     * because both keys obey identical interfaces. The prime range
     * key can be distinguished by the fact that it's reported range
     * from OC_RANGE_QUERY is large. */
    inv->exit.code = RC_OK;
    inv->exit.w1 = AKT_Range;
    return;

  case OC_eros_Range_query:
    {
      COMMIT_POINT();

      inv->exit.w1 = range;
      inv->exit.w2 = (fixreg_t) (range >> 32);

      inv->exit.code = RC_OK;

      return;
    }
    
  case OC_eros_Range_nextSubrange:
    {
      OID subStart;
      OID subEnd;
      OID startOffset;

      COMMIT_POINT();

      startOffset = inv->entry.w2;
      startOffset <<= 32;
      startOffset |= inv->entry.w1;
      startOffset += start;

      if (startOffset >= end) {
	inv->exit.code = RC_eros_Range_RangeErr;
	return;
      }

      /* FIX: This only works for 32-bit subranges */

      objC_FindFirstSubrange(startOffset, end, &subStart, &subEnd);

      range = subEnd - subStart;
      if (range >= (uint64_t) UINT32_MAX)
	range = UINT32_MAX;

      subStart -= start;

      inv->exit.w1 = subStart;
      inv->exit.w2 = (fixreg_t) (subStart >> 32);
      inv->exit.w3 = range;

      inv->exit.code = RC_OK;

      return;
    }
    
  case OC_eros_Range_makeSubrange:
    {
      OID newStart;
      OID newLen;
      OID newEnd;
      Key *key = 0;

      COMMIT_POINT();

      /* This implementation allows for 64 bit offsets, but only 32
       * bit limits, which is nuts! */
      newStart = inv->entry.w2;
      newStart <<= 32;
      newStart |= inv->entry.w1;
      newStart += start;

      /* This is not an issue with the broken interface, but with the
       * 48 bit length representation and a fixed interface we could
       * get caught here by a representable bounds problem. */
      newLen = inv->entry.w3;
      newLen = min(newLen, UINT32_MAX);

      newEnd = newLen + newStart;

      /* REMEMBER: malicious arithmetic might wrap! */
      if ((newStart < start) ||
	  (newStart >= end) ||
	  (newEnd <= start) ||
	  (newEnd > end)) {
	inv->exit.code = RC_eros_Range_RangeErr;
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
    
  case OC_eros_Range_identify:
    {
      OID oid;
      /* Key to identify is in slot 0 */
      Key* key /*@ not null @*/ = inv->entry.key[0];

      key_Prepare(key);


      COMMIT_POINT();

      inv->exit.code = RC_OK;
      
      if (keyBits_IsType(key, KKT_Page) == false &&
	  keyBits_IsType(key, KKT_Node) == false) {
	inv->exit.code = RC_eros_Range_RangeErr;
	return;
      }

#if 0
      /* There is no reason why these should fail to identify. */
      if (key.keyData & SEGMODE_ATTRIBUTE_MASK) {
	inv.exit.code = RC_eros_key_RequestError;
	return;
      }
#endif
	

      oid = key_GetKeyOid(key);

      
      if ( oid < start )
	inv->exit.code = RC_eros_Range_RangeErr;
      else if ( oid >= end )
	inv->exit.code = RC_eros_Range_RangeErr;
      else
	range = oid - start;
      
      /* ALERT: output convention depends on register size! */
      if (keyBits_IsType(key, KKT_Page))
	inv->exit.w1 = eros_Range_otPage;
      else if (keyBits_IsType(key, KKT_Node))
	inv->exit.w1 = eros_Range_otNode;
      else
	inv->exit.w1 = eros_Range_otInvalid;
      
      /* FIX: there is a register size assumption here! */
      assert (sizeof(range) == sizeof(uint64_t));
      assert (sizeof(inv->exit.w2) == sizeof(uint32_t) ||
	      sizeof(inv->exit.w2) == sizeof(uint64_t));
      inv->exit.w2 = range;
      if (sizeof(inv->exit.w2) != sizeof(eros_Range_off_t))
	inv->exit.w3 = (range >> 32);
      else
	inv->exit.w3 = 0;
      
      return;
    }
  case OC_eros_Range_rescind:
    {
      OID oid;
      Key* key /*@ not null @*/ = inv->entry.key[0];
      ObjectHeader *pObject = 0;
   
      key_Prepare(key);
   

      inv->exit.code = RC_OK;
      
      if (keyBits_IsType(key, KKT_Page) == false &&
	  keyBits_IsType(key, KKT_Node) == false) {
	inv->exit.code = RC_eros_Range_RangeErr;
	COMMIT_POINT();

	return;
      }

      /* FIX: Not clear this test should really be here. */
      if (key->keyPerms) {
	inv->exit.code = RC_eros_key_RequestError;
	COMMIT_POINT();

	return;
      }
	  

      oid = key_GetKeyOid(key);

      
#ifdef DEBUG_PAGERANGEKEY
      dprintf(true, "Rescinding page OID 0x%08x%08x\n",
		      (uint32_t) (oid>>32),
		      (uint32_t) oid);
#endif
      
      if ( oid < start ) {
	inv->exit.code = RC_eros_Range_RangeErr;
	COMMIT_POINT();

	return;
      }
      else if ( oid >= end ) {
	inv->exit.code = RC_eros_Range_RangeErr;
	COMMIT_POINT();

	return;
      }


      pObject = key_GetObjectPtr(key);


      objH_FlushIfCkpt(pObject);
  
      objH_MakeObjectDirty(pObject);
     

      COMMIT_POINT();
      
      objH_Rescind(pObject);

#ifdef DBG_WILD_PTR
      if (dbg_wild_ptr)
	check_Consistency("Object rescind()");
#endif
      return;
    }

  case OC_eros_Range_waitPageKey:
  case OC_eros_Range_waitNodeKey:
  case OC_eros_Range_getPageKey:
  case OC_eros_Range_getNodeKey:
    {
      uint64_t offset;
      uint32_t obNdx;
      ObjectHeader *pObj = 0;
      Key *key = 0;
      OID oid;

      inv->exit.code = RC_OK;  /* set the exit code */

      offset =
	(((uint64_t) inv->entry.w2) << 32) | ((uint64_t) inv->entry.w1);

      /* Figure out the OID for the new key: */
      oid = start + offset;

      if (oid > end) {	/* comparing ranges: INclusive */
	dprintf(true, "oid 0x%X top 0x%X\n", oid, end);
	inv->exit.code = RC_eros_Range_RangeErr;
	return;
      }

      obNdx = offset % EROS_OBJECTS_PER_FRAME;
      
     

      key = inv->exit.pKey[0];
      inv->flags |= INV_EXITKEY0;


      if (((inv->entry.code == OC_eros_Range_getPageKey) ||
	   (inv->entry.code == OC_eros_Range_getNodeKey)) &&
	  !objC_HaveSource(oid)) {
	COMMIT_POINT();
	inv->exit.code = RC_eros_Range_RangeErr;
	return;
      }

      switch(inv->entry.code) {
      case OC_eros_Range_getPageKey:
      case OC_eros_Range_waitPageKey:
	{
          uint8_t kt;

	  if (obNdx != 0) {
	    COMMIT_POINT();
	    inv->exit.code = RC_eros_Range_RangeErr;
	    return;
	  }
	
	  kt = KKT_Page;

	  /* If we don't get the object back, it's because of bad frame
	   * type:
	   */

	  pObj = objC_GetObject(oid, ot_PtDataPage, 0, false);


          assert(inv_CanCommit());
	  assert(pObj);

	  COMMIT_POINT();

	  /* It's definitely an object key.  Pin the object it names. */

	  objH_TransLock(pObj);

	  
	  if (key) {
	    /* Unchain the old key so we can overwrite it... */

	    if (key)
	      key_NH_Unchain(key);


	    keyBits_InitType(key, kt);
	    keyBits_SetPrepared(key);
	    key->keyData = EROS_PAGE_BLSS;
	  }
	  break;
	}
      case OC_eros_Range_getNodeKey:
      case OC_eros_Range_waitNodeKey:
	if (obNdx >= DISK_NODES_PER_PAGE) {
	  COMMIT_POINT();
	  inv->exit.code = RC_eros_Range_RangeErr;
	  return;
	}
	
	/* If we don't get the object back, it's because of bad frame
	 * type:
	 */

	pObj = objC_GetObject(oid, ot_NtUnprepared, 0, false);


        assert(inv_CanCommit());
	assert(pObj);

	COMMIT_POINT();

	/* It's definitely an object key.  Pin the object it names. */

	objH_TransLock(pObj);


	if (key) {
	  /* Unchain the old key so we can overwrite it... */

	  if (key)
	    key_NH_Unchain(key);


	  keyBits_InitType(key, KKT_Node);
	  keyBits_SetPrepared(key);
	}
	break;
      }
      
      if (key) {
	key->u.ok.pObj = pObj;
  
	/* Link as next key after object */
	key->u.ok.pObj = pObj;
  
	link_insertAfter(&pObj->keyRing, &key->u.ok.kr);
      }

#ifdef DEBUG_PAGERANGEKEY
      dprintf(true, "pObject is 0x%08x\n", pObj);
#endif
      
      
      return;
    }
    
  case OC_eros_Range_compare:
    {
      Key* key /*@ not null @*/ = inv->entry.key[0];
      OID kstart;
      OID kend;

      key_Prepare(key);


      COMMIT_POINT();

      inv->exit.code = RC_OK;
      inv->exit.w1 = 0;		/* no overlap until proven otherwise */

      if (!keyBits_IsType(key, KKT_Range) && !keyBits_IsType(key, KKT_PrimeRange))
	return;			/* RC_OK in this case probably wrong thing. */

      kstart = key->u.rk.oid;
      kend = key->u.rk.oid + key->u.rk.count;

      if (key->keyType == KKT_PrimeRange) {
	kstart = 0ll;
	kend = UINT64_MAX;
      }
  
      if (kstart >= end || kend <= start)
	return;

      /* They overlap; need to figure out how. */
      inv->exit.w1 = 1;
      inv->exit.w3 = 0;
      
      if (kstart < start) {
	inv->exit.w1 = 3;
	inv->exit.w2 = (fixreg_t) (start - kstart);
	if (sizeof(inv->exit.w2) == sizeof(uint32_t)) /* 32 bit system */
	  inv->exit.w3 = (fixreg_t) ((start - kstart) >> 32);
      }
      else if (kstart == start) {
	inv->exit.w1 = 1;
	inv->exit.w2 = 0;
      }
      else {
	inv->exit.w1 = 2;
	inv->exit.w2 = (fixreg_t) (kstart - start);
	if (sizeof(inv->exit.w2) == sizeof(uint32_t)) /* 32 bit system */
	  inv->exit.w3 = (fixreg_t) ((kstart - start) >> 32);
      }
      return;
    }
  default:
    COMMIT_POINT();

    inv->exit.code = RC_eros_key_UnknownRequest;
    return;
  }

  return;
}
