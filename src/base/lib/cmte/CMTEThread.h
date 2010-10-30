#ifndef __CMTETHREAD_H
#define __CMTETHREAD_H
/*
 * Copyright (C) 2007, 2008, 2009, Strawberry Development Group.
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
 * Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */
/* This material is based upon work supported by the US Defense Advanced
Research Projects Agency under Contract No. W31P4Q-07-C-0070.
Approved for public release, distribution unlimited. */

#include <eros/target.h>	// get result_t

// Values that are not thread numbers:
#define noThread (-1)
#define noThread2 (-2)

/* If stackSize > 0x1f000, returns RC_capros_key_RequestError.
 * If no more threads can be created (max is 32),
 *   returns RC_capros_CMTEThread_NoMoreThreads.
 * May return any exception from capros_ProcCre_createProcess().
 *
 * The return value from start_routine is not used.
 * FIXME: Change it to return void.
 */
/* FIXME: there should be separate allocate and start functions.
 * Allocate may fail due to lack of space, but start cannot. */
#define RC_capros_CMTEThread_NoMoreThreads (-1)
result_t
CMTEThread_create(uint32_t stackSize,
		  void * (* start_routine)(void *), void * arg,
		  /* out */ unsigned int * newThreadNum);
void CMTEThread_exit(void);
void CMTEThread_destroy(unsigned int threadNum);

#endif // __CMTETHREAD_H
