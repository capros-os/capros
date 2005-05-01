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

/*
 * EROS Disk Boostrap - bios version
 *
 * This code has been rewritten from scratch more times than I really care
 * to think about -- sufficiently that I feel justified in removing prior 
 * copyrights, as NONE of the original code remains.  The most recent rewrite 
 * occurred in February of 1998.
 *
 * Setting aside the problem of 16/32 bit issues in the assembler, the main
 * problem is that this code simply has quite a lot to do.
 *
 * I spent a long time trying to write this code so that the UNIX 'dd'
 * command would be sufficient to copy a bootable image to a disk.  In the 
 * end, I failed.  While I did manage to build code that should have done
 * the job in the requisite amount of space, I was completely unable to get 
 * the hard disk boot sequence reliably debugged.
 *
 * The current version maintains a block table.  It can be copied directly 
 * to a floppy using 'dd', and will work fine that way.
 *
 * The main problem with this design is that the block table becomes invalid 
 * if the geometry of the disk changes.  This can happen, for example, if 
 * the user resets their disk geometry from LBA to extended CHS.  For the
 * moment, I have concluded that it is more important for this to work 
 * (at all), and that patching the bootstrap in that even is not 
 * prohibitively difficult.  
 *
 * Note that there is actually no general solution to this problem, as some
 * drives do not preserve linear block sequence when switching translations.
 * On such drives, the drive needs to be rewritten anyway.
 *
 * One of these days, I should add code to verify that the assumed geometry
 * matches the alleged geometry.
 *
 * BUG: THIS CODE IS NOT DESIGNED TO BE RUN FROM THE MBR OF A HARD DISK!!!!
 *
 * On entry:	
 *
 *	CS:EIP are 0x0:0x7c00.  
 *      dl     holds BIOS id of boot drive.
 *	ES:SI  points to partition table entry IFF this was a 
 *	       hard disk boot.  This is the only RELIABLE way
 *             to know which image was booted, as there may be
 *             multiple instances of EROS installed, and some MBR
 *             multiboot loaders do not set the active flag on the 
 *             in-memory partition table.
 *
 * ASSUMPTIONS
 *
 * Because we are stuck with a 32 bit assembler, the design objective
 * of this code is to get to 32-bit mode as fast as possible.  
 *	
 * Most modern drive controllers will read past end of cylinder or head 
 * without objection, but for the sake of legacy drive support most BIOS
 * ROMs will not *issue* a drive command that does this, instead failing
 * the request.
 */
	
#include <eros/i486/asm.h>
#include <eros/i486/target-asm.h>
		
#include "boot-asm.h"
	
#define SIGNATURE		0xaa55
#define PARTABLE_START		0x1be
#define PARTABLE_END		0x1fe
#define EROS_PARTITION_TYPE	0x95
#define BOOT1_SIZE		0x200
#define BOOT2_EXT_SECTORS	0xf		/* 32k */
#define	BLKCNT			14
	
#define ABS16(x) .word EXT(x)-_start;
#define REL16(x) .word EXT(x)-0f; 0:;
#define REL8(x)  .byte EXT(x)-0f; 0:;
	
/* simplify some common cases for which space is a concern. */
#define jb_short	.byte 0x72; 
#define je_short	.byte 0x74;
#define jz_short	.byte 0x74;
#define jnc_short	.byte 0x73;
#define jle_short	.byte 0x7e;
#define jmp  .byte 0xe9; 
#define jmp_short  .byte 0xeb; 
#define call .byte 0xe8; 
#define jb   .byte 0xf, 0x82; 
#define jc   .byte 0xf, 0x82; 
#define jnc  .byte 0xf, 0x83; 
#define jae  .byte 0xf, 0x83; 
#define je   .byte 0xf, 0x84; 
#define jne  .byte 0xf, 0x85; 
#define jl   .byte 0xf, 0x8c; 
#define ret  .byte 0xc3;
	
#define CMSG(x) movb $x,%al; call REL16(cmsg)
#define CSTOP(c) movb $c,%al; jmp REL16(cstop)
	
	.file	"boot1.S"

	.text

/* N.B.: The layout of the following variables MUST MATCH that of VolHdr
         in disk/LowVolume.hxx */

ENTRY(_start)
ENTRY(start)
	/* no data32 prefix is needed, because we aren't coming back here.  
	   The extra bytes of the offset just get ignored. */
	data32
	ljmp $0x7c0, $EXT(realstart)

	/* boot information needed by the EROS kernel: */
ASMVAR16(Version)
	.long   1		/* sector of first division table */
ASMVAR16(PageSize)
	.long   EROS_PAGE_SIZE	/* sector of first division table */
ASMVAR16(DivTable)
	.long   16		/* sector of first division table */
ASMVAR16(AltDivTable)
	.long   0		/* sector of alternate division table */
ASMVAR16(VolFlags)
	.long   0		/* flags interpreted by second stage 
				 * boot loader */
ASMVAR16(BootSectors)
	.long   0		/* Number of sectors of bootstrap code */ 
ASMVAR16(VolSectors)
	.long   0		/* Number of sectors to load into ramdisk */
ASMVAR16(ZipLen)
	.long   0		/* Length of zipped portion of volume */
ASMVAR16(IplKey)
	.space 16		/* IPL process */
ASMVAR16(IplSysId)
	.long 0			/* Unique System ID */
	.long 0			/* Unique System ID */
Signature:
	.byte 'E', 'R', 'O', 'S'

LEXT(block_table_count)
	.word   /* BLKCNT */ 4
LEXT(block_table)
	/* Following prototable is accurate for 1.44M floppy. Total
	 * should be 64 sectors... */
	.word	1	/* cylinder/sector */
	.byte	0	/* head */
	.byte	18	/* nsect */
	
	.word	1	/* cylinder/sector */
	.byte	1	/* head */
	.byte	18	/* nsect */
	
	.word	0x101	/* cylinder/sector */
	.byte	0	/* head */
	.byte	18	/* nsect */
	
	.word	0x101	/* cylinder/sector */
	.byte	1	/* head */
	.byte	10	/* nsect */
	. = EXT(block_table) + BLKCNT*4
	
ENTRY16(realstart)
	mov	%cs, %ax
	mov	%ax,%ds

	/* set up stack for BIOS use:	 */
	mov	%ax,%ss
	/* stack at 16k */
	/* movw	$0x4000,%sp */
	.byte 0xbc; .word 0x4000

	/* move %es to %fs, where we won't stomp on it while talking to 
	  BIOS: */
	push	%es
	pop	%fs
	mov	%ax,%es
	
	cld	/* set copy direction */
	
	CMSG('b')
	
	cmpb	$0x80,%dl	/* check boot drive type */
	jb_short	REL8(fd)

LEXT(hd)
	/* Hard-disk path can undoubtedly be shrunk by bytecoding, but I 
	   want to wait until I have a hard disk handy to test on. */
	CMSG('h')
	
#if 0
	/* We are loading from a hard disk. Pick up the partition info from
	   the partition table. */
	addr32
	movb	%fs:4(%esi),%al
	cmpb	$EROS_PARTITION_TYPE,%al
	je_short	REL8(got_partition)
	CSTOP('P')
#endif
	
LEXT(got_partition)
#if 0
	/* drive is still in %dl -- do not clobber!!! */
	/* load head and sec/cyl into appropriate registers */
	addr32
	movb	%fs:1(%esi),%dh /* head */
	addr32
	mov	%fs:2(%esi),%cx /*sect, cyl */
#else
	addr32
	mov	EXT(block_table),%cx
	addr32
	movb	EXT(block_table)+2,%dh
#endif
	
	jmp_short	REL8(do_load)

	/* note that if we get really tight for space the floppy
	   logic can move into the partition table area.  I'm not
	   doing that now because I want to preserve the option to
	   just image copy the bootstrap as long as possible. */

LEXT(fd)
	CMSG('f')
	/* Reset the floppy drive subsystem, which spins up the drive.
	   On older machines the boot fails intermittently if this is
	   not done.  */
	push	%dx		/* probably not necessary */
	movb	$0x0, %ah
	int	$0x13
	pop	%dx		/* probably not necessary */

	xorb	%dh,%dh
	/* some BIOS's can boot from drive B, so don't change drive number. */

	/* movw	$0x1,%cx	*//* sec, cyl */
	.byte 0xb9; .word 0x1	/* sec, cyl */
	
	/* FALL THROUGH */
LEXT(do_load)
	CMSG('r')
	/* NOTE -- if we do the floppy probe thing we will need to
	   revisit the push logic. */
	/* FALL THROUGH */
	
	/* save the disk location of the starting partition to the stack. */
	push	%ecx		/* sec, cyl */
	push	%edx		/* drive, head */
	
	
	/* following not byte coded so can include BOOT2_SEG macro */
	data32
	movl	$BOOT2_SEG,%eax
	mov	%ax,%es
	xorl	%ebx,%ebx	
	
	addr32
	movl	$EXT(block_table), %esi
	
	addr32
	movb	EXT(block_table_count), %cl	/* how many to read */
	movzbl	%cl, %ecx

	/* Note that this makes an extra call with count of zero at 
	   the end. */
LEXT(load_loop)
	CMSG('l')

	pushl	%ecx		/* number of block table entries */
	
	cld
	lodsl	/* word */	/* cylinder/sector */
	movl	%eax,%ecx
	lodsb			/* head */
	movb	%al,%dh
	lodsb			/* # of sectors to load */
	movb	$0x2,%ah
	pushl	%eax
	int	$0x13
	jnc_short	REL8(load_more)

	/* handle read error */
	CSTOP('R')

LEXT(load_more)
	CMSG('.')
	/* adjust the %ebx offset for the next chunk. */
	popl	%eax
	movzbl	%al, %eax
	shll	$9, %eax		/* 512 bytes sectors */
	addl	%eax, %ebx
	popl	%ecx
	loop	EXT(load_loop)
		
LEXT(start_extended_bootstrap)
	CMSG('\r')
	CMSG('\n')
	
	/* extended bootstrap is now loaded.  Switch to 32-bit mode
	   and start it! */
	
	pop	%edx		/* drive, head */
	pop	%ecx		/* sec, cyl */

	data32
	ljmp	$BOOT2_SEG, $BOOT1_SIZE

	CSTOP('x')
/*
 * cmsg: write a character in %al to console.  Trashes %eax.
 */
ENTRY16(cmsg)
	/*
	 * Use BIOS "int 10H Function 0Eh" to write character in teletype mode
	 *	%ah = 0xe	%al = character
	 *	%bh = page	%bl = foreground color (graphics modes)
	 */

	push	%ebx
	
	data32
	mov	$0x01, %ebx
	movb	$0xe, %ah
	int	$0x10		/* display a byte */

	pop	%ebx
	ret

/*
 * stop: write the error message in %ds:%esi to console and halt
 */
ENTRY16(cstop)
	push	%eax
	CMSG('!')
	pop	%eax
	call	REL16(cmsg)
	
LEXT(lo_halt)
	hlt
	jmp_short	REL16(lo_halt)	/* halt doesnt actually halt forever */
	
	/* END OF CODE MARKER, FOR DEBUGGING -- FROM HERE DOWN IS DATA */
	.align 2
	.byte  0xad, 0xde, 0xef, 0xbe
	
	.space 16
	
	/* end of code marker, for debugging */
	.align 2
	.byte  0xad, 0xde, 0xef, 0xbe

	
	/* Partition table resides here */
	. = _start + PARTABLE_START
	
	. = _start + PARTABLE_END
	
	.value	SIGNATURE

	. = EXT(_start) + BOOT1_SIZE
