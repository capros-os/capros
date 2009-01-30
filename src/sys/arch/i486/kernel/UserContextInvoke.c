/*
 * Copyright (C) 1998, 1999, Jonathan S. Shapiro.
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

#include <kerninc/kernel.h>
#include <kerninc/Activity.h>
#include <kerninc/Process.h>
#include <kerninc/Node.h>
#include <kerninc/Invocation.h>
#include <kerninc/Machine.h>
#include <kerninc/ObjectCache.h>
#include <eros/Invoke.h>
#include <arch-kerninc/Process.h>
#include "Process486.h"

/* May Yield. */
void 
proc_SetupEntryBlock(Process* thisPtr, Invocation* inv /*@ not null @*/)
{
  uint8_t *sndKeys = 0;
  
  /* Not hazarded because invocation key */
  inv->key = &thisPtr->keyReg[thisPtr->pseudoRegs.invKey];

  inv->invType = thisPtr->pseudoRegs.invType;
  inv->entry.code = thisPtr->trapFrame.EAX;
  inv->entry.w1 = thisPtr->trapFrame.EBX;
  inv->entry.w2 = thisPtr->trapFrame.ECX;
  inv->entry.w3 = thisPtr->trapFrame.EDX;

  sndKeys = (uint8_t *) &thisPtr->pseudoRegs.sndKeys;
  
  /* Not hazarded because invocation key */
  inv->entry.key[0] = &thisPtr->keyReg[sndKeys[0]];
  inv->entry.key[1] = &thisPtr->keyReg[sndKeys[1]];
  inv->entry.key[2] = &thisPtr->keyReg[sndKeys[2]];
  inv->entry.key[3] = &thisPtr->keyReg[sndKeys[3]];

  inv->sentLen = 0;		/* set in CopyOut */
  
  /* Set up the entry string, faulting in any necessary data pages and
   * constructing an appropriate kernel mapping: */
  uint32_t len = thisPtr->pseudoRegs.sndLen;
  inv->entry.len = len;

  if (len == 0)
    return;
  if (sndLen > capros_key_msgLimit)
    fatal("Invalid sndLen: should fault the user"); // FIXME

  /* Make sure the string gets mapped if there is one: */

  ula_t ula = thisPtr->pseudoRegs.sndPtr + thisPtr->md.bias;
  inv->entry.data = (uint8_t *) (ula + KUVA);

  ula_t ulaTop = ula + len;

  for (ula &= ~EROS_PAGE_MASK;
       ula < ulaTop;
       ula += EROS_PAGE_SIZE) {
    PTE * pte0 = proc_TranslatePage(thisPtr, ula, PTE_V|PTE_USER, false);
    if (pte0 == 0)
      pte0 = proc_BuildMapping(thisPtr, ula, false, false);
  }
}


/* NOTE that this can be called with /thisPtr/ == 0, and must guard
   against that possibility! */
void 
proc_SetupExitBlock(Process* thisPtr, Invocation* inv /*@ not null @*/)
{
  /* NOTE THAT THIS PROCEEDS EVEN IF THE EXIT BLOCK IS INVALID!!! */
  
  /* inv.exit.code may be overwritten by the actual key handler: */
  inv->exit.code = RC_OK;
  inv->exit.w1 = 0;
  inv->exit.w2 = 0;
  inv->exit.w3 = 0;

  inv->exit.pKey[0] = 0;
  inv->exit.pKey[1] = 0;
  inv->exit.pKey[2] = 0;
  inv->exit.pKey[3] = 0;

  if (thisPtr == 0
      || ! proc_IsExpectingMsg(thisPtr) ) {
    inv->exit.rcvLen = 0;	/* needed because I think invokee==0 is not
			checked before xfer */
    return;
  }

  /* /thisPtr/ is valid. Decode the exit block. */
  uint8_t * rcvKeys = (uint8_t *) &thisPtr->pseudoRegs.rcvKeys;
  if (thisPtr->pseudoRegs.rcvKeys) {
    if (thisPtr->pseudoRegs.rcvKeys & 0xe0e0e0e0) {
      proc_SetFault(thisPtr, capros_Process_FC_MalformedSyscall, 0);
      inv->invokee = 0;
      inv->exit.rcvLen = 0;	/* needed because I think invokee==0 is not
			checked before xfer */
      return;
    }
    else {
      if (rcvKeys[0])
	inv->exit.pKey[0] = &thisPtr->keyReg[rcvKeys[0]];
      if (rcvKeys[1])
	inv->exit.pKey[1] = &thisPtr->keyReg[rcvKeys[1]];
      if (rcvKeys[2])
	inv->exit.pKey[2] = &thisPtr->keyReg[rcvKeys[2]];
      if (rcvKeys[3])
	inv->exit.pKey[3] = &thisPtr->keyReg[rcvKeys[3]];
    }
  }
  
  inv->exit.rcvLen = thisPtr->trapFrame.ESI;

  /* Should this set a fault code? */
  if (inv->exit.rcvLen > EROS_MESSAGE_LIMIT)
    inv->exit.rcvLen = EROS_MESSAGE_LIMIT;

  assert( proc_IsRunnable(thisPtr) );
}

void 
proc_DeliverGateResult(Process* thisPtr, Invocation* inv /*@ not null @*/, bool wantFault)
{
  /* No need to call Prepare() here -- it has already been  called in
   * the invocation path.
   */

  /* There used to be a check of IsRunnable() here.  I removed it
   * because if the recipient had a bad string a fault code was
   * generated in SetupExitString, and as a consequence the recipient
   * is no longer runnable.  It is guaranteed, however, that such a
   * short string will not impede progress.
   */
  
  uint16_t keyData = inv->key->keyData;

  /* copy return code and words */
  thisPtr->trapFrame.EAX = inv->entry.code;
  thisPtr->trapFrame.EBX = inv->entry.w1;
  thisPtr->trapFrame.ECX = inv->entry.w2;
  thisPtr->trapFrame.EDX = inv->entry.w3;

#ifdef OPTION_DDB
  /* Duplicate here so that debugger reporting is accurate. */
  inv->exit.code = inv->entry.code;
  inv->exit.w1 = inv->entry.w1;
  inv->exit.w2 = inv->entry.w2;
  inv->exit.w3 = inv->entry.w3;
#endif

  thisPtr->trapFrame.EDI = keyData;

  /* Data has already been copied out, so don't need to copy here.  DO
   * need to deliver the data length, however:
   */

  thisPtr->trapFrame.ESI = inv->sentLen;
}

/* May Yield. */
void 
proc_SetupExitString(Process* thisPtr,
                     Invocation* inv /*@ not null @*/, uint32_t senderLen)
{
  inv->sentLen = senderLen; // amount that is sent
  
  if (senderLen > inv->exit.rcvLen) {
    senderLen = inv->exit.rcvLen;   // take minimum
  }
  
  if (senderLen == 0)
    return;

  ula_t addr;

  /* In the case where we are going cross-space, we need to do some
   * fairly hairy crud here, but for device drivers it is important to
   * get the invoker == invokee case right.  In that special case, we
   * can simply use the invokee address directly and run the test in a
   * fashion similar to that of the EntryBlock case.
   */

  assert( proc_IsRunnable(thisPtr) );

  if (
      /* It is okay for recipient space to point to the kernel page
	 directory; if it gets bumped up to a large space we will
	 yield due to PTE zap. */

#ifdef OPTION_SMALL_SPACES
      thisPtr->md.smallPTE ||
#endif
      (act_CurContext()->md.MappingTable == thisPtr->md.MappingTable)
      ) {
#ifdef OPTION_SMALL_SPACES
    ula_t ula = thisPtr->trapFrame.EDI + thisPtr->md.bias;
#else
    ula_t ula = trapFrame.EDI;
#endif
    ula_t ulaTop = ula + senderLen;
    ula &= ~EROS_PAGE_MASK;

    while (ula < ulaTop) {
      PTE *pte0 = proc_TranslatePage(thisPtr, ula, PTE_V|PTE_USER, true);
      if (pte0 == 0)
	pte0 = proc_BuildMapping(thisPtr, ula, true, true);

      if (pte0 == 0) {
        /* FIXME: here be bugs */
#if 0
	uint32_t lenHere = ula - thisPtr->trapFrame.EDI;
	if (lenHere < senderLen)
	  inv->validLen = lenHere;
	break;
#endif
      }
      
      ula += EROS_PAGE_SIZE;
    }

#ifdef OPTION_SMALL_SPACES
    addr = thisPtr->trapFrame.EDI + thisPtr->md.bias + KUVA;
#else
    addr = trapFrame.EDI + KUVA;
#endif
    inv->exit.data = (uint8_t *) addr;
  }
  else {
#ifdef KVA_PTEBUF
    PTE *rcvPTE = 0;
#endif

    kva_t kernAddr;
    ula_t cur_ula;
    ula_t ulaTop;
#ifdef OPTION_SMALL_SPACES
    ula_t ula = thisPtr->trapFrame.EDI + thisPtr->md.bias;
#else
    ula_t ula = trapFrame.EDI;
#endif
    ula_t addr = ula;

#ifdef KVA_PTEBUF
    addr &= EROS_PAGE_MASK;
    addr |= KVA_PTEBUF;
  
    /* It is okay to directly reference the previously established 
       pte_kern_ptebuf, because this is actually a kernel mapping
       shared across all processes. */
    rcvPTE = pte_kern_ptebuf;
    kernAddr = KVA_PTEBUF;
#else
    addr &= EROS_L0ADDR_MASK;
    addr |= KVA_FSTBUF;
    kernAddr = addr;
    kernAddr = addr & ~EROS_PAGE_MASK;

#endif

    inv->exit.data = (uint8_t *) addr;

    ulaTop = ula + senderLen;

    ula &= ~EROS_PAGE_MASK;

    for (cur_ula = ula; cur_ula < ulaTop; cur_ula += EROS_PAGE_SIZE) {
      PTE *pte0 = proc_TranslatePage(thisPtr, cur_ula, PTE_V|PTE_USER, true);
      if (pte0 == 0)
	pte0 = proc_BuildMapping(thisPtr, cur_ula, true, false);

      if (pte0 == 0) {
        /* here be bugs */
#if 0
	uint32_t lenHere = ula - thisPtr->trapFrame.EDI;
	if (lenHere < inv->validLen)
	  inv->validLen = lenHere;
	break;
#endif
      }
    
#ifdef KVA_PTEBUF
      *rcvPTE = *pte0;
      rcvPTE++;
      /* FIX: flush logic here is really stupid! */
      mach_FlushTLBWith(kernAddr);
      kernAddr += EROS_PAGE_SIZE;
#endif
    }

#ifndef KVA_PTEBUF
    {
      /* It is NOT okay to directly reference the fast buffers, as these
	 are per-mapping-table, and therefore MUST be set in the
	 currently active mapping table. */
      PTE *entryMappingTable = (PTE *) PTOV(thisPtr->trapFrame.MappingTable);
      PTE *exitMappingTable = (PTE *) PTOV(thisPtr->trapFrame.MappingTable);

      entryMappingTable[KVA_FSTBUF>>22] = 
	exitMappingTable[ula >> 22];

      entryMappingTable[(KVA_FSTBUF>>22) + 1] = 
	exitMappingTable[(ula >> 22) + 1];

      for (cur_ula = ula; cur_ula < ulaTop; cur_ula += EROS_PAGE_SIZE) {
	mach_FlushTLBWith(kernAddr);
	kernAddr += EROS_PAGE_SIZE;
      }
    }
#endif
  }

  /* The segment walking logic may have set a fault code.  Clear it
   * here, since no segment keeper invocation should happen as a
   * result of the above.  We know this was the prior state, as the
   * domain would not otherwise have been runnable.
   */
  proc_ClearFault(thisPtr);
}

/* This may be called from an interrupt. */
void 
proc_DeliverResult(Process* thisPtr, Invocation* inv /*@ not null @*/)
{
  /* No need to call Prepare() here -- it has already been  called in
   * the invocation path.
   */
  assert(proc_IsRunnable(thisPtr));

  /* copy return code and words */
  thisPtr->trapFrame.EAX = inv->exit.code;
  thisPtr->trapFrame.EBX = inv->exit.w1;
  thisPtr->trapFrame.ECX = inv->exit.w2;
  thisPtr->trapFrame.EDX = inv->exit.w3;

  /* Data has already been copied out, so don't need to copy here.  DO
   * need to deliver the data length, however:
   */

  thisPtr->trapFrame.ESI = inv->sentLen;
}

