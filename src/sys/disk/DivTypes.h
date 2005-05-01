/* THIS FILE CAN BE MULTIPLY INCLUDED!!! */

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

#ifndef __DIVDECL
#error DivTypes.hxx included with __DIVDECL undefined
#endif

/* EROS Division types: */

__DIVDECL(Unused)
__DIVDECL(Boot)			/* contains boot page */
__DIVDECL(DivTbl)		/* contains division table */
__DIVDECL(Spare)		/* replacement sectors */
__DIVDECL(Kernel)		/* contains kernel code */
__DIVDECL(Log)			/* used for paging */
__DIVDECL(Object)		/* object range */
#if 0
__DIVDECL(Raw)			/* division is a foreign partition,
				 * such as a DOS partition.  EROS will
				 * do I/O to this division on a raw
				 * sector basis. Consistency across
				 * unplanned shutdowns is not
				 * guaranteed. 
				 */
#endif

/* Pseudo division type for failstart processes: */
__DIVDECL(FailStart)
