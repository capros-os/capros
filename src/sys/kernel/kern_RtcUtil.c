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

#include <kerninc/kernel.h>
#include <kerninc/Machine.h>
#include <kerninc/mach-rtc.h>

// Convert a Gregorian date to seconds since BASE_YEAR.
uint32_t
kernMktime(unsigned int year, unsigned int mon,
  unsigned int day, unsigned int hour,
  unsigned int min, unsigned int sec)
{
#if 0
  printf("kernMktime yr=%u mon=%u day=%u hr=%u min=%u sec=%u\n",
         year, mon, day, hour, min, sec);
#endif

  // Make March the first month since Feb has a leap day:
  if (mon > 2) {
    mon -= 2;
  } else {
    mon += 12-2;
    year -= 1;
  }

  unsigned long ret = year/4 - year/100 + year/400 + (367*mon)/12 + day;
#if 0
  printf("kernMktime ret=%u %u %u\n", ret, ret+(unsigned long)year*365, 0);
#endif
  ret += (unsigned long)year*365;
  // Make relative to Jan 1, BASE_YEAR
  ret -= (BASE_YEAR-1)*365 + (BASE_YEAR-1)/4
         - (BASE_YEAR-1)/100 + (BASE_YEAR-1)/400
         + (367*11)/12 + 1;	// compiler calculates this constant
  ret = ret * 24 + hour;	// hours
  ret = ret * 60 + min;		// minutes
  ret = ret * 60 + sec;		// seconds
  return ret;
}

void
kernGmtime(uint32_t inTime, struct kernTm * out)
{
  unsigned int days = inTime / (24*60*60);
  unsigned int time = inTime - days * (24*60*60);
  unsigned int hour = time / (60*60);
  time -= hour * 60*60;
  unsigned int minutes = time / 60;
  out->seconds = time - minutes * 60;
  out->minutes = minutes;
  out->hours = hour;

  // Convert days since 1/1/BASE_YEAR to Gregorian date.
  // Reference: Collected Algorithms from CACM.
  days += (BASE_YEAR-1)*365 + (BASE_YEAR-1)/4
          - (BASE_YEAR-1)/100 + (BASE_YEAR-1)/400
          + (367*10)/12 + 2;	// compiler calculates this constant
#define DAYS_PER_5_MONTHS (31+30+31+30+31)
#define DAYS_PER_4_YEARS (365*4 + 1)
#define DAYS_PER_100_YEARS (DAYS_PER_4_YEARS * 25 - 1)
#define DAYS_PER_400_YEARS (DAYS_PER_100_YEARS * 4 + 1)
  long days4 = days * 4 - 1;
  int century = days4 / DAYS_PER_400_YEARS;
  int dayOfCentury4 = ((days4 - century * DAYS_PER_400_YEARS) / 4) * 4 + 3;
  int yearOfCentury = dayOfCentury4 / DAYS_PER_4_YEARS;
  int year = (century * 100) + yearOfCentury;
  int dayOfYear5 = ((dayOfCentury4 - yearOfCentury * DAYS_PER_4_YEARS) / 4 + 1)
                   * 5 - 3;
  int month = dayOfYear5 / DAYS_PER_5_MONTHS;
  out->date = (dayOfYear5 - month * DAYS_PER_5_MONTHS) / 5 + 1;
#if 0
  printf("%d %d %d %d %d %d\n",
    inTime / (24*60*60), days, century, yearOfCentury, dayOfYear5, month);
#endif
  // Convert from March=0 to March=3:
  if (month < 10) {
    month += 3;
  } else {
    month -= 9;
    year++;
  }

  out->month = month;
  out->year = year;
}
