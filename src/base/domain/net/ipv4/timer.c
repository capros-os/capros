/*
 * Copyright (C) 1998, 1999, Jonathan S. Shapiro.
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

#include <stddef.h>
#include <eros/target.h>
#include <eros/Invoke.h>
#include <eros/NodeKey.h>
#include <eros/ProcessKey.h>
#include <eros/TimeOfDay.h>
#include <eros/machine/io.h>

#include <domain/domdbg.h>
#include <domain/Runtime.h>

#include "constituents.h"
#include "timer.h"
#include "keyring.h"

static uint32_t month_length[12] = {
  31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31
};

uint8_t inline
cmos_cmosByte(unsigned byte)
{
  outb(byte,0x70);
  return  inb(0x71);
}

#define BcdToBin(val) ((val)=((val)&15) + ((val)>>4)*10)
#define  yeartoday(year) (IsLeapYear(year) ? 366 : 365)

static inline
bool IsLeapYear(uint32_t yr)
{
  if (yr % 400 == 0)
    return true;
  if (yr % 100 == 0)
    return false;
  if (yr % 4 == 0)
    return true;
  return false;
}


void
getTimeOfDay(TimeOfDay* tod)
{
  uint32_t i = 0;
  uint32_t yr = 0;
  
  tod->sec = cmos_cmosByte(0x0);
  tod->min = cmos_cmosByte(0x2);
  tod->hr = cmos_cmosByte(0x4);
  tod->dayOfWeek = cmos_cmosByte(0x6);
  tod->dayOfMonth = cmos_cmosByte(0x7);
  tod->month = cmos_cmosByte(0x8);
  tod->year = cmos_cmosByte(0x9);
  
  tod->sec = BcdToBin(tod->sec);
  tod->min = BcdToBin(tod->min);
  tod->hr = BcdToBin(tod->hr);
  tod->dayOfMonth = BcdToBin(tod->dayOfMonth);
  tod->dayOfWeek = BcdToBin(tod->dayOfWeek);
  tod->month = BcdToBin(tod->month);
  tod->year = BcdToBin(tod->year);
  if (tod->year < 70)           /* correct for y2k rollover */
    tod->year += 100;
  
  tod->year += 1900;            /* correct for century. */
  
  tod->dayOfYear = 0;
  for (i = 0; i < tod->month; i++)
    tod->dayOfYear += month_length[i];
  
  tod->dayOfYear += tod->dayOfMonth;
  
  if (tod->month > 1 && IsLeapYear(tod->year))
    tod->dayOfYear++;

  /* Compute coordinated universal time: */
  tod->utcDay = 0;
  for (yr = 1970; yr < tod->year; yr++)
    tod->utcDay += yeartoday(yr);
  tod->utcDay += tod->dayOfYear;
}


uint64_t 
difftime(TimeOfDay *present, TimeOfDay *past) 
{
  uint64_t interval;

  interval =   60*(60*(24*(365*((present->year - past->year) % 100) + 
			   30*(present->month - past->month) + 
			   present->dayOfMonth - past->dayOfMonth) + 
		       present->hr - past->hr) + 
		   present->min - past->min ) +
    present->sec - past->sec;
  
  return interval;
}
