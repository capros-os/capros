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

#if defined(EROS_TARGET_i486)
#include <idl/capros/arch/i386/Process.h>
#endif

uint32_t
create_new_process(uint32_t krBank, uint32_t krDomKey)
{
  uint32_t isGood;
  int i;

  DEBUG kdprintf(KR_OSTREAM, "Check official bank...\n");

  if (capros_SpaceBank_verify(KR_BANK, krBank, &isGood) != RC_OK ||
      isGood == 0)
    return RC_ProcCre_BadBank;
  
  DEBUG kdprintf(KR_OSTREAM, "OK -- buy process nodes\n");

#if defined(EROS_TARGET_i486)

  /* Bank is okay, try to buy the space: */
  if (capros_SpaceBank_alloc2(krBank,
                              capros_Range_otNode | (capros_Range_otNode << 8),
                              krDomKey, KR_SCRATCH0
			     ) != RC_OK )
    return RC_capros_key_NoMoreNodes;

  DEBUG kdprintf(KR_OSTREAM, "Assemble them\n");

  /* We have the nodes.  Make the second the key registers node of the
     first: */
  (void) capros_Node_swapSlot(krDomKey, ProcGenKeys, KR_SCRATCH0, KR_VOID);

  /* Initialize the fixed registers to zero number keys. */
  const capros_Number_value zeroNumber = {{0, 0, 0}};
  for (i = ProcFirstRootRegSlot; i <= ProcLastRootRegSlot; i++)
    (void) capros_Node_writeNumber(krDomKey, i, zeroNumber);

  /* Now install the brand: */
  (void) capros_Node_swapSlot(krDomKey, ProcBrand, KR_OURBRAND, KR_VOID);

  DEBUG kdprintf(KR_OSTREAM, "Build process key:\n");

  /* Now make a process key of this: */
  (void) capros_ProcTool_makeProcess(KR_PROCTOOL, krDomKey, krDomKey);

  DEBUG kdprintf(KR_OSTREAM, "Got new process key:\n");

  /* Write valid values into the registers: */
  struct capros_arch_i386_Process_Registers regs = {
    .len = sizeof(struct capros_arch_i386_Process_Registers),
    .arch = capros_Process_ARCH_I386,
    .procFlags = 0,
    .faultCode = 0,
    .faultInfo = 0,
    .pc = 0,
    .sp = 0,
    .EDI = 0,
    .ESI = 0,
    .EBP = 0,
    .EBX = 0, 
    .EDX = 0,
    .ECX = 0,
    .EAX = 0,
    .EFLAGS = 0x200,	// interrupt enable
    .CS = capros_arch_i386_Process_CodeSeg,
    .SS = capros_arch_i386_Process_DataSeg,
    .DS = capros_arch_i386_Process_DataSeg,
    .ES = capros_arch_i386_Process_DataSeg,
    .FS = capros_arch_i386_Process_DataSeg,
    .GS = capros_arch_i386_Process_PseudoSeg
  };

  (void) capros_arch_i386_Process_setRegisters(krDomKey, regs);

#elif defined(EROS_TARGET_arm)

  /* Bank is okay, try to buy the space: */
  if (capros_SpaceBank_alloc2(krBank,
                              capros_Range_otNode | (capros_Range_otNode << 8),
                              krDomKey, KR_SCRATCH0
			     ) != RC_OK )
    return RC_capros_key_NoMoreNodes;

  DEBUG kdprintf(KR_OSTREAM, "Assemble them\n");

  /* We have the nodes.  Make the second the key registers node of the
     first: */
  (void) capros_Node_swapSlot(krDomKey, ProcGenKeys, KR_SCRATCH0, KR_VOID);

  /* Initialize the fixed registers to zero number keys. */
  const capros_Number_value zeroNumber = {{0, 0, 0}};
  for (i = ProcFirstRootRegSlot; i <= ProcLastRootRegSlot; i++)
    (void) capros_Node_writeNumber(krDomKey, i, zeroNumber);

  /* Now install the brand: */
  (void) capros_Node_swapSlot(krDomKey, ProcBrand, KR_OURBRAND, KR_VOID);

  DEBUG kdprintf(KR_OSTREAM, "Build process key:\n");

  /* Now make a process key of this: */
  (void) capros_ProcTool_makeProcess(KR_PROCTOOL, krDomKey, krDomKey);

  DEBUG kdprintf(KR_OSTREAM, "Got new process key:\n");

  // Zero register values are OK.

#endif

  return RC_OK;
}
