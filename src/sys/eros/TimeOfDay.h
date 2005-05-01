#ifndef __TIMEOFDAY_HXX__
#define __TIMEOFDAY_HXX__
/*
 * Copyright (C) 1998, 1999, Jonathan S. Shapiro.
 *
 * This file is part of the EROS Operating System runtime library.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, 59 Temple Place - Suite 330 Boston, MA 02111-1307, USA.
 */

typedef struct TimeOfDay {
  uint32_t utcDay;
  uint32_t year;
  uint32_t dayOfYear;
  uint8_t dayOfWeek;
  uint8_t month;
  uint8_t dayOfMonth;
  uint8_t hr;
  uint8_t min;
  uint8_t sec;
} TimeOfDay;

#define OC_TimeOfDay_Now     	 1
#define OC_TimeOfDay_UpTime  	 16

#ifndef __ASSEMBLER__
extern uint32_t tod_GetTimeOfDay(uint32_t krTOD, TimeOfDay *);
#endif /* __ASSEMBLER__ */

#endif /* __TIMEOFDAY_HXX__ */
