/*
 * Copyright (C) 2003, Jonathan S. Shapiro.
 * Copyright (C) 2007-2010, Strawberry Development Group.
 *
 * This file is part of the CapROS Operating System runtime library,
 * and is derived from the EROS Operating System runtime library.
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
#include <idl/capros/ProcCre.h>

#include <domain/Runtime.h>

#include "ethread.h"

#define assert(c) if (!c) return RC_Ethread_Unexpected_Err;

/* Convenience routine for creating an EROS thread */
uint32_t 
ethread_new_thread1(cap_t kr_bank, void * stack_pointer,
		    void * program_counter,
		    /* result */ cap_t kr_new_thread)
{
  uint32_t kr;
  result_t result;

  if (kr_new_thread == KR_TEMP0)
    return RC_Ethread_Unexpected_Err;
  
  result = capros_ProcCre_createProcess(KR_CREATOR, kr_bank, kr_new_thread);
  if (result != RC_OK)
    return result;

  /* Copy the address space to the new thread */
  result = capros_Process_getAddrSpace(KR_SELF, KR_TEMP0);
  assert(result == RC_OK);

  result = capros_Process_swapAddrSpace(kr_new_thread, KR_TEMP0, KR_VOID);
  assert(result == RC_OK);
  
  /* Copy schedule key */
  result = capros_Process_getSchedule(KR_SELF, KR_TEMP0);
  assert(result == RC_OK);
  result = capros_Process_swapSchedule(kr_new_thread, KR_TEMP0, KR_VOID);
  assert(result == RC_OK);
  
  /* Copy symspace key */
  result = capros_Process_getSymSpace(KR_SELF, KR_TEMP0);
  assert(result == RC_OK);
  result = capros_Process_swapSymSpace(kr_new_thread, KR_TEMP0, KR_VOID);
  assert(result == RC_OK);

  /* Now just copy all key registers */
  for (kr = 0; kr < EROS_NODE_SIZE; kr++) {
    cap_t krToCopy = kr == KR_SELF ? kr_new_thread : kr;
    result = capros_Process_swapKeyReg(kr_new_thread, kr, krToCopy, KR_VOID);
    assert(result == RC_OK);
  }

  // Set the PC and SP.
  struct capros_Process_CommonRegisters32 regs = {
    .procFlags = 0,
    .faultCode = 0,
    .faultInfo = 0,
    .pc = (uint32_t)program_counter,
    .sp = (uint32_t)stack_pointer
  };
  
  result = capros_Process_setRegisters32(kr_new_thread, regs);
  assert(result == RC_OK);
  
  return RC_OK;
}

void
ethread_start(cap_t thread)
{
  capros_Process_makeResumeKey(thread, KR_TEMP0);
  
  /* Invoke the fault key to start the thread */
  Message msg = {
    .snd_invKey = KR_TEMP0,
    .snd_key0 = KR_VOID,
    .snd_key1 = KR_VOID,
    .snd_key2 = KR_VOID,
    .snd_rsmkey = KR_VOID,
    .snd_len = 0,
    .snd_code = 0
  };
  PSEND(&msg);
}

/* Convenience routine for creating an EROS thread */
uint32_t 
ethread_new_thread(cap_t kr_bank, uint32_t stack_size,
		   uint32_t program_counter,
		   /* result */ cap_t kr_new_thread)
{
  result_t result;

  /* Request a portion of the heap for this thread's stack */
  void * stack = malloc(stack_size);

  if (stack == NULL)
    return RC_Ethread_Malloc_Err;

  result = ethread_new_thread1(kr_bank,
                   (uint8_t *)stack + stack_size, (void *)program_counter,
		   kr_new_thread);
  if (result != RC_OK) {
    free(stack);
    return result;
  }

  ethread_start(kr_new_thread);
  
  /* Fabricate a start key */
  result = capros_Process_makeStartKey(kr_new_thread, 0, kr_new_thread);
  assert(result == RC_OK);

  return RC_OK;
}
