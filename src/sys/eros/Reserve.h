#ifndef __RESERVE_H__
#define __RESERVE_H__

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

#define PRIO_INACTIVE   -2
#define PRIO_KERNIDLE	-1
#define PRIO_USERIDLE	0
#define PRIO_NORMAL     8
#define PRIO_HIGH       15

#define STD_CPU_RESERVE	16
#define MAX_CPU_RESERVE	256

#ifndef __ASSEMBLER__

typedef struct CpuReserveInfo CpuReserveInfo;
struct CpuReserveInfo {
  uint32_t index;
  uint64_t period;
  uint64_t duration;
  uint64_t quanta;
  uint64_t start;
  int rsrvPrio;
  int normPrio;
};

#endif

#endif /* __RESERVE_H__ */
