#ifndef __CMTE_H
#define __CMTE_H
/*
 * Copyright (C) 2007, 2008, Strawberry Development Group.
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

/* Common declarations for the CapROS Multi-Threading Environment. */


/* Memory layout: */

/* 0x0: nothing. To catch misuse of NULL pointers.
0x1000: beginning of code, read only */

#define LK_STACK_BASE 0x00400000
/* Thread stacks are allocated as follows:
0x00400000 through 0x0041ffff: area for stack for thread 0
  The actual stack is from 0x0041ffff down through whatever its size is.
0x00420000 through 0x0043ffff: area for stack for thread 1
etc. */
/* The highest addresses in the stack contain the Linux
thread_info structure, of which only preempt_count is used. */
#define LK_LGSTACK_AREA 17
#define LK_STACK_AREA (1ul << LK_LGSTACK_AREA)	// 0x00020000

#define LK_MAX_THREADS 32

#define LK_MAPS_BASE 0x00800000	// area for ioremap()

#define LK_DATA_BASE 0x00c00000 // .data, .bss, and heap, backed by a VCSK
// Limit of memory is 0x02000000, because that is the limit of an ARM
// small space. 


/* Key registers: */
#include <domain/Runtime.h>
#define KR_KEYSTORE 2
#define KR_LSYNC      KR_APP(0)	// start key to lsync process
#define KR_MAPS_GPT   KR_APP(1) // 
#define KR_SLEEP      KR_APP(2) // to speed up getting jiffies
#define KR_OSTREAM    KR_APP(3)
#define KR_CMTE(i)    KR_APP(4+(i))	// first available key reg for program


/* Slots in the supernode KR_KEYSTORE: */
/* For 0 < i < LK_MAX_THREADS (*not* 0),
     keystore[LKSN_THREAD_PROCESS_KEYS + i] has the process key to thread i,
   and keystore[LKSN_THREAD_RESUME_KEYS + i] is reserved for
     a resume key to thread i. */
#define LKSN_THREAD_PROCESS_KEYS 0
#define LKSN_THREAD_RESUME_KEYS  LK_MAX_THREADS
#define LKSN_STACKS_GPT          (LKSN_THREAD_RESUME_KEYS+LK_MAX_THREADS)
#define LKSN_CMTE                (LKSN_STACKS_GPT+1) // available for program

/* Constituents of driver constructors: */
#define KC_TEXT       0 // read-only portion of the program
#define KC_DATAVCSK   1	// initial data of the program
#define KC_INTERPRETERSPACE 2
#define KC_SNODECONSTR 3
#define KC_STARTADDR  4 // for dynamically-constructed drivers
#define KC_SLEEP      5
#define KC_OSTREAM    6
#define KC_CMTE(i)    (7+(i))

#ifndef __ASSEMBLER__
#include <domain/cmte_stackPtr.h>

/* The highest word on the stack contains thread local data: */
static inline void * *
CMTE_getThreadLocalDataAddr(void)
{
  return (void * *)
    ((current_stack_pointer & ~(LK_STACK_AREA - 1))
                + LK_STACK_AREA - sizeof(void *));
}
#endif

#endif // __CMTE_
