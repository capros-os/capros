#ifndef __KERNINC_CPU_H__
#define __KERNINC_CPU_H__

/*
 * Copyright (C) 2003, Jonathan S. Shapiro.
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

/* This is the beginnings of an attempt to gather per-CPU state into a
 * single place. It is not YET an SMP implementation. */

typedef struct CPU CPU;
struct CPU {
  uint64_t preemptTime;		/* when this processor should be preempted */
};

#if NCPU == 1
extern CPU theSingleCPU;
#define cpu (&theSingleCPU)
#endif

void cpu_BootInit(void);

#endif /* __KERNINC_CPU_H__ */
