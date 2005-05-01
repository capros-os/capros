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

#define SEC_SIZE		0x200		/* 512 bytes */

#define BOOT2_SEG		0x8000
#define BOOT2_ADDR		0x80000
#define BOOT2_STACK		0x10000		/* 64K */

#define BOOT2PA(va, ty) ((ty) ( ((kva_t) va) + BOOT2_ADDR ))
#define PA2BOOT(va, ty) ((ty) ( ((kva_t) va) - BOOT2_ADDR ))

/* Certain of the boot2 BIOS wrappers reference the boot2 stack from
   16 bit mode.  It is therefore imperative that the boot2 stack fall
   within the first 64K.  To avoid consuming space with buffers and
   the like, we place them in a separate heap.  Note that this heap is
   NOT reachable from the BIOS routines. */
#define BOOT2_HEAP_PADDR	0x100000
#define BOOT2_HEAP_SZ		0x20000		/* 128k */
#define BOOT2_HEAP_PTOP		(BOOT2_HEAP_PADDR+BOOT2_HEAP_SZ)

#define CR0_PE_ON		0x1
#define CR0_PE_OFF		0xfffffffe
#define SCRATCH_SEG		0x1000
#define SCRATCH_OFFSET		0x0
#define SCRATCH_NSEC 128

#define EXT_MEM_START 0x100000u

#define KERNCODESEG  0x08
#define KERNDATASEG  0x10

#define BOOTCODE32   0x18
#define BOOTDATA32   0x20
#define BOOTCODE16   0x28

#define DATA32SEG    0x10

/* Conventional GDT indexes. */
#define KERN_CS_INDEX		1
#define KERN_DS_INDEX		2
#define BOOT_CS_INDEX		3
#define BOOT_DS_INDEX		4
#define BOOT_CS16_INDEX		5

#if !defined(__ASSEMBLER__)

#ifndef min
#define min(x, y) ((x) < (y) ? (x) : (y))
#endif /* min */
#define RAM_DISK_ID		0xff

#ifdef __cplusplus
extern "C" {
#endif
/* Global variables defined in boot1.S */
extern uint32_t DivTable;
extern uint32_t AltDivTable;
extern uint32_t VolFlags;
extern uint32_t BootSectors;
extern uint32_t VolSectors;
extern uint32_t ZipLen;
extern uint64_t IplSysId;
#ifdef __cplusplus
}
#endif


#ifdef __GNUC__
/* The following DOES NOT provide a solution for strings!! */
#define LOW_VAR(type, name, value) \
  static type name  __attribute__ ((section (".text"))) = value
#else
#error "Need mechanism for low variables!!"
#endif

#endif /* !defined(__ASSEMBLER__) */
