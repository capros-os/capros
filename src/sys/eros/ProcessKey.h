#ifndef __PROCESSKEY_H__
#define __PROCESSKEY_H__

/*
 * Copyright (C) 1998, 1999, Jonathan S. Shapiro.
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

/*
 * This file resides in eros/ because the kernel and the invocation
 * library various must agree on the values.
 */

/* Local Variables: */
/* comment-column:34 */
/* End: */

/* ORDER and RESULT code values: */

#define OC_Process_Copy             0
#define OC_Process_Swap             1
#define OC_Process_CopyKeyReg       2
#define OC_Process_SwapKeyReg       3

#define OC_Process_GetRegs32        128
#define OC_Process_GetFloatRegs     129
#if 0
#define OC_Process_GetCtrlInfo32    130
#endif

#define OC_Process_SetRegs32        144
#define OC_Process_SetFloatRegs     145

#define OC_Process_MkStartKey        160
#define OC_Process_MkResumeKey       161
#define OC_Process_MkFaultKey        162

#define OC_Process_MkProcessAvailable 176
#define OC_Process_MkProcessWaiting   177

#define OC_Process_SwapMemory32      192

#define RC_Process_Running		1
#define RC_Process_Malformed	 	2
#define RC_Process_NoKeys	 	3

#ifndef __ASSEMBLER__

#include <eros/machine/Registers.h>

uint32_t process_make_start_key(uint32_t procKey, uint16_t keyData, uint32_t toReg);
uint32_t process_make_fault_key(uint32_t procKey, uint32_t toReg);
uint32_t process_get_regs(uint32_t krProcess, struct Registers *regs);
uint32_t process_set_regs(uint32_t krProcess, struct Registers *regs);
uint32_t process_swap(uint32_t krProcess, uint32_t slot, uint32_t krFrom, uint32_t krTo);
uint32_t process_copy(uint32_t krProcess, uint32_t slot, uint32_t krTo);
uint32_t process_copy_keyreg(uint32_t krProcess, uint32_t slot, uint32_t krTo);
uint32_t process_swap_keyreg(uint32_t krProcess, uint32_t slot, uint32_t krFrom, uint32_t krTo);

#endif /* __ASSEMBLER__ */

#endif /* __PROCESSKEY_H__ */
