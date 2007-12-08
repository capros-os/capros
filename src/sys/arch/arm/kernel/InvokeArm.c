/*
 * Copyright (C) 2006, 2007, Strawberry Development Group
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
Research Projects Agency under Contract Nos. W31P4Q-06-C-0040 and
W31P4Q-07-C-0070.  Approved for public release, distribution unlimited. */

#include <kerninc/kernel.h>
#include <kerninc/Invocation.h>
#include <kerninc/Process.h>
#include <kerninc/Activity.h>
#include <kerninc/KernStats.h>
#include <arch-kerninc/Process-inline.h>
#include <arch-kerninc/IRQ-inline.h>

#define dbg_init	0x1u

/* Following should be an OR of some of the above */
#define dbg_flags   ( 0u | dbg_init)

#define DEBUG(x) if (dbg_##x & dbg_flags)

INLINE ula_t
proc_VAToMVA(Process * thisPtr, uva_t va)
{
  if ((va & PID_MASK) == 0)
    return va + thisPtr->md.pid;
  else return va;
}

void 
proc_DeliverResult(Process * thisPtr, Invocation * inv /*@ not null @*/)
{
  assert(proc_IsRunnable(thisPtr));

  assert (inv->invKeyType > KKT_Resume);
  
  /* copy return code and words */
  thisPtr->trapFrame.r1 = inv->exit.code;
  thisPtr->trapFrame.r2 = inv->exit.w1;
  thisPtr->trapFrame.r3 = inv->exit.w2;
  thisPtr->trapFrame.r4 = inv->exit.w3;

  /* Data has already been copied out, so don't need to copy here.  DO
   * need to deliver the data length, however:
   */

  thisPtr->trapFrame.r12 = inv->sentLen;
}

/* May Yield. */
void 
proc_SetupEntryBlock(Process* thisPtr, Invocation* inv /*@ not null @*/)
{
  /* Note, sender's DACR and PID are loaded, so we can reference the
  user's memmory without adding the PID or checking access. */

  uva_t const entryMessage = thisPtr->trapFrame.r0;
  uint32_t const invKeyAndType = thisPtr->trapFrame.r1;
  
  /* Not hazarded because invocation key */
  unsigned int invSlot = invKeyAndType & 0xff;
  if (invSlot >= EROS_NODE_SIZE) {
    fatal("Invalid invKey: should fault the user"); // FIXME
  }
  inv->key = &thisPtr->keyReg[invSlot];

  unsigned int typ = (invKeyAndType >> 8) & 0xff;
  if (!INVTYPE_ISVALID(typ)) {
    fatal("Invalid invType: should fault the user"); // FIXME
  }
  inv->invType = typ;

  inv->entry.code = thisPtr->trapFrame.r4;
/* We may be able to eliminate the copy of w1, w2, and w3 when invoking
   kernel keys that don't use those parameters. 
   For now, just copy them. */
  inv->entry.w1 = thisPtr->trapFrame.r5;
  /* Current process's address map is still loaded. */
  LoadWordFromUserVirtualSpace(entryMessage + offsetof(Message, snd_w2),
                               &inv->entry.w2);
  LoadWordFromUserVirtualSpace(entryMessage + offsetof(Message, snd_w3),
                               &inv->entry.w3);

  uint8_t * sndKeys = (uint8_t *) &thisPtr->trapFrame.r2;
  if (thisPtr->trapFrame.r2 & 0xe0e0e0e0) {
    fatal("Invalid sndKeys: should fault the user"); // FIXME
  }
  
  /* Not hazarded because invocation key */
  inv->entry.key[0] = &thisPtr->keyReg[sndKeys[0]];
  inv->entry.key[1] = &thisPtr->keyReg[sndKeys[1]];
  inv->entry.key[2] = &thisPtr->keyReg[sndKeys[2]];
  inv->entry.key[3] = &thisPtr->keyReg[sndKeys[3]];

  inv->sentLen = 0;		/* set in CopyOut */

  /* Set up the entry string, faulting in any necessary data pages and
   * constructing an appropriate kernel mapping: */
  uint32_t sndLen = inv->entry.len = thisPtr->trapFrame.r3;
  if (sndLen == 0)
    return;

  /* Get user's snd_addr from his Message structure. */
  ula_t addr;
  LoadWordFromUserVirtualSpace(entryMessage + offsetof(Message, snd_data),
                               &addr);

  /* Since this is the UNmodified virtual address, the sender's PID
  must remain loaded as long as we might need the string. */
  inv->entry.data = (uint8_t *) addr;

  /* Ensure each page of the string is mapped. */
  ula_t ulaTop = addr + sndLen;	/* addr of last byte +1 */
  for (addr &= ~EROS_PAGE_MASK;
       addr < ulaTop;
       addr += EROS_PAGE_SIZE) {
    /* Fastest way to see if it is mapped is to try to fetch it.
       We don't use the value fetched. */
    uint32_t unused;
    LoadWordFromUserVirtualSpace(addr, &unused);
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
    inv->exit.rcvLen = 0;	/* needed because I think invokee == 0 is not
			checked before xfer */
    return;
  }

  /* /thisPtr/ is valid. Decode the exit block. */
  /* Note, we can't easily get the exit block from the Message structure,
  because that process's address map isn't loaded now.
  Fortunately we have stashed the values in registers. */

  uint8_t * rcvKeys = (uint8_t *) &thisPtr->trapFrame.r14;
  if (thisPtr->trapFrame.r14) {		/* if any nonzero */
    if (thisPtr->trapFrame.r14 & 0xe0e0e0e0) {	/* if any too large */
      proc_SetFault(thisPtr, capros_Process_FC_MalformedSyscall, 0);
      inv->invokee = 0;
      inv->exit.rcvLen = 0;	/* needed because I think invokee == 0 is not
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
  
  inv->exit.rcvLen = thisPtr->trapFrame.r12;	/* rcv_limit */

  /* Should this set a fault code? */
  if (inv->exit.rcvLen > EROS_MESSAGE_LIMIT)
    inv->exit.rcvLen = EROS_MESSAGE_LIMIT;

  assert( proc_IsRunnable(thisPtr) );
}

// May Yield.
void 
proc_SetupExitString(Process * thisPtr, Invocation * inv /*@ not null @*/,
                     uint32_t senderLen)
{
  inv->sentLen = senderLen;	// amount that is sent

  if (senderLen > inv->exit.rcvLen) {
    senderLen = inv->exit.rcvLen;	// take minimum
  }

  if (senderLen == 0)
    return;

  assert( proc_IsRunnable(thisPtr) );

revalidate:
  if (act_CurContext()->md.firstLevelMappingTable
      == thisPtr->md.firstLevelMappingTable ) {
    // Processes are using the same map.
    // The PID of act_CurContext() is loaded.
    mach_LoadDACR(thisPtr->md.dacr);
    // Ensure the destination is mapped.
    uva_t va = thisPtr->trapFrame.r0; // VA of Message structure
    // FIXME: who checks that this is word-aligned?
    va += offsetof(Message, rcv_data);	// VA of Message.rcv_data
    // Must calculate MVA because this proc's PID may not be loaded.
    va = proc_VAToMVA(thisPtr, va);
    if (! LoadWordFromUserSpace(va, (uint32_t *)&va)) {
      // Not mapped, try to map it.
printf("faulting on exit rcv_data addr, va=0x%x\n", va);////
      // FIXME: Does proc_DoPageFault check access (wrong domain)?
      if (! proc_DoPageFault(thisPtr, va,
            false /* read only */, true /* prompt */ )) {
        fatal("proc_SetupExitString needs to fault, unimplemented!");
      } else {
        // We repaired the fault. BUT, in so doing, we may have
        // invalidated some other map needed for this operation.
        // Therefore start validating all over again.
        // NOTE this may even need to go back to the caller.
        // FIXME: need a retry count
        goto revalidate;
      }
    }
    // Got rcv_data in va.
    uva_t vaTop = va + senderLen;

    if (thisPtr->md.pid) {
      /* Since this is a small space, addresses should be <= (1 << PID_SHIFT).
      Check for that here, to prevent the following scenario:
      The process has pages at 0 and 0x01fff000.
      It receives a string into an area with start address 0x01fff000 and 
      length 0x2000. 
      The unmodified virtual addresses of the pages in the string area are
      0x01fff000 and 0x02000000.
      If the process happens to have a pid of 0x02000000, the modified
      virtual addresses will be 0x03fff000 and 0x02000000.
      Those will both be valid addresses for it to reference,
      and for us to reference in LoadWordFromUserSpace below.
      But if we set inv->exit.data to 0x03fff000, we will reference the
      second page at 0x04000000 not 0x02000000.
      */

      if (va >= (1ul << PID_SHIFT)
          || vaTop >= (1ul << PID_SHIFT))
        fatal("proc_SetupExitString needs to fault, unimplemented!");
    }

    uva_t pgPtr = va & ~EROS_PAGE_MASK;
    // Validate every destination page.
    for (; pgPtr < vaTop; pgPtr += EROS_PAGE_SIZE) {
      uint32_t word;
      /* Convert to MVA explicitly, because the destination process's
      pid is not loaded. */
      if (! LoadWordFromUserSpace(proc_VAToMVA(thisPtr, pgPtr), &word)) {
        // Not mapped, try to map it.
        if (! proc_DoPageFault(thisPtr, pgPtr,
              true /* write */, true /* prompt */ )) {
          fatal("proc_SetupExitString needs to fault, unimplemented!");
        } else {
          goto revalidate;
        }
      }
    }
    // Restore DACR of current process.
    // ? mach_LoadDACR(act_CurContext()->md.dacr);
    mach_LoadDACR(0x55555555);	// need access to both from and to domains
    // FIXME: Figure out when the DACR has what. 
    // Current process's map is current, so destination addr is too.
    inv->exit.data = (uint8_t *)proc_VAToMVA(thisPtr, va);
  } else {
    // Processes are using different maps.
    fatal("proc_SetupExitString cross-space unimplemented!\n");
  }

  /* The segment walking logic may have set a fault code.  Clear it
   * here, since no segment keeper invocation should happen as a
   * result of the above.  We know this was the prior state, as the
   * domain would not otherwise have been runnable.  */
  // FIXME: Prevent setting the fault.
  proc_ClearFault(thisPtr);
}

void 
proc_DeliverGateResult(Process* thisPtr,
                       Invocation* inv /*@ not null @*/, bool wantFault)
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
  thisPtr->trapFrame.r1 = inv->entry.code;
  thisPtr->trapFrame.r2 = inv->entry.w1;
  thisPtr->trapFrame.r3 = inv->entry.w2;
  thisPtr->trapFrame.r4 = inv->entry.w3;

#ifdef OPTION_DDB
  /* Duplicate here so that debugger reporting is accurate. */
  inv->exit.code = inv->entry.code;
  inv->exit.w1 = inv->entry.w1;
  inv->exit.w2 = inv->entry.w2;
  inv->exit.w3 = inv->entry.w3;
#endif
  
  thisPtr->trapFrame.r14 = keyData;

  /* Data has already been copied out, so don't need to copy here.  DO
   * need to deliver the data length, however:
   */

  thisPtr->trapFrame.r12 = inv->sentLen;
}

void /* does not return */
InvokeArm(Process * invokerProc,
          uint32_t typeAndKey,	/* invoked key in low byte, type in next */
          uint32_t snd_keys,
          uint32_t snd_len)
{
  assert(invokerProc == proc_Current());
#if 0
  printf("Inv p=%x, type.key %x, oc 0x%x, psr=%x, pc=0x%08x, r0=%x, sp=0x%08x\n",
         invokerProc, typeAndKey, invokerProc->trapFrame.r4,
         invokerProc->trapFrame.CPSR, invokerProc->trapFrame.r15,
         invokerProc->trapFrame.r0, invokerProc->trapFrame.r13);
#endif

#if 1
  goto general_path1;	/* bypass fast path */
#else

  Process * invokee;

  /* Instead of validating parameters, just ignore any cruft. It's faster. */
#define InvTypeMask 0x7	/* must be >= IT_NUM_INVTYPES */
  typeAndKey &= (InvTypeMask << 8) | (EROS_NODE_SIZE -1);
  snd_keys &= (EROS_NODE_SIZE -1)
              | ((EROS_NODE_SIZE -1) << 8)
              | ((EROS_NODE_SIZE -1) << 16)
              | ((EROS_NODE_SIZE -1) << 24);

  /* See if we can use a fast path. */
  if (snd_len) goto general_path0;	/* no strings */

  unsigned int invokerFinalState;
  { unsigned int type = typeAndKey >> 8;
    if (type < IT_Call) invokerFinalState = RS_Available; /* return */
    else if (type == IT_Call) invokerFinalState = RS_Waiting; /* call */
    else goto general_path1; /* only call, return */
  }
  //// no one blocked on stall queue? interrupt.S line 1051
  Key * invKey = &invokerProc->keyReg[typeAndKey & (EROS_NODE_SIZE -1)];
  /* Assuming it is a Start or Resume key, it must be prepared. */
  if (!(invKey->keyFlags & KFL_PREPARED)) goto general_path1;
  switch (invKey->keyType) {
  case KKT_Start:
    invokee = invKey->u.gk.pContext;
    if (invokee->runState != RS_Available) goto general_path1;
    break;
  case KKT_Resume:
    invokee = invKey->u.gk.pContext;
    /* Since a resume key exists, invokee must be in the waiting state. */
    assert(invokee->runState == RS_Waiting);
    break;
  default: goto general_path1;
  }

  if (invokee->processFlags & capros_Process_PF_FaultToProcessKeeper) goto general_path1;
  /* Do we need to check invokee has an address space? */

  if (invokee->trapFrame.r14) {	/* invokee is receiving some keys */
    unsigned int rcvKeyNdx = invokee->trapFrame.r14 & (EROS_NODE_SIZE -1);
    unsigned int sndKeyNdx;
    if (rcvKeyNdx != 0) {	/* send key 0 */
      Key * rcvKey0 = &invokee->keyReg[rcvKeyNdx];
#if RESUME_SLOT == 0
      if (invokerFinalState != RS_Available) { /* a call */
        proc_BuildResumeKey(invokerProc, rcvKey0);
      } else /* a return */
#endif
      {
        sndKeyNdx = invokerProc->trapFrame.r2 & (EROS_NODE_SIZE -1);
        /* key_NH_Set handles some cases that don't occur here:
           src == dst
           src has hazard bits */
        key_NH_Set(rcvKey0, &invokerProc->keyReg[sndKeyNdx]);
      }
    }
    rcvKeyNdx = (invokee->trapFrame.r14 >> 8) & (EROS_NODE_SIZE -1);
    if (rcvKeyNdx != 0) {	/* send key 1 */
      sndKeyNdx = (invokerProc->trapFrame.r2 >> 8) & (EROS_NODE_SIZE -1);
      key_NH_Set(&invokee->keyReg[rcvKeyNdx],
                 &invokerProc->keyReg[sndKeyNdx]);
    }
    rcvKeyNdx = (invokee->trapFrame.r14 >> 16) & (EROS_NODE_SIZE -1);
    if (rcvKeyNdx != 0) {	/* send key 2 */
      sndKeyNdx = (invokerProc->trapFrame.r2 >> 16) & (EROS_NODE_SIZE -1);
      key_NH_Set(&invokee->keyReg[rcvKeyNdx],
                 &invokerProc->keyReg[sndKeyNdx]);
    }
    rcvKeyNdx = (invokee->trapFrame.r14 >> 24) & (EROS_NODE_SIZE -1);
    if (rcvKeyNdx != 0) {	/* send key 3 */
      Key * rcvKey0 = &invokee->keyReg[rcvKeyNdx];
#if RESUME_SLOT == 3
      if (invokerFinalState != RS_Available) { /* a call */
        proc_BuildResumeKey(invokerProc, rcvKey0);
      } else /* a return */
#endif
      {
        sndKeyNdx = (invokerProc->trapFrame.r2 >> 24) & (EROS_NODE_SIZE -1);
        key_NH_Set(rcvKey0, &invokerProc->keyReg[sndKeyNdx]);
      }
    }
  }

  invokee->trapFrame.r14 = invKey->keyData;

  /* Move data registers. */
  invokee->trapFrame.r1 = invokerProc->trapFrame.r4;	/* snd_code */
  /* Current process's address map is still loaded. */
  uva_t const entryMessage = invokerProc->trapFrame.r0;
  if (! LoadWordFromUserSpace(entryMessage + offsetof(Message, snd_w1),
                              &invokee->trapFrame.r2))
    fatal("Error loading w1-w3");	/* do we need to handle this? */
  if (! LoadWordFromUserSpace(entryMessage + offsetof(Message, snd_w2),
                              &invokee->trapFrame.r3))
    fatal("Error loading w1-w3");
  if (! LoadWordFromUserSpace(entryMessage + offsetof(Message, snd_w3),
                              &invokee->trapFrame.r4))
    fatal("Error loading w1-w3");

  /* Set returned length to zero. */
  invokee->trapFrame.r12 = 0;
  /* Migrate the Activity. */
  act_MigrateFromCurrent(invokerProc->curActivity, invokee);

  /* Leave invokee's PC advanced. */
  proc_AdjustInvocationPC(invokerProc);	/* back up invoker PC */

  invokerProc->runState = invokerFinalState;
  /* keyR_ZapResumeKeys is only needed if the invoked key was KKT_Resume.
     If it was KKT_Start, there should already be no resume keys. */
  keyR_ZapResumeKeys(&invokee->keyRing);

  /* FIX:  Gotta wake everybody on my own stall queue here! */

  invokee->runState = RS_Running;

  ExitTheKernel();
  assert(false);

general_path0:
  if (snd_len > EROS_MESSAGE_LIMIT) {
    fatal("snd_len too big.\n");	// FIXME
  }
#endif
general_path1:
  
  assert(irq_DISABLE_DEPTH() == 1);	// disabled right after exception

  /* Enable IRQ interrupts. */
  irq_ENABLE();

  assert(act_CurContext()->faultCode == capros_Process_FC_NoFault);

  proc_DoKeyInvocation(act_CurContext());

  assert( irq_DISABLE_DEPTH() == 0 );
  irq_DISABLE();

  ExitTheKernel();
  return;
}
