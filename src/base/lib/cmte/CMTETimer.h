#ifndef __CMTETIMER_H
#define __CMTETIMER_H
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

#include <eros/machine/atomic.h>
#include <eros/Link.h>

/* CMTETime_t is a time value in the same units as returned by
   capros_Sleep_getPersistentMonotonicTime(). */
typedef uint64_t CMTETime_t;

typedef struct CMTETimer {
  Link link;
  CMTETime_t expiration;
  void (*function)(unsigned long);
  unsigned long data;
} CMTETimer;

#define CMTETimer_Define(name, _function, _data) \
  CMTETimer name = { \
    .link = { .next = NULL }, \
    .function = (_function), \
    .data = (_data) \
  }

result_t CMTETimer_setup(void);
void CMTETimer_init(CMTETimer * timer,
  void (*function)(unsigned long), unsigned long data);

/* Returns false if timer was inactive, true if was active. */
bool CMTETimer_delete(CMTETimer * timer);

/* Returns false if timer was inactive, true if was active. */
bool CMTETimer_setExpiration(CMTETimer * timer, CMTETime_t expirationTime);

/* Returns false if timer was inactive, true if was active. */
bool CMTETimer_setDuration(CMTETimer * timer, uint64_t durationNsec);
CMTETime_t CMTETimer_remainingTime(CMTETimer * timer);

INLINE int
CMTETimer_IsPending(CMTETimer * timer)
{
  return timer->link.next != NULL;
}

#endif // __CMTETIMER_H
