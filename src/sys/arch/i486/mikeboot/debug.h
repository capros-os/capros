/*
 * Copyright (C) 1998, 1999, Jonathan Adams.
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


#define dbg_disk	0x01u   /* debug initialization logic */
#define dbg_heap	0x02u   /* debug heap mgmt logic */
#define dbg_load	0x04u   /* kernel/ramdisk load */
#define dbg_unimpl	0x80u   /* unimplemented stuff */
#define dbg_step	0x100u  /* do everything stepwise */
#define dbg_bootalloc	0x200u  /* calls to BootAlloc() */

/* Following should be an OR of some of the above */
#define dbg_flags   ( dbg_load | 0u )

/* #define ASM_DEBUG */
#define CND_DEBUG(x) (dbg_##x & dbg_flags)
#define DEBUG(x) if (CND_DEBUG(x))
#define DEBUG2(x,y) if (((dbg_##x|dbg_##y) & dbg_flags) == (dbg_##x|dbg_##y))
