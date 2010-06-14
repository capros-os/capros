/*
 * Copyright (C) 2006-2010, Strawberry Development Group
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
#include <kerninc/IRQ.h>
#include <arch-kerninc/Process-inline.h>
#include <arch-kerninc/PTE.h>
#include <idl/capros/key.h>

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

/* This may be called from an interrupt. */
void 
proc_DeliverResult(Process * thisPtr, Invocation * inv /*@ not null @*/)
{
  assert(proc_IsRunnable(thisPtr));

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
    fatal("Invalid invKey: should fault the user\n"); // FIXME
  }
  inv->key = &thisPtr->keyReg[invSlot];

  unsigned int typ = (invKeyAndType >> 8) & 0xff;
  if (!INVTYPE_ISVALID(typ)) {
    fatal("Invalid invType: should fault the user\n"); // FIXME
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
    fatal("Invalid sndKeys: should fault the user\n"); // FIXME
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
  if (sndLen > capros_key_messageLimit)
    fatal("Invalid sndLen: should fault the user\n"); // FIXME

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
  if (inv->exit.rcvLen > capros_key_messageLimit)
    inv->exit.rcvLen = capros_key_messageLimit;

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

revalidate: ;
  uva_t va = thisPtr->trapFrame.r0; // VA of destination's Message structure
  // FIXME: who checks that this is word-aligned?
  va += offsetof(Message, rcv_data);	// VA of Message.rcv_data
  // Must calculate MVA because this proc's PID may not be loaded.
  if (act_CurContext()->md.firstLevelMappingTable
      == thisPtr->md.firstLevelMappingTable ) {
    // Processes are using the same map.
    // The PID of act_CurContext() is loaded.
    mach_LoadDACR(thisPtr->md.dacr);
    // Ensure the destination is mapped.
    // Make sure the user-mode map is correct:
    if (MapsWereInvalidated()) {
      KernStats.nYieldForMaps++;
      act_Yield();
    }

    if (! LoadWordFromUserSpace(proc_VAToMVA(thisPtr, va), (uint32_t *)&va)) {
      // Not mapped, try to map it.
      // FIXME: Does proc_DoPageFault check access (wrong domain)?
      if (! proc_DoPageFault(thisPtr, va,
            false /* read only */, true /* prompt */ )) {
        fatal("proc_SetupExitString needs to fault, unimplemented!\n");
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
        fatal("proc_SetupExitString needs to fault, unimplemented!\n");
    }

    uva_t pgPtr;
    // Validate every destination page.
    for (pgPtr = va;
         pgPtr < vaTop;
         pgPtr = (pgPtr + EROS_PAGE_SIZE) & ~EROS_PAGE_MASK ) {
      /* The area to receive the string is subject to being written,
      so we are entitled to write to it here even though the
      invocation is not committed yet. */
      /* Convert to MVA explicitly, because the destination process's
      pid is not loaded. */
      if (! StoreByteToUserSpace(proc_VAToMVA(thisPtr, pgPtr), 0)) {
        // Not mapped, try to map it.
        if (! proc_DoPageFault(thisPtr, pgPtr,
              true /* write */, true /* prompt */ )) {
          fatal("proc_SetupExitString needs to fault, unimplemented!\n");
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
    if (thisPtr->md.firstLevelMappingTable == FLPT_NullPA) {
      // thisPtr currently has no address map at all.
      // FIXME: Does proc_DoPageFault check access (wrong domain)?
      if (! proc_DoPageFault(thisPtr, va,
            false /* read only */, true /* prompt */ )) {
        fatal("proc_SetupExitString needs to fault, unimplemented!\n");
      }
      goto revalidate;	// it should have a non-null FLPT now
    }

    if (act_CurContext()->md.firstLevelMappingTable == FLPT_NullPA) {
      // The current process currently has no address map at all.
      // This can happen if the map was stolen, say in EnsureSSDomain.
      // Start over and hope for better luck next time.
      act_Yield();
    }

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

  proc_LogState(invokerProc, Trap_FromInv);

  assert(local_irq_disabled());	// disabled right after exception

  /* Enable IRQ interrupts. */
  irq_ENABLE();

  assert(invokerProc->faultCode == capros_Process_FC_NoFault);

  BeginInvocation();
  
  objH_BeginTransaction();

  /* Roll back the invocation PC in case we need to restart this operation */
  proc_AdjustInvocationPC(invokerProc);

  proc_SetupEntryBlock(invokerProc, &inv);

  proc_DoKeyInvocation(invokerProc);

  irq_DISABLE();

  ExitTheKernel();
  return;
}
