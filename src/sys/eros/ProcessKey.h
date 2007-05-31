#ifndef __PROCESSKEY_H__
#define __PROCESSKEY_H__

/*
 * Copyright (C) 1998, 1999, Jonathan S. Shapiro.
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

/*
 * This file resides in eros/ because the kernel and the invocation
 * library various must agree on the values.
 */


/* Slots of a process root. Changes here should be matched in the
 * architecture-dependent layout files and also in the mkimage grammar
 * restriction checking logic. */
#define ProcSched             0
#define ProcKeeper            1
#define ProcAddrSpace         2
#define ProcCapSpace          3	/* unimplemented */
#define ProcGenKeys           3 /* for now */
#define ProcIoSpace           4	/* unimplemented */
#define ProcSymSpace          5
#define ProcBrand             6
/*			      7    unused */
#define ProcTrapCode          8
#define ProcPCandSP           9
#define ProcFirstRootRegSlot  8
#define ProcLastRootRegSlot   31

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
#ifndef __KERNEL__	// these are not kernel procedures

struct Registers;

uint32_t process_make_start_key(uint32_t procKey, uint16_t keyData, uint32_t toReg);
uint32_t process_make_fault_key(uint32_t procKey, uint32_t toReg);
uint32_t process_get_regs(uint32_t krProcess, struct Registers *regs);
uint32_t process_set_regs(uint32_t krProcess, struct Registers *regs);
uint32_t process_swap(uint32_t krProcess, uint32_t slot, uint32_t krFrom, uint32_t krTo);
uint32_t process_copy(uint32_t krProcess, uint32_t slot, uint32_t krTo);
uint32_t process_copy_keyreg(uint32_t krProcess, uint32_t slot, uint32_t krTo);
uint32_t process_swap_keyreg(uint32_t krProcess, uint32_t slot, uint32_t krFrom, uint32_t krTo);

#endif // __KERNEL__
#endif /* __ASSEMBLER__ */

#endif /* __PROCESSKEY_H__ */
