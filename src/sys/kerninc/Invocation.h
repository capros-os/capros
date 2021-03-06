#ifndef __KERNINC_INVOCATION_H__
#define __KERNINC_INVOCATION_H__
/*
 * Copyright (C) 1998, 1999, Jonathan S. Shapiro.
 * Copyright (C) 2005, 2006, 2007, 2008, Strawberry Development Group.
 *
 * This file is part of the CapROS Operating System,
 * and is derived from the EROS Operating System.
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

#include <kerninc/Key.h>
#include <eros/Invoke.h>
/* This file requires #include <kerninc/kernel.h> */

/* The "Entry Block" describes the invocation parameters for entry to the
   kernel (exit from the user code). 
   The "Exit Block" describes the invocation parameters for exit from 
   the kernel (reentry to the user code).
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
  uint32_t rcvLen;
  uint8_t * data;

  Key      *pKey[4];
};

typedef struct ExitBlock ExitBlock;

#ifndef NDEBUG
extern bool InvocationCommitted;
extern bool ReturneeSetUp;
extern bool traceInvs;
#endif

// Values for Invocation.flags:
#define INV_SCRATCHKEY    0x200u
#define INV_KEEPERARG     0x400u

#ifdef OPTION_DDB
extern uint32_t ddb_inv_flags;
#define DDB_INV_all    0x1u
#define DDB_INV_gate   0x2u
#define DDB_INV_return 0x4u
#define DDB_INV_keeper 0x8u
#define DDB_INV_keyerr 0x10u
#define DDB_INV_pflag  0x20u	/* a per-process debug flag has been set */
#endif /* OPTION_DDB */

struct Invocation {
  uint32_t flags;
  Key *key;			/* key that was invoked */
#if 0
  Key resumeKey;		/* synthesized resume key */
#endif
  Key scratchKey;		/* for call on red segments; not usually used. */
  Key keeperArg;		/* for keeper; not usually used. */

  uint32_t invType;		/* extracted for efficiency */
#define IT_KeeperCall 6		// in this field only, not in a Message

  EntryBlock entry;
  ExitBlock exit;

  uint32_t sentLen;		/* amount sent (may be more than received) */
  
  Process *invokee;		/* extracted from the key for efficiency */
};

typedef struct Invocation Invocation;

void BeginInvocation(void);
void inv_CopyOut(Invocation* thisPtr, uint32_t len, void *data);
uint32_t inv_CopyIn(Invocation* thisPtr, uint32_t len, void *data);
void inv_GetReturnee(Invocation * inv);
void ReturnMessage(Invocation * inv);
void inv_SetupExitBlock(Invocation * inv);
void inv_InvokeGateOrVoid(Invocation * inv, Key * invKey);

/* This is the only instance of Invocation.
   I think the plan was to have one instance for each CPU. */
extern Invocation inv;
struct Activity;
extern struct Activity * activityToRelease;

typedef void (*KeyHandler)(Invocation*);
void GateKey(Invocation *);
void VoidKey(Invocation *);
void MemoryKey(Invocation *);

/* Commit point appears in each invocation where the invocation should
 * now be able to proceed without impediment. It may Yield. 
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

void inv_RetryInvocation(Invocation* thisPtr) NORETURN;

void inv_Commit(Invocation* thisPtr);

#ifdef OPTION_DDB
  bool inv_IsInvocationKey(Invocation * thisPtr, const Key *);
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
INLINE bool
invType_IsCall(unsigned int t)
{
  return t & IT_Call;	// IT_Call, IT_PCall, or IT_KeeperCall
}

INLINE bool
invType_IsPrompt(unsigned int t)
{
  return t & IT_PReturn;	// low order bit means prompt
}

#endif /* __KERNINC_INVOCATION_H__ */
