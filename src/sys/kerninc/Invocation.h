#ifndef __KERNINC_INVOCATION_H__
#define __KERNINC_INVOCATION_H__
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

/* This file requires #include <eros/memory.h> (for bcopy) */
#include <kerninc/Key.h>
#include <kerninc/memory.h>
/* This file requires #include <disk/KeyStruct.hxx> */
/* This file requires #include <kerninc/kernel.h> */

/* There are real speed advantages to the assymetry in these
 * structures.  The entry keys do not need to be copied from their key
 * registers, and every key you don't have to copy saves about 60
 * instructions in the IPC path.
 */
struct EntryBlock {
  fixreg_t code;
  fixreg_t w1;
  fixreg_t w2;
  fixreg_t w3;
  uint8_t *data;
  uint32_t len;

  Key  *key[4];
};

typedef struct EntryBlock EntryBlock;

struct ExitBlock {
  fixreg_t code;
  fixreg_t w1;
  fixreg_t w2;
  fixreg_t w3;
  uint8_t  *data;
  uint32_t len;

  Key      *pKey[4];
};

typedef struct ExitBlock ExitBlock;

INLINE void
entryBlock_init(EntryBlock *thisPtr)
{
  thisPtr->key[0] = 0;
  thisPtr->key[1] = 0;
  thisPtr->key[2] = 0;
  thisPtr->key[3] = 0;
}

INLINE void
exitBlock_init(ExitBlock *thisPtr)
{
  thisPtr->pKey[0] = 0;
  thisPtr->pKey[1] = 0;
  thisPtr->pKey[2] = 0;
  thisPtr->pKey[3] = 0;
}

#ifndef NDEBUG
extern bool InvocationCommitted;
#endif

#if 0
#define INV_RESUMEKEY     0x100u
#endif
#define INV_SCRATCHKEY    0x200u
#define INV_REDNODEKEY    0x400u

#define INV_EXITKEY0      0x001u
#define INV_EXITKEY3      0x008u
#define INV_EXITKEYOTHER  0x00eu

#ifdef OPTION_DDB
extern uint32_t ddb_inv_flags;
#define DDB_INV_all    0x1u
#define DDB_INV_gate   0x2u
#define DDB_INV_return 0x4u
#define DDB_INV_keeper 0x8u
#define DDB_INV_keyerr 0x10u
#define DDB_INV_pflag  0x20u	/* a per-process debug flag has been set */
#endif /* OPTION_DDB */

/* Former member functions of Invocation */
INLINE bool 
inv_CanCommit()
{
  extern bool PteZapped;
  return PteZapped ? false : true;
}

struct Invocation {
  uint32_t flags;
  Key *key;			/* key that was invoked */
#if 0
  Key resumeKey;		/* synthesized resume key */
#endif
  Key scratchKey;		/* for call on red segments; not usually used. */
  Key redNodeKey;		/* for seg keeper; not usually used. */

  bool suppressXfer;		/* should transfer be suppressed? */
  
  uint32_t invType;		/* extracted for efficiency */

  EntryBlock entry;
  ExitBlock exit;

  uint32_t validLen;		/* bytes that can be validly received */
  uint32_t sentLen;		/* amount actually sent */
  
  // #define invKeyType key->keyType
#ifndef invKeyType
  uint8_t invKeyType;		/* extracted from the key for efficiency */
#endif

  Process *invokee;		/* extracted from the key for efficiency */

#if CONVERSION
  Invocation();
#endif

#if 0
  ~Invocation();
#endif
};

#if defined(OPTION_KERN_TIMING_STATS)
uint64_t inv_KeyHandlerCycles[PRIMARY_KEY_TYPES][3];
uint64_t inv_KeyHandlerCounts[PRIMARY_KEY_TYPES][3];
#endif


typedef struct Invocation Invocation;
/* Copy at most COUNT bytes out to the process.  If the process
 * receive length is less than COUNT, silently truncate the outgoing
 * bytes.  Rewrite the process receive count field to indicate the
 * number of bytes actually transferred.
 */
INLINE uint32_t 
inv_CopyOut(Invocation* thisPtr, uint32_t len, void *data)
{
  assert(InvocationCommitted);

  thisPtr->sentLen = len;

  if (thisPtr->validLen < len)
    len = thisPtr->validLen;

  thisPtr->exit.len = len;
  
  if (thisPtr->exit.len)
    bcopy(data, thisPtr->exit.data, thisPtr->exit.len);

  return thisPtr->exit.len;
}

/* Copy at most COUNT bytes in from the process.  If the process
 * send length is less than COUNT, copy the number of bytes in the
 * send buffer.  Return the number of bytes transferred.
 */
INLINE uint32_t 
inv_CopyIn(Invocation* thisPtr, uint32_t len, void *data)
{
  assert(InvocationCommitted);
  
  if (thisPtr->entry.len < len)
    len = thisPtr->entry.len;
  
  if (thisPtr->entry.len)
    bcopy(thisPtr->entry.data, data, len);

  return len;
}

extern Invocation inv;

extern bool PteZapped;

typedef void (*KeyHandler)(Invocation*);
extern void FaultGate(Invocation*);

/* Commit point appears in each invocation where the invocation should
 * now be able to proceed without impediment. At some point in the
 * near future I shall NDEBUG this so as to check the invariant in the
 * debug kernel.
 */

#define COMMIT_POINT() \
   do { \
     extern Invocation inv; \
     inv_Commit(&inv); \
   } while (0) 

/* Former member functions of Invocation */

INLINE bool 
inv_IsActive(Invocation* thisPtr)
{
  return (thisPtr->key != 0);
}

void inv_InitInv(Invocation *thisPtr);

bool inv_IsInvocationKey(Invocation* thisPtr, const Key *);

void inv_RetryInvocation(Invocation* thisPtr);

INLINE void 
inv_MaybeDecommit(Invocation* thisPtr)
{
  /* CONVERSION */
  if (inv_CanCommit() == false)
    inv_RetryInvocation(thisPtr);
  /* END CONVERSION */
}

void inv_Commit(Invocation* thisPtr);

#ifndef NDEBUG
  bool inv_IsCorrupted(Invocation* thisPtr);
#endif

void inv_Cleanup(Invocation* thisPtr);

#if defined(OPTION_KERN_TIMING_STATS)
void inv_ZeroStats();
#endif

void inv_BootInit();

INLINE void 
inv_SetExitKey(Invocation* thisPtr, uint32_t ndx, Key* k /*@ not null @*/)
{
#ifndef NDEBUG
  assert(InvocationCommitted);
#endif

  if (thisPtr->exit.pKey[ndx])
    key_NH_Set(thisPtr->exit.pKey[ndx], k);

#if 0
  /* This will compile into |= of constant after inlining: */
  flags |= (1u << ndx);
#endif
}

#define INVTYPE_ISVALID(x) ((x) < IT_NUM_INVTYPES)

#endif /* __KERNINC_INVOCATION_H__ */
