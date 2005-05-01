#ifndef __CPUFEATURES_H__
#define __CPUFEATURES_H__
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

/* Local Variables: */
/* comment-column:36 */

#define CPUFEAT_FPU   0x01u	    /* processor has FPU */
#define CPUFEAT_VME   0x02u	    /* v86 mode */
#define CPUFEAT_DE    0x04u	    /* debugging extensions */
#define CPUFEAT_PSE   0x08u	    /* page size extensions */
#define CPUFEAT_TSC   0x10u	    /* time stamp counter */
#define CPUFEAT_MSR   0x20u	    /* model specific registers */
#define CPUFEAT_PAE   0x40u	    /* physical address extensions */
#define CPUFEAT_MCE   0x80u	    /* machine check exception */
#define CPUFEAT_CXB   0x100u	    /* compare and exchange byte */
#define CPUFEAT_APIC  0x200u	    /* APIC on chip */
#define CPUFEAT_SEP   0x800u	    /* Fast system call */
#define CPUFEAT_MTRR  0x1000u	    /* Memory type range registers */
#define CPUFEAT_PGE   0x2000u	    /* global pages */
#define CPUFEAT_MCA   0x4000u	    /* machine check architecture */
#define CPUFEAT_CMOV  0x8000u	    /* conditional move instructions */
#define CPUFEAT_FGPAT 0x10000u	    /* CMOVcc/FMOVCC */
#define CPUFEAT_PSE36 0x20000u	    /* 4MB pages w/ 36-bit phys addr */
#define CPUFEAT_PN    0x40000u	    /* processor number */
#define CPUFEAT_MMX   0x800000u	    /* processor has MMX extensions */
#define CPUFEAT_FXSR  0x1000000u    /* streaming float save */
#define CPUFEAT_XMM   0x2000000u    /* streaming SIMD extensions */

#endif /* __CPUFEATURES_H__ */
