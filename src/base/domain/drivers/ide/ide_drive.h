#ifndef __IDE_DRIVE_HXX__
#define __IDE_DRIVE_HXX__
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

/* This file requires #include <kerninc/IoRequest.hxx> */
/* This file requires #include <kerninc/BlockDev.hxx> */
/* This file requires #include <device/ide_ide.hxx> */

//#include "IoRequest.hxx"
#include "ide_ide.h"
//#include "BlockDev.hxx"

struct ide_hwif;



enum Type {
    Read_Cmd,
    Write_Cmd,
#if 0
    WriteVerify_Cmd,	/* for now */
#endif
    /* Any command above these is a unit-specific command.  Some
     * drivers use the request structure to handle such things.
     */
    Plug_Cmd,
};

enum { NUM_IOCMD = Plug_Cmd + 1 };



struct RequestQueue{ };
struct Request { 

    uint8_t     unit;           /* unit on which request should occur */
    uint8_t     cmd;

    kva_t	req_ioaddr;	/* base address */
    uint32_t	req_start;	/* starting sector number */
    uint32_t	req_nsec;	/* total number of sectors */
    uint32_t    nsec;

    uint32_t    nError; /* number of errors on this request */

};

struct CHS {
    uint32_t cyl;
    uint32_t hd;
    uint32_t sec;
};

struct ide_drive {
    union {
	uint8_t        all;

	struct {
	    uint8_t setGeom : 1;		/* issue drive geometry specification */
	    uint8_t recal : 1;		/* seek to cyl 0 */
	    uint8_t setMultMode : 1;	/* set multmode count */
#if 0
	    uint8_t pio : 1;		/* set pio mode */
#endif
	} b;
    }             flags;

    struct RequestQueue  rq;		/* request queue. */

    struct ide_driveid   id;		/* vendor id info */
    bool	        present;	/* unit is present */
    bool		removable;	/* unit is removable */
    bool	        needsMount;	/* unit wants to be mounted */
    bool	        mounted;	/* unit is mounted */
    bool	        use_dma;	/* DMA (if available) can be used */

    bool		active;		/* an operation is in progress on this
					 * drive.
					 */
    struct ide_hwif      *hwif;
  
    uint32_t	multCount;	/* max sectors per xfer */
    uint32_t	multReq;	/* requested mult sector setting */
    media_t	media;		/* cdrom, disk, tape */
    /* bool		io_32bit; */
#if 0
    CHS		p_chs;		/* physical geometry */
#endif
    struct CHS		l_chs;		/* logical geometry */
    struct CHS		b_chs;		/* BIOS geometry */

    union {
	uint8_t all;
	struct {
	    uint8_t head : 4;
	    uint8_t unit : 1;
	    uint8_t : 1;			/* always 1 */
	    uint8_t lba : 1;		/* using LBA, not CHS */
	    uint8_t : 1;			/* always 1 */
	} b;
    } select;
    
    uint8_t		ctl;		/* control register value */
    uint32_t          ndx;

    char          name[5];

#if 0
protected:
    void FlushResidue();

    uint8_t DumpStatus (const char *msg, uint8_t stat, struct Request* req);

    void MultWrite(struct Request *req);
    void OutputData(void *buffer, unsigned int wcount);
    void InputData(void *buffer, unsigned int wcount);
    void AtapiIdentify(uint8_t);
    uint8_t Identify(uint8_t);

    void HandleDriveFlags();

    void Error(const char*, uint8_t);

    void EndDriveCmd(uint8_t stat, uint8_t err);
    void DoDriveCommand(struct Request*);
    void DoDiskRequest(struct Request*);

    bool WaitStatus(uint8_t good, uint8_t bad, uint32_t timelimit);
  
public:
    void DoRequest(struct Request*);

    uint32_t DoProbe(uint8_t cmd);
    void Probe();
    uint32_t Capacity();

    ide_drive();

#endif 

};
void drive_DoRequest(struct ide_drive *drive, struct Request * req);
bool drive_WaitStatus(struct ide_drive *drive, uint8_t good, uint8_t bad, uint32_t timelimit);
uint8_t drive_DumpStatus (struct ide_drive *drive, const char *msg, uint8_t stat, struct Request* req);
void drive_Error( struct ide_drive *drive, const char*, uint8_t);
void drive_FlushResidue( struct ide_drive *drive);


void drive_MultWrite( struct ide_drive *drive, struct Request *req);
void drive_OutputData( struct ide_drive *drive, void *buffer, unsigned int wcount);
void drive_InputData( struct ide_drive *drive, void *buffer, unsigned int wcount);

uint32_t drive_DoProve( struct ide_drive *drive, uint8_t cmd );
void drive_Probe( struct ide_drive *drive); 
#endif /* __IDE_DRIVE_HXX__ */
