/*
 * Copyright (C) 2003, Jonathan S. Shapiro.
 * Copyright (C) 2007, Strawberry Development Group.
 *
 * This file is part of the CapROS Operating System runtime library.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, 59 Temple Place - Suite 330 Boston, MA 02111-1307, USA.
 */
/* This material is based upon work supported by the US Defense Advanced
Research Projects Agency under Contract No. W31P4Q-07-C-0070.
Approved for public release, distribution unlimited. */

#include <stdlib.h>
#include <string.h>

#include <eros/target.h>
#include <eros/machine/Registers.h>
#include <eros/ProcessKey.h>
#include <eros/ProcessState.h>
#include <eros/Invoke.h>

#include <idl/capros/key.h>

#include <domain/Runtime.h>
#include <domain/ProcessCreatorKey.h>

#include "ethread.h"

/* Convenience routine for creating an EROS thread */
uint32_t 
ethread_new_thread(cap_t kr_bank, cap_t kr_tmp, uint32_t stack_size,
		   uint32_t program_counter,
		   /* result */ cap_t kr_new_thread)
{
  uint32_t kr;
  result_t result;
  struct Registers regs;
  Message msg;
  
  result = proccre_create_process(KR_CREATOR, kr_bank, kr_new_thread);
  if (result != RC_OK)
    return RC_Ethread_Create_Process_Err;

  /* Copy the address space to the new thread */
  result = process_copy(KR_SELF, ProcAddrSpace, kr_tmp);
  if (result != RC_OK)
    return RC_Ethread_Copy_AddrSpace_Err;

  result = process_swap(kr_new_thread, ProcAddrSpace, kr_tmp, KR_VOID);
  if (result != RC_OK)
    return RC_Ethread_Copy_AddrSpace_Err;
  
  /* Copy schedule key */
  result = process_copy(KR_SELF, ProcSched, kr_tmp);
  if (result != RC_OK)
    return RC_Ethread_Copy_SchedKey_Err;

  result = process_swap(kr_new_thread, ProcSched, kr_tmp, KR_VOID);
  if (result != RC_OK)
    return RC_Ethread_Copy_SchedKey_Err;

  /* Now just copy all key registers */
  for (kr = 0; kr < EROS_NODE_SIZE; kr++) {
    result = process_copy_keyreg(KR_SELF, kr, kr_tmp);
    if (result != RC_OK)
      return RC_Ethread_Copy_Keyregs_Err;

    result = process_swap_keyreg(kr_new_thread, kr, kr_tmp, KR_VOID);
    if (result != RC_OK)
      return RC_Ethread_Copy_Keyregs_Err;
  }

  /* Now set up registers appropriately */
  process_get_regs(KR_SELF, &regs);
  
  regs.CS = DOMAIN_CODE_SEG;
  regs.SS = DOMAIN_DATA_SEG;
  regs.DS = DOMAIN_DATA_SEG;
  regs.ES = DOMAIN_DATA_SEG;
  regs.FS = DOMAIN_DATA_SEG;
  regs.GS = DOMAIN_PSEUDO_SEG;
  
  regs.pc = program_counter;
  
  /* Request a portion of the heap for this thread's stack */
  {
    void *stack = malloc(stack_size);

    if (stack == NULL)
      return RC_Ethread_Malloc_Err;

    regs.sp = (uint32_t)stack + stack_size;
  }

  regs.faultCode = 0;
  regs.faultInfo = 0;
  regs.domFlags = 0;
  regs.EFLAGS = 0x200;

  result = process_set_regs(kr_new_thread, &regs);
  if (result != RC_OK) 
    return RC_Ethread_SetRegs_Err;
  
  result = process_make_fault_key(kr_new_thread, kr_tmp);
  if (result != RC_OK) 
    return RC_Ethread_Make_Fault_Key_Err;
  
  /* Fabricate a start key */
  result = process_make_start_key(kr_new_thread, 0, kr_new_thread);
  if (result != RC_OK) 
    return RC_Ethread_Make_Start_Key_Err;
  
  /* Invoke the fault key to start the thread */
  memset(&msg, 0, sizeof(Message));
  msg.snd_invKey = kr_tmp;

  return SEND(&msg);
}
