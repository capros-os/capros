#ifndef __PROCESSSTATE_H__
#define __PROCESSSTATE_H__

/*
 * Copyright (C) 1998, 1999, 2001, Jonathan S. Shapiro.
 * Copyright (C) 2005, 2007, Strawberry Development Group.
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

/* This file resides in eros/ because the kernel and various sorts of
   keepers must agree on the values.

   Note that this file is also loaded by low-level assembly code, so
   any C structures must be suitably ifdef'd
   */

/* We have departed from the KeyKOS design in our handling of fault
   codes.  In KeyKOS, the process fault code space and the segment
   error code space were separate namespaces.  Since the segment
   keeper can pass faults to the process keeper, we have unified the
   name spaces into a single fault code space.  This unification
   somewhat simplifies keeper invocation, which is a good thing.  See
   the "Keeper" concepts section of the object reference manual.

   After going back and forth for a while, I decided to just add all
   of the weird processor-specific faults here where no other fault
   code would do.  StackSegFault will never be generated on any
   non-x86 machine, but it's easier to have it in the enum than not.

   Segment error codes from the GNOSIS reference:

   1 write protect
   2 invalid key {other than data}
   3 invalid address {too big}
   4 path passes thru node that belongs to the process
   5 invalid data key
   6 tree too deep
   7 slot 15 of red segment node not a data key
   8 bad format for slot 15 {format key} of red segment node
   9 ssc out of range
   10 window key offset not a multiple of slot size
   11 hardware page damage
   12 hardware node damage
   13 background window key was used but there is no background key

   */

/* Odd codes mean that PC points to next instruction */

/* FAULT CODES */
#define capros_Process_FC_NoFault	      0	  /* process is not faulted */

/* SEGMENT FAULT CODES */
#define capros_Process_FC_InvalidAddr        1  /* reference to undefined address */
#define capros_Process_FC_AccessViolation	      2  /* write on RO subseg */

#define capros_Process_FC_TraverseLimit           16  /* segment tree is too deep */
#define capros_Process_FC_MalformedSpace       17  /* format key of red seg not a number key */

#define FC_SegHwPageDamage    18  /* page is broken */
#define FC_SegHwNodeDamage    19  /* node is broken */

/* PROCESS FAULT CODES */
#define FC_NoAddrSpace	      32  /* process has no address space */
#define capros_Process_FC_MalformedProcess   33  /* process malformed */
#define FC_NoSchedule	      34  /* process lacks a schedule key */
#define FC_BadGenRegs	      35  /* gen regs holds non-number key */
#define FC_RegValue	      36  /* reg values inappropriate */
#define FC_BreakPointFault    37  /* BPT with PC at bpt instr */
#define FC_BreakPointTrap     38  /* BPT with PC after bpt instr */
#define FC_BadOpcode	      39  /* bad or undefined opcode */
#define FC_DivZero	      40  /* divide by zero exception */
#define FC_ForeignInvocation  41  /* Process has no key registers */
#define FC_BadEntryBlock      42  /* Invocation had bad entry block */
#define FC_BadExitBlock       43  /* Invocation had bad exit block */
//			44
#define FC_BadSegReg          45  /* Segment register holds invalid value */
#define FC_NoFPU              46  /* Floating point unit not present */
#define FC_FloatingPointError 47  /* Floating point exception or fault */
#define FC_Alignment          48  /* Alignment error */
    
/* MACHINE SPECIFIC FAULTS FOR X86 */

#define FC_GenProtection      128 /* hardware protection violation */
#define FC_StackSeg           129 /* hardware protection violation */
#define FC_Overflow           130 /* intel-specific overflow error */
#define FC_Bounds             131 /* intel-specific bounds error */
#define capros_Process_arch_i386_FC_SegNotPresent      132 /* intel-specific error */
#define FC_InvalidTSS         133 /* intel-specific error */
#define FC_SIMDFloatingPointError 134 /* intel-specific error */

#define RS_Available 0
#define RS_Waiting   1
#define RS_Running   2

/* PROCESS FLAGS */
#define PF_Faulted   0x1 /* process currently has a fault */
/* NOTE that PF_Foreign is not yet implemented, though it won't be
   hard to do now that the flag is defined. */
#define PF_Foreign   0x2 /* process should not make invocations */
#define PF_ExpectingMsg 0x4 /* if process is RS_Available or RS_Waiting,
			it is expecting to receive a message.
			(Cleared on a call to a keeper.) */

/* Fault classifications used by the kernel when reporting faults: */
#define OC_SEGFAULT   0
#define OC_PROCFAULT  1

#endif /* __PROCESSSTATE_H__ */
