#ifndef __TIMEPAGE_HXX__
#define __TIMEPAGE_HXX__
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


typedef struct timeval {
  uint32_t tv_secs;
  uint32_t tv_usecs;
} timeval;

/* structure of information on the TimePage */
#define TIMEPAGE_VERSION 1
typedef struct TimePageStruct {
  uint32_t       tps_version;	/* version of this structure */
  struct timeval tps_sinceboot;	/* elapsed time since boot */
  struct timeval tps_wall;	/* elapsed time since epoch */
} TimePageStruct;

#endif /* __TIMEPAGE_HXX__ */
