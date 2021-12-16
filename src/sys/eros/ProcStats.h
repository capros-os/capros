#ifndef __PROCSTATS_H__
#define __PROCSTATS_H__
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

struct ProcStat_s {
  uint32_t     pfCount;		/* Total page faults */
  uint32_t     pgRdFlt;		/* Page faults on loads */
  uint32_t     pgWrFlt;		/* Page faults on stores */
  uint32_t     pgWrUpFlt;		/* Page write updgrade faults */
  uint64_t  evtCounter0;		/* Hardware event counter 0 */
  uint64_t  evtCounter1;		/* Hardware event counter 1 */
} ;

typedef struct ProcStat_s ProcStat;

#endif /* __PROCSTATS_H__ */
