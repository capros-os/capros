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


#include <kerninc/kernel.h>
#include <kerninc/Activity.h>
#include <kerninc/Process.h>
#include <kerninc/Node.h>
#include <kerninc/Invocation.h>
#include <kerninc/Machine.h>
#include <kerninc/ObjectCache.h>
#include <arch-kerninc/Process.h>

#ifndef OPTION_SMALL_SPACES
const uint32_t bias = 0;
#endif

#ifndef ASM_VALIDATE_STRINGS
void 
proc_SetupEntryString(Process* thisPtr, Invocation* inv /*@ not null @*/)
{
  ula_t ula;
  ula_t ulaTop;
  uint32_t addr;
#ifndef OPTION_PURE_ENTRY_STRINGS
  if (inv->entry.len == 0)
    return;
#endif

  /* Make sure the string gets mapped if there is one: */

  ula = thisPtr->pseudoRegs.sndPtr + thisPtr->bias;

  ulaTop = ula + inv->entry.len;
  ula &= ~EROS_PAGE_MASK;

  while (ula < ulaTop) {
    PTE *pte0 = proc_TranslatePage(thisPtr, ula, PTE_V|PTE_USER, false);
    if (pte0 == 0)
      pte0 = proc_BuildMapping(thisPtr, ula, false, false);

    ula += EROS_PAGE_SIZE;
  }

  addr = (uint32_t) thisPtr->pseudoRegs.sndPtr + thisPtr->bias + KUVA;
  
  inv->entry.data = (uint8_t *) addr;
}
#endif /* ASM_VALIDATE_STRINGS */


void 
proc_SetupExitString(Process* thisPtr, Invocation* inv /*@ not null @*/, uint32_t bound)
{
  ula_t addr;
#ifndef OPTION_PURE_EXIT_STRINGS
  if (inv->validLen == 0)
    return;
#endif

  if (inv->validLen > bound)
    inv->validLen = bound;

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
      thisPtr->smallPTE ||
#endif
      (act_CurContext()->MappingTable == thisPtr->MappingTable)
      ) {
#ifdef OPTION_SMALL_SPACES
    ula_t ula = thisPtr->trapFrame.EDI + thisPtr->bias;
#else
    ula_t ula = trapFrame.EDI;
#endif
    ula_t ulaTop = ula + inv->validLen;
    ula &= ~EROS_PAGE_MASK;

    while (ula < ulaTop) {
      PTE *pte0 = proc_TranslatePage(thisPtr, ula, PTE_V|PTE_USER, true);
      if (pte0 == 0)
	pte0 = proc_BuildMapping(thisPtr, ula, true, true);

      if (pte0 == 0) {
        /* here be bugs */
	uint32_t lenHere = ula - thisPtr->trapFrame.EDI;
	if (lenHere < inv->validLen)
	  inv->validLen = lenHere;
	break;
      }
      
      ula += EROS_PAGE_SIZE;
    }

#ifdef OPTION_SMALL_SPACES
    addr = thisPtr->trapFrame.EDI + thisPtr->bias + KUVA;
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
    ula_t ula = thisPtr->trapFrame.EDI + thisPtr->bias;
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

    ulaTop = ula + inv->validLen;

    ula &= ~EROS_PAGE_MASK;

    for (cur_ula = ula; cur_ula < ulaTop; cur_ula += EROS_PAGE_SIZE) {
      PTE *pte0 = proc_TranslatePage(thisPtr, cur_ula, PTE_V|PTE_USER, true);
      if (pte0 == 0)
	pte0 = proc_BuildMapping(thisPtr, cur_ula, true, false);

      if (pte0 == 0) {
        /* here be bugs */
	uint32_t lenHere = ula - thisPtr->trapFrame.EDI;

	if (lenHere < inv->validLen)
	  inv->validLen = lenHere;

	break;
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
  proc_SetFault(thisPtr, FC_NoFault, 0, false);
}

void 
proc_DeliverResult(Process* thisPtr, Invocation* inv /*@ not null @*/)
{
  /* No need to call Prepare() here -- it has already been  called in
   * the invocation path.
   */
  uint32_t rcvPtr;
  assert(proc_IsRunnable(thisPtr));

  assert (inv->invKeyType > KKT_Resume);
  
  /* copy return code and words */
  thisPtr->trapFrame.EAX = inv->exit.code;
  thisPtr->trapFrame.EBX = inv->exit.w1;
  thisPtr->trapFrame.ECX = inv->exit.w2;
  thisPtr->trapFrame.EDX = inv->exit.w3;

  rcvPtr = thisPtr->trapFrame.EDI;
  
  thisPtr->trapFrame.EDI = 0;		/* key info field */

  /* Data has already been copied out, so don't need to copy here.  DO
   * need to deliver the data length, however:
   */

  thisPtr->trapFrame.ESI = inv->sentLen;

  /* If the recipient specified an invalid receive area, though, they
   * are gonna get FC_ParmLack:
   */
  if (inv->validLen < inv->exit.len)
    proc_SetFault(thisPtr, FC_ParmLack, rcvPtr + inv->validLen, false);
}

