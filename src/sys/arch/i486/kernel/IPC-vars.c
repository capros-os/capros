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

#include <kerninc/kernel.h>
#include <kerninc/Activity.h>
#include <arch-kerninc/PTE.h>

Activity *act_curActivity  __attribute__((aligned(4),
						   section(".data")));
uint32_t act_yieldState __attribute__((aligned(4),
						   section(".data")));
#ifdef OPTION_KERN_TIMING_STATS
uint32_t inv_delta_reset __attribute__((aligned(4),
					section(".data")));
uint64_t inv_delta_cy __attribute__((aligned(4),
				     section(".data")));
uint64_t inv_handler_cy __attribute__((aligned(4),
				       section(".data")));
uint64_t pf_delta_cy __attribute__((aligned(4),
				    section(".data")));
uint64_t kpr_delta_cy __attribute__((aligned(4),
				    section(".data")));
#ifdef OPTION_KERN_EVENT_TRACING
uint64_t inv_delta_cnt0 __attribute__((aligned(4),
				       section(".data")));
uint64_t inv_delta_cnt1 __attribute__((aligned(4),
				       section(".data")));
uint64_t pf_delta_cnt0 __attribute__((aligned(4),
				      section(".data")));
uint64_t pf_delta_cnt1 __attribute__((aligned(4),
				      section(".data")));
uint64_t kpr_delta_cnt0 __attribute__((aligned(4),
				      section(".data")));
uint64_t kpr_delta_cnt1 __attribute__((aligned(4),
				      section(".data")));
#endif
#endif
PTE* pte_kern_fstbuf  __attribute__((aligned(4),
				      section(".data")));
PTE* pte_kern_ptebuf  __attribute__((aligned(4),
				      section(".data")));
