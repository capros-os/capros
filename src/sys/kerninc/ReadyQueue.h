#ifndef __READYQUEUE_H__
#define __READYQUEUE_H__
/*
 * Copyright (C) 2003, Jonathan S. Shapiro.
 * Copyright (C) 2008, Strawberry Development Group.
 *
 * This file is part of the CapROS Operating System,
 * and is derived from the EROS Operating System.
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
#include <kerninc/StallQueue.h>

typedef struct ReadyQueue ReadyQueue;

struct ReadyQueue {
  StallQueue queue;		/* queue of ready activities */  

  /* mask for act_RunQueueMap.
     For ReadyQueues in prioQueues,
       this field has 1 << (index of this queue in prioQueues).
     For ReadyQueues in res_ReserveTable,
       this field has 1 << capros_SchedC_Priority_Reserve. */
  uint32_t mask;

  void * other;                  /* misc. field. will hold pointer to reserve*/ 

  /* When an activity is waking up and needs to be placed on this ReadyQ, the
     per-ReadyQ doWakeup function is called to place the activity on the target
     readyQ. */
  /* For ReadyQueues in prioQueues (except capros_SchedC_Priority_Reserve),
       this field has readyq_GenericWakeup.
     For ReadyQueues in res_ReserveTable,
       and prioQueues[capros_SchedC_Priority_Reserve],
       this field has readyq_ReserveWakeup. */
  void (*doWakeup)(ReadyQueue *, struct Activity *);

  /* When the quanta of a activity has run out, call this function to place the
     activity back on the readyQ */
  /* For ReadyQueues in prioQueues,
       this field has readyq_Timeout.
     For ReadyQueues in res_ReserveTable,
       this field has readyq_ReserveTimeout. */
  void (*doQuantaTimeout)(ReadyQueue *, struct Activity *);
};

extern ReadyQueue prioQueues[];

extern ReadyQueue *dispatchQueues[];

#endif /* __READYQUEUE_H__ */
