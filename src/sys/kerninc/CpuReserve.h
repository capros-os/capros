/*
 * Copyright (C) 2002, Jonathan S. Shapiro.
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

/* Functions for reservation creation and management */

#include "kernel.h"
#include "Machine.h"
#include "util.h"
#include <kerninc/ReadyQueue.h>

struct Reserve {
  int index;                /* index in table of this reserve */
  bool isActive;            /* true iff timeAcc < duration */

  uint64_t duration;        /* time slice in period */
  uint64_t period;          /* period of time over which slice should be
                               allocated */

  uint64_t nextDeadline;    /* end of this period */
  uint64_t timeAcc;         /* time accumulated this period*/
  uint64_t totalTimeAcc;    /* total usage over reserve lifetime */
  uint64_t lastSched;       /* when this reserve began running */
  uint64_t lastDesched;     /* time of last deschedule */

  ReadyQueue readyQ;        /* readyQ info for this reserve */
};

typedef struct Reserve Reserve;

extern Reserve* res_ReserveTable;

void res_AllocReserves();

void res_AllocResTree();

Reserve* res_NextReadyReserve();

void res_ReplenishReserve(Reserve* thisPtr);

Reserve * res_GetNextReserve();

Reserve *res_GetEarliestReserve();

void res_SetActive(uint32_t ndx);

void res_SetInactive(uint32_t ndx);

void res_DeplenishReserve(Reserve *r);

uint64_t NextTimeInterrupt(Reserve *current);

void res_ActivityTimeout(uint64_t now);

void res_SetReserveInfo(uint32_t p, uint32_t d, uint32_t ndx);
