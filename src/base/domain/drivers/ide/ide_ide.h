#ifndef __IDE_IDE_H__
#define __IDE_IDE_H__
/*
 * Copyright (C) 1998, 1999, Jonathan S. Shapiro.
 * Copyright (C) 2005, 2007, Strawberry Development Group
 *
 * This file is part of the CapROS Operating System,
 * and is derived from the EROS Operating System.
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
/* This material is based upon work supported by the US Defense Advanced
Research Projects Agency under Contract No. W31P4Q-07-C-0070.
Approved for public release, distribution unlimited. */

#include <eros/target.h>
#include <eros/Invoke.h>
#include <idl/capros/Sleep.h>
#include <eros/machine/io.h>
#include <eros/DevicePrivs.h>
#include <domain/ConstructorKey.h>
#include <domain/Runtime.h>
#include <domain/domdbg.h>

#define KR_START       KR_APP( 0 )
#define KR_OSTREAM     KR_APP( 1 )
#define KR_DEVICEPRIVS KR_APP( 2 )
#define KR_SLEEP       KR_APP( 3 )
#define KR_IDECALL     KR_APP( 4 )


#define OC_GIVEKEY         16
#define OC_DO_IO           17
#define OC_IO_READ         18
#define OC_IO_WRITE        19
#define OC_IO_RESPONSE     20
#define OC_INTERRUPT       21

#define READ               OC_IO_READ
#define WRITE              OC_IO_WRITE


#define MAX_HWIF  2		/* number of HWIFs we will use */
#define MAX_DRIVE 2		/* number of drives per HWIF */


/* CS0 register offsets */
#define IDE_DATA	  0x0	/* 16 bits */
#define IDE_ERROR         0x1	/* when reading */
#define IDE_FEATURE       IDE_ERROR /* when writing */
#define IDE_NSECS         0x2
#define IDE_SECTOR        0x3
#define IDE_CYL_LO        0x4
#define IDE_CYL_HI        0x5
#define IDE_DRV_HD        0x6	/* 101DHHHH */
#define IDE_STATUS        0x7	/* clears IRQ */
#define IDE_CMD           IDE_STATUS

/* CS1 register offsets */
#define IDE_CTL_ALTSTATUS     0x206	/* doesn't clear IRQ */
#define IDE_CTL_DRV_ADDR      0x207	/* rd only  */

/* Bits of HD_STATUS */
#define ERR_STAT	0x01
#define INDEX_STAT	0x02
#define ECC_STAT	0x04	/* Corrected error */
#define DRQ_STAT	0x08
#define SEEK_STAT	0x10
#define WRERR_STAT	0x20
#define READY_STAT	0x40
#define BUSY_STAT	0x80

#define BAD_R_STAT		(BUSY_STAT   | ERR_STAT)
#define BAD_W_STAT		(BAD_R_STAT  | WRERR_STAT)
#define BAD_STAT		(BAD_R_STAT  | DRQ_STAT)
#define DRIVE_READY		(READY_STAT  | SEEK_STAT)
#define DATA_READY		(DRIVE_READY | DRQ_STAT)

#define OK_STAT(stat,good,bad) (((stat)&((good)|(bad)))==(good))

/* Values for HD_COMMAND */
#define WIN_RESTORE		0x10
#define WIN_READ		0x20
#define WIN_WRITE		0x30
#define WIN_VERIFY		0x40
#define WIN_FORMAT		0x50
#define WIN_INIT		0x60
#define WIN_SEEK 		0x70
#define WIN_DIAGNOSE		0x90
#define WIN_SPECIFY		0x91	/* set drive geometry translation */
#define WIN_SETIDLE1		0xE3
#define WIN_SETIDLE2		0x97

#define WIN_DOORLOCK		0xde	/* lock door on removeable drives */
#define WIN_DOORUNLOCK		0xdf	/* unlock door on removeable drives */

#define WIN_MULTREAD		0xC4	/* read sectors using multiple mode */
#define WIN_MULTWRITE		0xC5	/* write sectors using multiple mode */
#define WIN_SETMULT		0xC6	/* enable/disable multiple mode */
#define WIN_IDENTIFY		0xEC	/* ask drive to identify itself	*/
#define WIN_SETFEATURES		0xEF	/* set special drive features */
#define WIN_READDMA		0xc8	/* read sectors using DMA transfers */
#define WIN_WRITEDMA		0xca	/* write sectors using DMA transfers */

/* Additional drive command codes used by ATAPI devices. */
#define WIN_PIDENTIFY		0xA1	/* identify ATAPI device	*/
#define WIN_SRST		0x08	/* ATAPI soft reset command */
#define WIN_PACKETCMD		0xa0	/* Send a packet command. */

/* Bits for HD_ERROR */
#define MARK_ERR	0x01	/* Bad address mark */
#define TRK0_ERR	0x02	/* couldn't find track 0 */
#define ABRT_ERR	0x04	/* Command aborted */
#define ID_ERR		0x10	/* ID field not found */
#define ECC_ERR		0x40	/* Uncorrectable ECC error */
#define	BBD_ERR		0x80	/* block marked bad */

/*
 * Timeouts in milliseconds:
 */
#define WAIT_DRQ	(50)	/* 50msec - spec allows up to 20ms */
#define WAIT_READY	(30)	/* 30msec - should be instantaneous */
#define WAIT_PIDENTIFY	(1000)	/* 1sec   - should be less than 3ms (?) */
#define WAIT_WORSTCASE	(31000)	/* 31sec  - worst case when spinning up */
#define WAIT_CMD	(10000)	/* 10sec  - maximum wait for an IRQ to happen */

/* Danger, Will Robinson! the DRV_ADDRESS register is shared with the
 * floppy change status bit, so if you're not careful...
 */




//struct IDE {
typedef enum media {
    med_disk,
    med_cdrom,
    med_tape
} media_t;

typedef enum chipset {
    cs_unknown,
    cs_generic,
    cs_triton,
    cs_cmd640,
#if 0
    cs_dtc2278,
    cs_ali14xx,
    cs_qd6580,
    cs_umc8672,
    cs_ht6560b
#endif
} chipset_t;

//  static void InitChipsets();
//};


/* structure returned by HDIO_GET_IDENTITY, as per ANSI ATA2 rev.2f spec */
struct ide_driveid {
  uint16_t	config;		/* lots of obsolete bit flags */
  uint16_t	cyls;		/* "physical" cyls */
  uint16_t	reserved2;	/* reserved (word 2) */
  uint16_t	heads;		/* "physical" heads */
  uint16_t	track_uint8_ts;	/* unformatted bytes per track */
  uint16_t	sector_bytes;	/* unformatted bytes per sector */
  uint16_t	sectors;	/* "physical" sectors per track */
  uint16_t	vendor0;	/* vendor unique */
  uint16_t	vendor1;	/* vendor unique */
  uint16_t	vendor2;	/* vendor unique */
  uint8_t	serial_no[20];	/* 0 = not_specified */
  uint16_t	buf_type;
  uint16_t	buf_size;	/* 512 byte increments; 0 = not_specified */
  uint16_t	ecc_bytes;	/* for r/w long cmds; 0 = not_specified */
  uint8_t	fw_rev[8];	/* 0 = not_specified */
  uint8_t	model[40];	/* 0 = not_specified */
  uint8_t	max_multsect;	/* 0=not_implemented */
  uint8_t	vendor3;	/* vendor unique */
  uint16_t	dword_io;	/* 0=not_implemented; 1=implemented */
  uint8_t	vendor4;	/* vendor unique */
  uint8_t	capability;	/* bits 0:DMA 1:LBA 2:IORDYsw 3:IORDYsup*/
  uint16_t	reserved50;	/* reserved (word 50) */
  uint8_t	vendor5;	/* vendor unique */
  uint8_t	tPIO;		/* 0=slow, 1=medium, 2=fast */
  uint8_t	vendor6;	/* vendor unique */
  uint8_t	tDMA;		/* 0=slow, 1=medium, 2=fast */
  uint16_t	field_valid;	/* bits 0:cur_ok 1:eide_ok */
  uint16_t	cur_cyls;	/* logical cylinders */
  uint16_t	cur_heads;	/* logical heads */
  uint16_t	cur_sectors;	/* logical sectors per track */
  uint16_t	cur_capacity0;	/* logical total sectors on drive */
  uint16_t	cur_capacity1;	/*  (2 words, misaligned int)     */
  uint8_t	multsect;	/* current multiple sector count */
  uint8_t	multsect_valid;	/* when (bit0==1) multsect is ok */
  uint32_t	lba_capacity;	/* total number of sectors */
  uint16_t	dma_1word;	/* single-word dma info */
  uint16_t	dma_mword;	/* multiple-word dma info */
  uint16_t  	eide_pio_modes; /* bits 0:mode3 1:mode4 */
  uint16_t  	eide_dma_min;	/* min mword dma cycle time (ns) */
  uint16_t  	eide_dma_time;	/* recommended mword dma cycle time (ns) */
  uint16_t  	eide_pio;       /* min cycle time (ns), no IORDY  */
  uint16_t  	eide_pio_iordy; /* min cycle time (ns), with IORDY */
  uint16_t  	reserved69;	/* reserved (word 69) */
  uint16_t  	reserved70;	/* reserved (word 70) */
  /* uint16_t reservedxx[57];*/	/* reserved (words 71-127) */
  /* uint16_t vendor7  [32];*/	/* vendor unique (words 128-159) */
  /* uint16_t reservedyy[96];*/	/* reserved (words 160-255) */
#ifdef RMG
  bool CheckLbaCapacity();
#endif
};
#endif /* __IDE_IDE_HXX__ */
