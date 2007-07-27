#ifndef __DISK_KEYSTRUCT_H__
#define __DISK_KEYSTRUCT_H__
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

#include <disk/ErosTypes.h>
#include <eros/StdKeyType.h>
#ifdef __KERNEL__
#include <kerninc/Link.h>
#endif

/* 
 * KeyType - the major key types supported in EROS. Keys also have a subtype
 * For example, the Process Tool is KtMisc, KsProcTool.
 * 
 */

/* If these numbers change, the dispatch table in kerninc/Invoke.cxx
 * must be changed as well.  Key preparation is a very frequent
 * operation in the kernel, so these numbers are ordered in such a way
 * as to take advantage of a representation pun.  Key types having a
 * prepared form appear starting at zero.  In the actual key data
 * structure, the first byte of the key is layed out as:
 * 
 *   (bit 7)   hazard[2] :: prepared :: keytype[5]   (bit 0)
 * 
 * Both the hazard field and the prepared field are 0 on an unprepared
 * key.  This allows a combined test on the byte value to determine
 * which keys need to be prepared and which keys need to be
 * validated.  If the byte value is <= LAST_OBJECT_KEYTYPE, then the
 * key is NOT prepared and needs to be.  If the key is prepared, it
 * needs to be validated. 
 */

/* These REALLY need to get defined in StdKeyTypes, lest they drift. */
#define LAST_GATE_KEYTYPE KKT_Resume
#define LAST_OBJECT_KEYTYPE KKT_Page
#ifndef NDEBUG
#define FIRST_MISC_KEYTYPE KKT_KeyBits
#endif

typedef unsigned KeyType;

enum Priority {
  pr_Never = 0,			/* lower than idle, therefore never runs */
  pr_Idle = 1,
  pr_Normal = 8,
  pr_Reserve = 14,              /* for reserve scheduler */
  pr_High = 15
};
typedef enum Priority Priority;


/* The ObjectHeader pointer declaration is unused in the on-disk
 * key format, but including it here is harmless and simplifies
 * the declarations in Key.hxx.
 */

struct ObjectTable;

typedef struct KeyBits KeyBits;

/*
When KFL_RHAZARD is set, we say the key is "read-hazarded".
This means that the logical value of the key is different from
the actual value.
Before fetching the key you must get the logical state from wherever it is
and restore the actual value.
The following cases occur:
In a node prepared as a process root, slots for register values.
In a node prepared as a process key registers, slots for key registers
  (not necessarily prepared).

When KFL_WHAZARD is set, we say the key is "write-hazarded". 
This means that there is state elsewhere that depends on the key.
Before changing the key you must clear that other state. 
The following cases occur:
In a node prepared as a process root, slots for register values.
In a node prepared as a process root, the ProcSched slot.
In a node prepared as a process root, the ProcGenKeys slot.
In a node prepared as a process root, the ProcAddrSpace slot.
In a node prepared as a process key registers, slots for key registers
  (not necessarily prepared).
In a node prepared as a segment, a slot used to build a mapping table entry.

node_ClearHazard() handles all these cases.
 */

#define KFL_PREPARED       0x4
#define KFL_RHAZARD        0x2
#define KFL_WHAZARD        0x1
#define KFL_HAZARD_BITS    0x3
#define KFL_ALL_FLAG_BITS  0x7

/* These values apply only for segmode keys. */
// This duplicates definitions in Memory.idl.
#define capros_Memory_readOnly	8
#define capros_Memory_noCall	4
#define capros_Memory_weak	2
#define capros_Memory_opaque	1

struct KeyBits {
  union {
    struct {
      OID     oid;
      ObCount count;
    } unprep;

#ifdef __KERNEL__
    struct {
      Link kr;
      struct ObjectHeader *pObj;
    } ok;
    struct {
      Link kr;
      struct Process *pContext;
    } gk;
#endif

    struct {			/* NUMBER KEYS */
      uint32_t value[3];
    } nk;

    /* Priority keys use the keyData field, but no other fields. */
    
    /* Miscellaneous Keys have no substructure, but use the 'keyData'
     * field.
     */

    /* Device Keys (currently) have no substructure, but use the
     * 'keyData' field. 
     */

    /* It is currently true that oidLo and oidHi in range keys and
     * object keys overlap, but not important that they do so.
     */

    struct {			/* RANGE KEYS */
      OID     oid;
      uint32_t    count;
    } rk;      
    
    struct {			/* DEVICE KEYS */
      uint16_t devClass;
      uint16_t devNdx;
      uint32_t devUnit;
    } dk;
  } u;

  /* EVERYTHING BELOW HERE IS GENERIC TO ALL KEYS.  Structures above
   * this point should describe the layout of a particular key type
   * and should occupy exactly three words.
   */
  
#ifdef BITFIELD_PACK_LOW
  uint8_t keyType;
  uint8_t keyFlags : 3;
  uint8_t keyPerms : 5;
  uint16_t keyData;
#else
#error "verify bitfield layout" 
#endif
} ;

#ifdef __cplusplus
extern "C" {
#endif
/* Former member functions of KeyBits */

INLINE uint32_t 
keyBits_GetBlss(const KeyBits *kb /*@ not null @*/)
{
  return kb->keyData;
}

INLINE void 
keyBits_SetBlss(KeyBits *kb /*@ not null @*/, uint32_t blss)
{
  kb->keyData = blss;
}

INLINE KeyType
keyBits_GetType(const KeyBits *thisPtr)
{
  return (KeyType) thisPtr->keyType;
}

INLINE bool 
keyBits_IsType(const KeyBits *thisPtr, uint8_t t)
{
  return (keyBits_GetType(thisPtr) == t);
}

INLINE bool 
keyBits_IsHazard(const KeyBits *thisPtr /*@ not null @*/)
{
  return (thisPtr->keyFlags & KFL_HAZARD_BITS);
}
  
INLINE void 
keyBits_UnHazard(KeyBits *thisPtr /*@ not null @*/)
{
  thisPtr->keyFlags &= ~KFL_HAZARD_BITS;
}

INLINE void 
keyBits_SetPrepared(KeyBits *thisPtr /*@ not null @*/)
{
  thisPtr->keyFlags |= KFL_PREPARED;
}
  
INLINE void 
keyBits_SetUnprepared(KeyBits *thisPtr /*@ not null @*/)
{
  thisPtr->keyFlags &= ~KFL_PREPARED;
}
  
INLINE void 
keyBits_SetRdHazard(KeyBits *thisPtr /*@ not null @*/)
{
  thisPtr->keyFlags |= KFL_RHAZARD;
}
  
INLINE void 
keyBits_SetWrHazard(KeyBits *thisPtr /*@ not null @*/)
{
  thisPtr->keyFlags |= KFL_WHAZARD;
}
  
INLINE void 
keyBits_SetRwHazard(KeyBits *thisPtr /*@ not null @*/)
{
  thisPtr->keyFlags |= KFL_RHAZARD|KFL_WHAZARD;
}

INLINE bool 
keyBits_IsRdHazard(KeyBits *thisPtr /*@ not null @*/)
{
  return (thisPtr->keyFlags & KFL_RHAZARD);
}
  
INLINE bool 
keyBits_IsWrHazard(KeyBits *thisPtr /*@ not null @*/)
{
  return (thisPtr->keyFlags & KFL_WHAZARD);
}
    
INLINE void 
keyBits_InitType(KeyBits *thisPtr /*@ not null @*/, uint8_t t)
{
  thisPtr->keyType = (KeyType)t; /* not hazard, not prepared */
  /* FIXME: With keyFlags and keyPerms as separate bit fields,
     this is slow. */
  thisPtr->keyFlags = 0;
  thisPtr->keyPerms = 0;
  thisPtr->keyData = 0;
}
  
INLINE void 
keyBits_SetType(KeyBits *thisPtr /*@ not null @*/, KeyType kt)
{
  thisPtr->keyType = kt;
}

INLINE void 
keyBits_SetRestrictions(KeyBits * thisPtr, unsigned int restrictions)
{
  thisPtr->keyPerms |= restrictions;
}
  
INLINE bool 
keyBits_IsNoCall(const KeyBits *thisPtr)
{
  return (thisPtr->keyPerms & (capros_Memory_noCall|capros_Memory_weak));
}
  
INLINE void 
keyBits_SetNoCall(KeyBits *thisPtr)
{
  thisPtr->keyPerms |= capros_Memory_noCall;
}

INLINE bool 
keyBits_IsReadOnly(const KeyBits *thisPtr)
{
  return (thisPtr->keyPerms & capros_Memory_readOnly);
}

INLINE bool 
keyBits_IsOpaque(const KeyBits * thisPtr)
{
  return (thisPtr->keyPerms & capros_Memory_opaque);
}

INLINE bool 
keyBits_IsWeak(const KeyBits *thisPtr)
{
  return (thisPtr->keyPerms & capros_Memory_weak);
}

INLINE void 
keyBits_SetWeak(KeyBits *thisPtr)
{
  thisPtr->keyPerms |= capros_Memory_weak;
}
  
INLINE bool 
keyBits_IsPrepared(const KeyBits *thisPtr)
{
  return (thisPtr->keyFlags & KFL_PREPARED);
}

INLINE bool 
keyBits_IsUnprepared(const KeyBits *thisPtr)
{
  return ! (thisPtr->keyFlags & KFL_PREPARED);
}

INLINE bool 
keyBits_IsPreparedResumeKey(const KeyBits *thisPtr)
{
  /* Resume keys are never hazarded... */
  return (keyBits_IsType(thisPtr, KKT_Resume) && keyBits_IsPrepared(thisPtr));
}

INLINE void 
keyBits_SetReadOnly(KeyBits *thisPtr)
{
  thisPtr->keyPerms |= capros_Memory_readOnly;
}

/* In memory keys, l2g is in the first byte of keyData.
Only 7 bits are required. */
INLINE unsigned int
keyBits_GetL2g(KeyBits * thisPtr)
{
  return * (uint8_t *) &thisPtr->keyData;
}

// assert(l2g <= 64 && l2g >= EROS_PAGE_ADDR_BITS);
INLINE void
keyBits_SetL2g(KeyBits * thisPtr, unsigned int l2g)
{
  * (uint8_t *) &thisPtr->keyData = l2g;
}

/* In memory keys, guard is in the second byte of keyData.
We could squeeze more bits by taking the high bit of the first byte,
bits in the keyPerms field, and perhaps bits from the keyType field.
Also, l2g might be squeezed into 6 bits if offset by 1. */
INLINE unsigned int
keyBits_GetGuard(KeyBits * thisPtr)
{
  return ((uint8_t *) &thisPtr->keyData)[1];
}

// assert(guard < 256);
INLINE void
keyBits_SetGuard(KeyBits * thisPtr, unsigned int guard)
{
  ((uint8_t *) &thisPtr->keyData)[1] = guard;
}
 

INLINE bool 
keyBits_IsGateKey(const KeyBits *thisPtr)
{
  return (keyBits_GetType(thisPtr) <= LAST_GATE_KEYTYPE);
}
  
INLINE bool 
keyBits_IsObjectKey(const KeyBits *thisPtr)
{
  return (keyBits_GetType(thisPtr) <= LAST_OBJECT_KEYTYPE);
}

INLINE bool 
keyBits_NeedsPrepare(const KeyBits *thisPtr)
{
  return keyBits_IsUnprepared(thisPtr);
}
  
INLINE bool 
keyBits_IsPreparedObjectKey(const KeyBits *thisPtr)
{
  return (keyBits_IsObjectKey(thisPtr) && keyBits_IsPrepared(thisPtr));
}
      
INLINE bool 
keyBits_NeedsPin(const KeyBits *thisPtr)
{
  return (keyBits_IsObjectKey(thisPtr) && !keyBits_IsGateKey(thisPtr));
}
  
#ifndef NDEBUG
INLINE bool 
keyBits_IsMiscKey(const KeyBits *thisPtr)
{
  return (keyBits_GetType(thisPtr) >= FIRST_MISC_KEYTYPE);
}
#endif

INLINE bool
keyBits_IsNodeKeyType(const KeyBits *thisPtr)
{
  switch (keyBits_GetType(thisPtr)) {
  case KKT_Wrapper:
  case KKT_Node:
  case KKT_Segment:
  case KKT_Process:
  case KKT_Start:
  case KKT_Resume:
  case KKT_Forwarder:
  case KKT_GPT:
    return true;
  default:
    return false;
  }
}

INLINE bool 
keyBits_IsSegKeyType(const KeyBits *thisPtr)
{
  return (keyBits_IsType(thisPtr, KKT_GPT));
}

INLINE bool 
keyBits_IsDataPageType(const KeyBits *thisPtr)
{
  return (keyBits_IsType(thisPtr, KKT_Page) 
#ifdef KKT_TimePage
          || keyBits_IsType(thisPtr, KKT_TimePage)
#endif
          );
}

INLINE bool 
keyBits_IsSegModeType(const KeyBits *thisPtr)
{
  return (keyBits_IsSegKeyType(thisPtr) || keyBits_IsDataPageType(thisPtr));
}

/* IsVoidKey does not check for a rescinded key, only that the
   current representation is void. */
INLINE bool 
keyBits_IsVoidKey(const KeyBits *thisPtr)
{
  return keyBits_IsType(thisPtr, KKT_Void);
}
  
#ifdef __KERNEL__
INLINE void
keyBits_Unchain(KeyBits *thisPtr)
{
  if ( keyBits_IsPreparedObjectKey(thisPtr) ) {
    link_Unlink(&thisPtr->u.ok.kr);
  }
}
#endif

INLINE void
keyBits_InitToVoid(KeyBits *thisPtr)
{
  keyBits_InitType(thisPtr, KKT_Void);
  thisPtr->u.nk.value[0] = 0;
  thisPtr->u.nk.value[1] = 0;
  thisPtr->u.nk.value[2] = 0;
}

#ifdef __cplusplus
}
#endif


#endif /* __DISK_KEYSTRUCT_H__ */
