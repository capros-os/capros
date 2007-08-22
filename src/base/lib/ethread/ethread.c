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
#include <eros/Invoke.h>

#include <idl/capros/key.h>
#include <idl/capros/Process.h>

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
  Message msg;
  
  result = proccre_create_process(KR_CREATOR, kr_bank, kr_new_thread);
  if (result != RC_OK)
    return RC_Ethread_Create_Process_Err;

  /* Copy the address space to the new thread */
  result = capros_Process_getAddrSpace(KR_SELF, kr_tmp);
  if (result != RC_OK)
    return RC_Ethread_Copy_AddrSpace_Err;

  result = capros_Process_swapAddrSpace(kr_new_thread, kr_tmp, KR_VOID);
  if (result != RC_OK)
    return RC_Ethread_Copy_AddrSpace_Err;
  
  /* Copy schedule key */
  result = capros_Process_getSchedule(KR_SELF, kr_tmp);
  if (result != RC_OK)
    return RC_Ethread_Copy_SchedKey_Err;

  result = capros_Process_swapSchedule(kr_new_thread, kr_tmp, KR_VOID);
  if (result != RC_OK)
    return RC_Ethread_Copy_SchedKey_Err;

  /* Now just copy all key registers */
  for (kr = 0; kr < EROS_NODE_SIZE; kr++) {
    result = capros_Process_getKeyReg(KR_SELF, kr, kr_tmp);
    if (result != RC_OK)
      return RC_Ethread_Copy_Keyregs_Err;

    result = capros_Process_swapKeyReg(kr_new_thread, kr, kr_tmp, KR_VOID);
    if (result != RC_OK)
      return RC_Ethread_Copy_Keyregs_Err;
  }

  // Set the PC and SP.
  {
    /* Request a portion of the heap for this thread's stack */
    void * stack = malloc(stack_size);

    if (stack == NULL)
      return RC_Ethread_Malloc_Err;

    struct capros_Process_CommonRegisters32 regs = {
      .procFlags = 0,
      .faultCode = 0,
      .faultInfo = 0,
      .pc = program_counter,
      .sp = (uint32_t)stack + stack_size
    };
  
    result = capros_Process_setRegisters32(kr_new_thread, regs);
  }
  if (result != RC_OK) 
    return RC_Ethread_SetRegs_Err;
  
  result = capros_Process_makeResumeKey(kr_new_thread, kr_tmp);
  if (result != RC_OK) 
    return RC_Ethread_Make_Fault_Key_Err;
  
  /* Fabricate a start key */
  result = capros_Process_makeStartKey(kr_new_thread, 0, kr_new_thread);
  if (result != RC_OK) 
    return RC_Ethread_Make_Start_Key_Err;
  
  /* Invoke the fault key to start the thread */
  memset(&msg, 0, sizeof(Message));
  msg.snd_invKey = kr_tmp;

  return SEND(&msg);
}
