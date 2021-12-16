#ifndef __ASM_H__
#define __ASM_H__
/*
 * Copyright (C) 2010, Strawberry Development Group.
 *
 * This file is part of the CapROS Operating System.
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

// Control Register 0 declarations:

#define CR0_MP 0x2
#define CR0_EM 0x4
#define CR0_TS 0x8
#define CR0_ET 0x10

#ifndef __ASSEMBLER__

uint32_t ReadCR0();
void WriteCR0(uint32_t cr0);

#ifdef EROS_HAVE_FPU
void FPUInit(void);
void FPUSave(floatsavearea_t * p);
void FPURestore(const floatsavearea_t * p);
//uint16_t ReadFCW(void);
void ClearTSFlag();
#endif

uint64_t rdtsc();
void halt(char c);
uint32_t rdcounter0();
uint32_t rdcounter1();

#endif // __ASSEMBLER__

#endif /* __ASM_H__ */
