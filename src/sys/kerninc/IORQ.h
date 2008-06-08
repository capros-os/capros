#ifndef __IORQ_H__
#define __IORQ_H__
/*
 * Copyright (C) 2008, Strawberry Development Group.
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

#include <eros/Link.h>

struct PageHeader;
struct ObjectRange;

typedef struct IORequest {
  Link lk;
  struct PageHeader * pageH;	// page to read into or write from
  struct ObjectRange * objRange;
  uint64_t Oid;		// OID or LID requested, must be in objRange
  void (*doneFn)(struct IORequest * ioReq);	// function to call when done
  uint16_t requestCode;
} IORequest;

typedef struct IORQ {
  Link lk;	/* If free, lk.next is link in the free list.
		Otherwise lk is the chain of linked IORequests. */
} IORQ;

extern IORQ IORQs[];

void IORQ_Init(void);
IORQ * IORQ_Allocate(void);
void IORQ_Deallocate(IORQ * iorq);

#endif /* __IORQ_H__ */
