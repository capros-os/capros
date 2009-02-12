#ifndef __ETHREAD_H__
#define __ETHREAD_H__
/*
 * Copyright (C) 2003, Jonathan S. Shapiro.
 * Copyright (C) 2008, 2009, Strawberry Development Group.
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

/* Convenience routine for creating an EROS thread. Pass in:
   a space bank key,
   the stack pointer for the new thread,
   the address to which to set the program counter,
   and the keyreg into which to store the process key to the new thread.

   The current process's key registers are copied to the new thread,
   except that KR_SELF will be a process key to the new process,
   and KR_TEMP0 is unspecified.
 */
/* Clobbers only KR_TEMP0. */
/* Returns: an exception from capros_ProcCre_createProcess,
   RC_Ethread_Unexpected_Err, or RC_OK. */
uint32_t 
ethread_new_thread1(cap_t kr_bank, uint32_t stack_pointer,
		   uint32_t program_counter,
		   /* result */ cap_t kr_new_thread);

/* After calling ethread_new_thread1, call ethread_start to start
 * the new thread running. */
/* Clobbers only KR_TEMP0. */
void ethread_start(cap_t thread);

/* Convenience routine for creating an EROS thread. Pass in:
   a space bank key,
   a requested stack size for the new thread,
   the address to which to set the program counter,
   and the keyreg into which to store the start key to the new thread.

   Similar to ethread_new_thread1, except allocates the stack using malloc,
   starts the thread running, and creates a start key with keyInfo 0.
 */
/* Clobbers only KR_TEMP0. */
/* Returns: an exception from capros_ProcCre_createProcess, RC_OK, or: */
#define RC_Ethread_Unexpected_Err      13
#define RC_Ethread_Malloc_Err          14

uint32_t ethread_new_thread(cap_t kr_bank, uint32_t stack_size,
			    uint32_t program_counter,
			    /* result */ cap_t kr_new_thread);


#endif /* __ETHREAD_H__ */
