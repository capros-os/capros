#ifndef __READYQUEUE_H__
#define __READYQUEUE_H__
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
#include <kerninc/StallQueue.h>

typedef struct ReadyQueue ReadyQueue;

struct ReadyQueue {
  StallQueue queue;		/* queue of ready activities */  
  uint32_t mask;                /* mask for prioQ */
  void *other;                  /* misc. field. will hold pointer to reserve*/ 
  /* make the other a void * ptr. This way, you
     can make it point to a reserve table entry */

  /* When an activity is waking up and needs to be placed on this ReadyQ, the
     per-ReadyQ doWakeup function is called to place the activity on the target
     readyQ. */
  /* This field has readyq_GenericWakeup or readyq_ReserveWakeup. */
  void (*doWakeup)(ReadyQueue *, struct Activity *);

  /* When the quanta of a activity has run out, call this function to place the
     activity back on the readyQ */
  void (*doQuantaTimeout)(ReadyQueue *, struct Activity *);
};


extern ReadyQueue prioQueues[];
extern ReadyQueue *dispatchQueues[];
#define DeepSleepQ prioQueues[pr_Never].queue
#define KernIdleQ prioQueues[pr_Idle].queue
#define HighPriorityQ prioQueues[pr_High].queue


#endif /* __READYQUEUE_H__ */
