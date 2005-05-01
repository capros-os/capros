#ifndef __STALLQUEUE_H__
#define __STALLQUEUE_H__
/*
 * Copyright (C) 2003, Jonathan S. Shapiro.
 *
 * This file is part of the EROS Operating System.
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

#include <kerninc/Link.h>

struct Activity;

/* Stall Queues are simply link structures. The struct exists against
   the possibility that it may become useful to do various sorts of
   per-queue book keeping for testing purposes. */
typedef struct StallQueue StallQueue;
struct StallQueue {
  Link q_head;
};


INLINE void
sq_Init(StallQueue *thisPtr)
{
  link_Init(&thisPtr->q_head);
}

bool sq_IsEmpty(StallQueue* thisPtr);

void sq_WakeAll(StallQueue* thisPtr, bool verbose /*@ default false @*/);
void sq_WakeSome(StallQueue* thisPtr, unsigned long mask,
		 unsigned long orBits,
		 unsigned long match, 
		 bool cancelRetry, bool verbose);


#define INITQUEUE(name) { &name.q_head, &name.q_head }
#define DEFQUEUE(name) \
   struct StallQueue name = { { &name.q_head, &name.q_head } }

#endif /* __STALLQUEUE_H__ */
