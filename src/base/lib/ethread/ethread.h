#ifndef __ETHREAD_H__
#define __ETHREAD_H__

/*
 * Copyright (C) 2003, Jonathan S. Shapiro.
 *
 * This file is part of the EROS Operating System runtime library.
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

/* Convenience routine for creating an EROS thread. Pass in a space
   bank key, a keyreg for temporary storage, a requested stack size
   for the new thread, the address to which to set the program
   counter, and the keyreg into which to store the start key to the
   new thread. */

/* Exceptions */
#define RC_Ethread_Create_Process_Err  10
#define RC_Ethread_Copy_AddrSpace_Err  11
#define RC_Ethread_Copy_SchedKey_Err   12
#define RC_Ethread_Copy_Keyregs_Err    13
#define RC_Ethread_Malloc_Err          14
#define RC_Ethread_SetRegs_Err         15
#define RC_Ethread_Make_Fault_Key_Err  16
#define RC_Ethread_Make_Start_Key_Err  17

uint32_t ethread_new_thread(cap_t kr_bank, cap_t kr_tmp, uint32_t stack_size,
			    uint32_t program_counter,
			    /* result */ cap_t kr_new_thread);


#endif /* __ETHREAD_H__ */
