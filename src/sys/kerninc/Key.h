#ifndef __KERNINC_KEY_H__
#define __KERNINC_KEY_H__
/*
 * Copyright (C) 1998, 1999, 2001, Jonathan S. Shapiro.
 * Copyright (C) 2006, 2007, Strawberry Development Group.
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
/* This material is based upon work supported by the US Defense Advanced
Research Projects Agency under Contract No. W31P4Q-07-C-0070.
Approved for public release, distribution unlimited. */


/* Dependencies: */
#include <disk/KeyStruct.h>
#include <kerninc/ObjectHeader.h>

/* 
 * Key.  A Key is a capability, in the sense of Hydra or the Sigma-7.
 * EROS is a pure capability system, which means that Keys are the
 * only namespace known to the system.  They are therefore the *only*
 * way that one can refer to an object within EROS.
 * 
 * A Key names a Page (see Page.hxx), a Node (see Node.hxx), a Number,
 * or one of the EROS devices or miscellaneous kernel tools.
 * 
 * Keys have an on-disk and in-core form.  Both are 3 words.  A valid
 * in-core key points to an in-core object.
 * 
 * Before being used, a Key must be converted to in-core form, but
 * this is done in a lazy fashion, so many of the Keys that happen to
 * be in core remain encoded in the on-disk form.  A Key that has been
 * converted for use in its in-core form is said to be "prepared."
 * 
 * Number, Device, and Miscellaneous keys have the same form on disk
 * as in core:
 * 
 *      +--------+--------+--------+--------+
 *      |             Data[31:0]            |
 *      +--------+--------+--------+--------+
 *      |             Data[63:32]           |
 *      +--------+--------+--------+--------+
 *      |000 Type|    Data[87:64]           |
 *      +--------+--------+--------+--------+
 * 
 * NOTE: Reorder per machine's natural word order for long long
 * 
 * In the in-core form, the P bit is always set (1).  This eliminates
 * the need to special case the check for Key preparedness for this
 * class of Keys.
 * 
 * The Type field encodes only primary keys, and is always 5 bits
 * wide. In the Number key, the Data slots hold the numeric value.  In
 * Device and Miscellaneous key, they differentiate the called device
 * or tool.
 * 
 * The Sanity field is used by the system sanity checker to verify
 * that the world is sane.  It's value should not be examined or
 * counted on.
 * 
 * 
 * Page and Node keys (all other cases) are a bit different in core
 * then they are on disk:
 * 
 *      +--------+--------+--------+--------+
 *      |          Allocation Count         |    PREPARED IN CORE
 *      +--------+--------+--------+--------+
 *      |   Pointer to ObHeader for object  |
 *      +--------+--------+--------+--------+
 *      |Phh Type|Datauint8_t|    OID[47:32]   |
 *      +--------+--------+--------+--------+
 * 
 *      +--------+--------+--------+--------+
 *      |          Allocation Count         |    ON DISK
 *      +--------+--------+--------+--------+
 *      |             OID[31:0]             |
 *      +--------+--------+--------+--------+
 *      |000 Type|Datauint8_t|    OID[47:32]   |
 *      +--------+--------+--------+--------+
 * 
 * For an interpretation of the fields, see eros/KeyType.hxx.
 * 
 * The Datauint8_t is an 8 bit value handed to the recipient when the Key
 * is invoked.  It allows the recipient to handle multiple client
 * classes by providing each client class with a unique Datauint8_t.
 * 
 * The ObHeader Pointer is an in-core pointer to the object denoted by
 * the Key.
 * 
 * The Allocation Count is used to rescind keys to pages and notes.
 * This is described in the rescind logic for Keys.
 * 
 */

typedef struct KeyBits Key;

extern Key key_VoidKey;

/* Former member functions of Key */

void key_DoPrepare(Key* thisPtr);

#ifndef NDEBUG
bool key_IsValid(const Key* thisPtr);
#endif

/* key_Prepare may Yield. */
#ifdef NDEBUG
INLINE void 
key_Prepare(Key* thisPtr)
{
  if (keyBits_IsUnprepared(thisPtr))
    key_DoPrepare(thisPtr);
  
  if ( keyBits_NeedsPin(thisPtr) )
    objH_TransLock(thisPtr->u.ok.pObj);
}
#else
void key_Prepare(Key* thisPtr);
#endif

/* ALL OF THE NH ROUTINES MUST BE CALLED ON NON-HAZARDED KEYS */

/* Unprepare key with intention to overwrite immediately, so
 * no need to remember that the key was unprepared.
 */

INLINE void 
key_NH_Unchain(Key* thisPtr)
{
  keyBits_Unchain(thisPtr);
}

/* Called from Node::Unprepare after hazards are cleared.  Also used
 * by Thread unprepare, which is why it is in the key rather than
 * the node.
 */
void key_NH_Unprepare(Key* thisPtr);

/* KS_Set now checks for need to unchain internally. */
void key_NH_Set(KeyBits *thisPtr, KeyBits* kb);

void key_SetToNumber(KeyBits *thisPtr, uint32_t hi, uint32_t mid, uint32_t lo);

INLINE void 
key_NH_SetToVoid(Key* thisPtr)
{
#ifdef __KERNEL__
  keyBits_Unchain(thisPtr);
#endif

  keyBits_InitToVoid(thisPtr);
}

OID  key_GetKeyOid(const Key* thisPtr);
ObjectHeader *key_GetObjectPtr(const Key* thisPtr);
uint32_t key_GetAllocCount(const Key* thisPtr);

void key_Print(const Key* thisPtr);

INLINE void 
key_NH_RescindKey(Key* thisPtr)
{
#ifdef __KERNEL__
  keyBits_Unchain(thisPtr);
#endif

  keyBits_InitToVoid(thisPtr);
}

#ifdef OPTION_OB_MOD_CHECK
uint32_t key_CalcCheck(Key* thisPtr);
#endif

/* Orders on Various Key Types:
 * 
 * KtNumber:
 * 
 *   KeyType
 *   GetData(small);   aslong
 *   GetData(medium);  aslonglong
 *   GetData(large);   asNumber ;-) (spec'd to return as 16 bytes)
 * 
 * KtDataPage:
 * 
 *   Return a read-only key to same page (ROpageKey) - impl in
 *   	Datauint8_t
 *   Read(start, buf)
 * 	DOCNOTE: logically returns entire page, wrapping at the end.
 * 	    this is bounded by the buffer size in implementation.
 *   Write(start, buf) (not on ROkeys)
 * 	DOCNOTE: logically returns entire page, wrapping at the end.
 * 	    this is bounded by the buffer size in implementation.
 *   Clear() - very fast, but don't play poker with Norm. Also, don't
 *   	tell the marketeers, who will subsequently tell the public
 * 	that EROS can clear any size page in one instruction on any
 * 	architectrue. 
 * 
 * KtNode:
 *   get(n,nr) - fetch key in slot N into domain's node register nr.
 *   swap(n,nr) - swap key in slot N with domain's node register nr.
 * 	Don't have to accept the responding key, so this subsumes
 * 	set().
 * 	LIBNOTE: should have a set() in the access library. Blech.
 * 
 *   asMeter()		- obvious conversions (Datauint8_t always 0)
 *   asSegment(Datauint8_t)
 *   asDomain(Datauint8_t)
 *   asTimer(Datauint8_t)
 *   asNode(Datauint8_t)
 *   asFetch(Datauint8_t)
 *   asSensory(Datauint8_t) FIX - norm thinks there's a virtualization
 * 	issue buried here.
 *   datauint8_t() - return the data byte in the return code. (?) Maybe
 * 	should be in register-string
 *   clear() - c.f. page clear. Same problem with marketeers.
 *   numerate(start, buf) - place a number key in the node. In the old
 *      logic, you could create multiple number keys in sequence,
 *      which was useful.  We do not do that for now.
 * 
 * KtMeter:
 *   no protocol outside the Kernel
 *   might have stuff for key cacheing.
 * 
 * KtSegment:
 * 
 *   getROSegment() - does this require dynamic allocation?
 *   read()
 *   write()
 * 
 *   send message to keeper. standard ops on keeper are:
 *   
 * KtDomain:
 *   Bill suggested dividing this into MI/MD parts.
 * 
 *   Machine Independent:
 * 
 *   SetAddressSlot(KeyReg):  - alternative: set both. Option:
 *       give new PC.
 *   SetMeterSlot(KeyReg):
 *   SetKeeperSlot(KeyReg):
 *   SetSymbolSlot():
 * 
 *   GetAddressSlots(KeyReg):
 *   GetMeterSlot(KeyReg):
 *   GetKeeperSlot(KeyReg):
 *   GetSymbolSlot:
 * 
 *   GetStartKey(Datauint8_t):
 *   Reset() - makes the domain available
 *   Stop() => (ResumeKey,GeneralRegs)
 * 
 *   FetchKeyReg(nkr):
 *   SwapKeyReg(nkr):
 * 
 *   GetDomainInfo() - returns general information about the Domain
 *       implementation.
 * 
 *   SetPC():
 *   GetPC():
 * 
 *   SingleStep() - step a single instruction, may fail. Arch defined
 *       on delay slots.
 * 
 *   NOTE: canNOT get the brand or the general registers nodes.
 * 
 *   Machine Dependent:
 * 
 *   Get{General,Float}Registers():
 *   Set{General,Float}Registers():
 * 
 *   GetTrapCode():
 *   SetTrapCode():
 * 
 * KtTimer:
 * 
 *   SetInterval(interval)
 *   SleepFor(interval) - sets interval too
 *   Sleep()		- useful for rendevous
 * 
 * KtStart, KtResume:
 * 
 *   Sends message to the domain itself.
 * 
 * KtFetch:
 *   get(n,nr) - fetch key in slot N into domain's node register nr.
 *   swap(n,nr):
 *   asMeter():
 *   asSegment(Datauint8_t):
 *   asDomain(Datauint8_t):
 *   asTimer(Datauint8_t):
 *   asNode(Datauint8_t):
 * 	these always fail in kernel. Should this be accesViolation or
 *      badOrderCode? 
 * 
 *   asFetch(Datauint8_t) - see Node protocol? What current designs does
 *   	this break?
 *   asSensory(Datauint8_t) - if I can get a fetch key, I can get a sense
 * 	key.
 * 
 *   datauint8_t() - return the data byte in the return code. (?) Maybe
 * 	should be in register-string
 *   clear():
 *   numerate(start, buf):
 * 	- fails, either accessViolation or invalidOrder.
 * 
 * KtRange:
 * 	Range key argument CDAs are expressed as offsets relative to
 * 		the start of the range controlled by the range key.
 *   getPageKey(relative CDA) - policy issue here. Should bank be
 * 	cognizant of disk partitions?
 *   rescindPageKey(relative CDA)
 *   makeSubRangeKey(relative start, count) - NEW and IMPROVED! Well,
 * 	new at least.
 *   validCDA(relative CDA)
 *   maxCDAQuery(buf)
 * 
 * KtHook:
 *   No protocol - just a kernel tool
 * 
 * KtDevice:
 *   Block:
 *     read()
 *     write()
 *     seek()
 *     reset()
 *     init()
 *     control()
 *     size()
 *     shutdown()
 * 
 *   Disk
 *   Screen
 *   WallClock
 *   OPTION_SCSI[0..7] - multimaster; where does this belong?
 * 
 *   Asynch:
 *     read()
 *     write()
 *     ?seek()
 *     reset()
 *     init()
 *     control()
 *     size()
 *     shutdown()
 * 
 *   Keyboard
 *   Mouse
 *   Serial
 *   Parallel
 *   OPTION_SCSI[0..7]
 * 
 *   ?WallClock
 *   ?DiskPackManager - manages removable media
 * 
 * KtMisc:
 *   DomainCreator
 *   Returner
 *   Discrim
 *   DataCreater
 *   DeviceCreator - creator/rescinder of device keys
 *   Journaler
 *   Checkpointer
 *   Shutdown/Reboot
 *   ErrorKey - used by error log daemon
 *   ?ItimerCreator - Wall banging issue
 *   ?OPTION_SCSIKeyCreator - Avoid this if can be done dynamically. Lets me
 *       tell the kernel about the ones it can't decipher.
 * 
 *   Tool specific
 * 
 */
#endif /* __KERNINC_KEY_H__ */
