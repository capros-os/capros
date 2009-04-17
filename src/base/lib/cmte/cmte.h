#ifndef __CMTE_H
#define __CMTE_H
/*
 * Copyright (C) 2007, 2008, 2009, Strawberry Development Group.
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

#include <domain/CMME.h>

/* Thread stacks are allocated as follows:
0x00400000 (LK_STACK_BASE) through 0x0041ffff: area for stack for thread 0
  The actual stack is from 0x0041ffff down through whatever its size is.
0x00420000 through 0x0043ffff: area for stack for thread 1
etc. */
/* The highest addresses in the stack contain the Linux
thread_info structure, of which only preempt_count is used. */

#define LK_MAX_THREADS 32


/* Key registers: */
#include <domain/Runtime.h>
// #define KR_KEYSTORE 2	// in Runtime.h
#define KR_LSYNC      KR_CMME(0)	// start key to lsync process
#define KR_SLEEP      KR_CMME(1)	// to speed up getting jiffies
#define KR_CMTE(i)    KR_CMME(2+(i))	// first available key reg for program


/* Slots in the supernode KR_KEYSTORE: */
/* For 0 < i < LK_MAX_THREADS (*not* 0),
     keystore[LKSN_THREAD_PROCESS_KEYS + i] has the process key to thread i,
   and keystore[LKSN_THREAD_RESUME_KEYS + i] is reserved for
     a resume key to thread i. */
#define LKSN_THREAD_PROCESS_KEYS 0
#define LKSN_THREAD_RESUME_KEYS  LK_MAX_THREADS
#define LKSN_STACKS_GPT          (LKSN_THREAD_RESUME_KEYS+LK_MAX_THREADS)
#define LKSN_CMTE                (LKSN_STACKS_GPT+1) // available for program

/* Required constituents of CMTE constructors:
 * Those required by CMME, plus: */
#define KC_SNODECONSTR	KC_CMME(0)
#define KC_SLEEP     	KC_CMME(1)
#define KC_CMTE(i)    	KC_CMME(2+(i))

#ifndef __ASSEMBLER__
#include <eros/machine/StackPtr.h>

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
