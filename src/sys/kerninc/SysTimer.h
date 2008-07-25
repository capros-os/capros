#ifndef __SYSTIMER_H__
#define __SYSTIMER_H__
/*
 * Copyright (C) 1998, 1999, Jonathan S. Shapiro.
 * Copyright (C) 2006, 2007, 2008, Strawberry Development Group.
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

extern uint32_t loopsPer8us;

extern uint64_t sysT_latestTime;

struct Activity;
extern struct Activity * ActivityChain;

uint64_t sysT_NowPersistent(void);

void sysT_ResetWakeTime();
uint64_t sysT_WakeupTime(void);
void sysT_WakeupAt(void);
void sysT_actWake(struct Activity * act, uint32_t rc);
  
void sysT_AddSleeper(struct Activity *, uint64_t wakeTime);
void sysT_CancelAlarm(struct Activity *);

void sysT_BootInit();

#ifdef KT_TIMEPAGE
void sysT_InitTimePage();
extern struct ObjectHeader *sysT_TimePageHdr;
#endif

bool IsLeapYear(uint32_t yr);

#endif /* __SYSTIMER_H__ */
