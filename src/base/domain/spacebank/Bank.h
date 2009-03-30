/*
 * Copyright (C) 1998, 1999, Jonathan Adams.
 * Copyright (C) 2001, The EROS Group.
 * Copyright (C) 2007, 2008, 2009, Strawberry Development Group.
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


#ifndef BANK_H
#define BANK_H

#include "AllocTree.h"

struct Message;
typedef uint32_t BankPrecludes;

/* note:  There can only be 15 distinct precludes, as all start keys >=
   0x8000 are reserved for other uses. (like the brand!) */

#define BANKPREC_MASK             (0x03u)
#define BANKPREC_NUM_PRECLUDES    (BANKPREC_MASK + 1)

struct Bank_MOFrame {
  uint64_t frameOid;
  uint32_t frameMap;	// bitmap for objects in the frame, 1 == free
};

typedef struct Bank Bank;       
struct Bank {
  Bank *parent;
  Bank *nextSibling;
  Bank *firstChild;

  bool  exists[BANKPREC_NUM_PRECLUDES];
  OID   limitedKey[BANKPREC_NUM_PRECLUDES];
                     /* holds the OIDs of the nodes for the various
		      * variants on this bank.
		      */

  TREE allocTree;
  
  uint64_t limit;
  uint64_t allocCount; /* == #allocated frames */

  struct Bank_MOFrame nodeFrame;

  uint64_t allocs[capros_Range_otNUM_TYPES];
  uint64_t deallocs[capros_Range_otNUM_TYPES];
};

extern Bank bank0;		/* The primordial allocate-only bank. */
extern Bank primebank;		/* The prime bank. */
extern Bank verifybank;         /* A zero-limit bank which can only be
				   used to validate banks */

void bank_init(void);
/* bank_init:
 *     Initialized the Bank structures etc.
 */

result_t
GetCapAndRetype(capros_Range_obType obType,
  OID offset, cap_t kr);

uint32_t
bank_ReserveFrames(Bank *bank, uint32_t count);
/* bank_ReserveFrames:
 *     Logically, this bumps the number of frames allocated by /bank/
 *   after making sure that that does not cross the limits of this
 *   bank and its parents.
 *
 *     Returns RC_OK on success, -- /count/ frames are reserved, and
 *                                  you are responible for unreserving
 *                                  the frames when done with them.
 *             RC_SB_LimitReached on failure (always due to a
 *                                limit somewhere) -- No frames are resered.
 */

void
bank_UnreserveFrames(Bank *bank, uint32_t count);
/* bank_UnreserveFrames:
 *     Returns /count/ frames to /bank/ -- this should be called any
 *   time a frame is returned to the ObjSpace.  This never fails
 *   (though it will panic if somehow you unreserve more frames than
 *   you reserved.)
 */

uint32_t
BankSetLimits(Bank * bank, const capros_SpaceBank_limits * newLimits);
/* BankSetLimits:
 */

uint32_t
BankGetLimits(Bank * bank, /*OUT*/ capros_SpaceBank_limits * getLimit);
/* BankGetLimits:
 */

uint32_t
bank_MarkAllocated(uint32_t type, uint64_t oid);
/* bank_MarkAllocated:
 *     Marks /oid/ as an allocated object of type /type/ in the
 *   PRIMEBANK.
 *
 *     Returns 0 on success,
 *             1 if the object is already marked allocated,
 *             2 on failure
 */

BankPrecludes PrecludesFromInvocation(struct Message *argMsg);
#define PrecludesFromInvocation(argMsg) \
  ( (BankPrecludes) (argmsg)->rcv_keyInfo )
/* LimitsFromInvocation:
 *     Given invocation message /argMsg/, recovers limits field.
 *
 *     Cannot fail, because spacebank fabricates the red seg nodes
 *     with proper values at all times. (also, it's a macro)
 */

uint32_t
BankAllocObject(Bank * bank, uint8_t type, uint32_t kr, /*OUT*/ OID * oidRet);
/* BankAllocObject:
 *     Allocates an object of type /type/ from /bank/, putting
 *     the key into kr and the OID into *oid.
 *
 *     Returns RT_OK on success, error code if some error occurs.
 */

uint32_t
BankDeallocObject(Bank * bank, uint32_t kr);
/* BankDeallocObject:
 *     Deallocates an object that was allocated from /bank/.
 *
 *     An object will fail to deallocate if it is not an object key,
 *   it was not allocated from /bank/, or if it is not a strong
 *   (read/write) key to the object. 
 */

uint32_t
bank_deallocOID(Bank * bank, uint8_t type, OID oid);
/* bank_deallocOID:
 *     Returns the object described by /oid/ with type /type/ to the
 *     available object pool.
 */

uint32_t
BankCreateKey(Bank *bank, BankPrecludes limits, uint32_t kr);
/* BankCreateKey:
 *     Creates a key in register /kr/ to /bank/ with /limits/.  Note
 *     that this key names a red segment node.
 *
 *     Returns RC_OK on success.
 */

uint32_t
BankCreateChild(Bank *bank, uint32_t kr);
/* BankCreateChild:
 *     Creates a new child of /bank/, and creates an unrestricted key
 *   to it in /kr/.
 *
 *     Returns RC_OK on success,
 *     Returns RC_SB_LimitReached if the limits on /bank/ preclude
 *   adding a child.
 *     Returns RC_SB_OutOfSpace if there is no more space on disk for
 *   the new child bank.
 */

uint32_t
BankDestroyBankAndStorage(Bank *bank, bool andStorage);
/* BankDestroyBankAndStorage:
 *     Destroys bank and all of its children.  If /andStorage/ is
 *   false, the allocated storage is put into the control of /bank/'s
 *   parent. If /andStorage/ is true, all the storage allocated by
 *   both /bank/ and its children is rescinded.
 *
 *     All keys referencing the destroyed banks are useless, and will
 *   be turned into void keys. 
 *
 *     Returns RC_OK on success.
 */

void
BankPreallType(Bank *bank,
	       uint32_t type,
	       uint64_t startOID,
	       uint64_t number);
/* BankPreallType:
 *     Marks /number/ objects of type /type/ starting at /startOID/
 *   (which should be a frame OID) as allocated in /bank/.
 */

#endif /* BANK_H */

