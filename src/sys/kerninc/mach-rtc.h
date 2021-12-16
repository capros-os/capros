#ifndef __MACH_RTC_H__
#define __MACH_RTC_H__
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
Research Projects Agency under Contract Nos. W31P4Q-06-C-0040 and
W31P4Q-07-C-0070.  Approved for public release, distribution unlimited. */

#include <kerninc/kernel.h>
#include <kerninc/Machine.h>
#include <idl/capros/RTC.h>

// capros_RTC_time_t is seconds from the beginning of this year.
#define BASE_YEAR 1970

/* Some inline procedures useful for board-specific RTC handlers. */

// Procedures for 2-digit Binary Coded Decimal values.
INLINE unsigned int
BCDToBin(unsigned int dd)
{
  return (dd >> 4) * 10 + (dd & 0xf);
}

INLINE unsigned int
BinToBCD(unsigned int i)
{
  unsigned int tens = i / 10;
  return (tens << 4) + (i - tens * 10);
}

uint32_t kernMktime(unsigned int year, unsigned int mon,
  unsigned int day, unsigned int hour,
  unsigned int min, unsigned int sec);

struct kernTm {
  int seconds;
  int minutes;
  int hours;
  int date;
  int month;
  int year;
};
void kernGmtime(uint32_t inTime, struct kernTm * out);


/* Procedures implemented in architecture-specific files. */

/* RtcSave saves the specified time to nonvolatile media.
 * If the specified time cannot be represented on nonvolatile media,
 * we return -1, otherwise 0. */
int RtcSave(capros_RTC_time_t newTime);

/* RtcRead reads the Real Time Clock. */
capros_RTC_time_t RtcRead(void);

/* RtcSet sets the Real Time Clock. 
 * If the specified time cannot be represented, we return -1.
 * Otherwise we return the number of seconds the caller must wait
 * before the update is seen by RtcRead(). */
int RtcSet(capros_RTC_time_t newTime);

#endif //__MACH_RTC_H__
