#ifndef __SYSTIMER_H__
#define __SYSTIMER_H__
/*
 * Copyright (C) 1998, 1999, Jonathan S. Shapiro.
 * Copyright (C) 2006, Strawberry Development Group.
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

struct Activity;
extern struct Activity * ActivityChain;

extern volatile uint64_t sysT_now;
extern volatile uint64_t sysT_wakeup;

/* Former member functions of SysTimer and Timer */

void sysT_ResetWakeTime();

void sysT_Wakeup(savearea_t *);
  
void sysT_AddSleeper(struct Activity *);
void sysT_CancelAlarm(struct Activity *);

void sysT_ActivityTimeout();

INLINE uint64_t 
sysT_Now()
{
  uint64_t t1;
  uint64_t t2;
  
  do {
    t1 = sysT_now;
    t2 = sysT_now;
  } while (t1 != t2);
  
  return t1;
}

void sysT_BootInit();

#ifdef KT_TIMEPAGE
void sysT_InitTimePage();
extern struct ObjectHeader *sysT_TimePageHdr;
#endif

#endif /* __SYSTIMER_H__ */
