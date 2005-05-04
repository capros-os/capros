#ifndef __IDE_HWIF_H__
#define __IDE_HWIF_H__
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

/* This file requires #include <kerninc/BlockDev.hxx> */
/* This file requires #include <device/ide_ide.hxx> */
/* This file requires #include <device/ide_drive.hxx> */
/* This file requires #include <include/io.h> */

struct ide_group;

typedef enum dma_cmd {
    dma_read,
    dma_write,
    dma_abort,
    dma_check,
    dma_status_bad,
    dma_transferred,
    dma_begin
} dma_cmd_t;
  

struct ide_hwif { 
    struct ide_hwif          *next;
    struct ide_hwif          *readyChain;
    int		      ndx;
    uint32_t          nUnits;
    struct ide_drive  drive[MAX_DRIVE];
    uint8_t	      irq;		/* nonzero == assigned. */
    uint32_t          ioBase;		/* io port at which interface is assigned. */
    char              *name;
    chipset_t      chipset;
    uint32_t          totalMountedUnits;
    /* Do not use bitmaps -- shitty compilation. */
    bool	      noprobe;	/* do not probe for this interface */
    bool	      present;	/* interface is present */

    /* uint8_t	      selected_drive; */
    uint8_t	      cur_drive;	/* which drive is active */
  
    struct ide_group     *group;

    bool (*int_handler)();

    dma_cmd_t dma_cmd;
    
    /* Return true on success */
    bool (*dma_handler)(dma_cmd_t, struct ide_drive*);

#ifdef RMG
    void Put8(uint16_t reg, uint8_t value)
    {
	outb(reg + ioBase, value);
    }
  
    uint8_t Get8(uint16_t reg)
    {
	return inb(reg + ioBase);
    }
  

    void PutCtl8(uint8_t value)
    {
	outb(ctlBase, value);
    }
  
    uint8_t GetCtl8()
    {
	return inb(ctlBase);
    }

  
    void Put16(uint16_t reg, uint16_t value)
    {
	outb(reg + ioBase, value); // THIS IS WRONG NEED 16 bits not 8
    }
  
    uint16_t Get16(uint16_t reg)
    {
	return 1; // inh(reg + ioBase);
    }
  
    void PutCtl16(uint16_t value)
    {
	old_outh(ctlBase, value);
    }
  
    uint16_t GetCtl16()
    {
	return inh(ctlBase);
    }

    void SelectDrive(uint32_t ndx);

    static void OnIntr(struct IntAction*);
    /* Interrupt handler functions return true when the current phase of
     * the request is over.  If further interrupts are expected for the
     * current phase, they return false.
     */
    bool (ide_hwif::*int_handler)();
    void SetHandler(uint8_t unit, bool (ide_hwif::*handler)());
  
    /* Interrupt handlers: */
    bool SetMultmodeIntr();
    bool RecalibrateIntr();
    bool SetGeometryIntr();
    bool ReadIntr();
    bool WriteIntr();
    bool MultWriteIntr();
  
    /* Issue a simple drive command */
    void DoCmd(uint8_t unit, uint8_t cmd, uint8_t nsec, bool (ide_hwif::*handler)());

    void DoReset1(uint8_t unit, bool suppressAtapi);
    void DoReset(uint8_t unit);
  
    /* DMA handler fn pointer -- most controllers still don't do onboard
     * DMA.  For the ones that do, this pointer points to the
     * handler/setup function.  It's not clear to me if this should be
     * part of ide_drive or ide_hwif.
     */

    enum dma_cmd {
	dma_read,
	dma_write,
	dma_abort,
	dma_check,
	dma_status_bad,
	dma_transferred,
	dma_begin
    } ;
  
    /* Return true on success */
    bool (*dma_handler)(dma_cmd, ide_drive*);

    ide_hwif();

    void Probe();

    /* EXTERNAL BLOCKDEV PROTOCOL: */

    void MountUnit(uint8_t unit);	/* called by mount daemon */
    void GetUnitInfo(uint8_t unit, BlockUnitInfo&);

    void InsertRequest(Request* req);
    void StartIO();

    /* INTERFACE CALLED BY HW GROUP LOGIC: */

    /* Return true if request is completed, false otherwise. */
    bool StartRequest(Request*);
    Request* GetNextRequest();
    static void ActivateTask(){ 
	//pTask->ActivateTask()
    };

#endif

};

void hwif_DoCmd( struct ide_hwif *hwif, uint8_t unit, uint8_t cmd, uint8_t nsec, bool (*handler)());

void hwif_DoReset1( struct ide_hwif *hwif, uint8_t unit, bool suppressAtapi);
void hwif_DoReset( struct ide_hwif *hwif, uint8_t unit);

struct IntAction {};

#define PUT8( reg, value ) Put8(drive->hwif, reg, value )
void Put8(struct ide_hwif *, uint16_t, uint8_t );


#define GET8( reg ) Get8( drive->hwif, reg )
uint8_t Get8(struct ide_hwif *hwif, uint16_t reg);

struct Request* hwif_GetNextRequest( struct ide_hwif *hwif );

void hwif_SelectDrive( struct ide_hwif *hwif, uint32_t ndx);
void hwif_SetHandler(struct ide_hwif *hwif, uint8_t unit, bool (*handler)());
void hwif_StartIO( struct ide_hwif *hwif );
bool hwif_StartRequest( struct ide_hwif *hwif, struct Request*);

/* Interrupt handlers: */
bool hwif_SetMultmodeIntr( struct ide_hwif *hwif );
bool hwif_RecalibrateIntr( struct ide_hwif *hwif );
bool hwif_SetGeometryIntr( struct ide_hwif *hwif );
bool hwif_ReadIntr( struct ide_hwif *hwif );
bool hwif_WriteIntr( struct ide_hwif *hwif );
bool hwif_MultWriteIntr( struct ide_hwif *hwif );

void request_Finish( struct Request *req);
bool request_IsCompleted( struct Request *req );

struct Request *requestQueue_GetNextRequest( struct RequestQueue *rq );
void requestQueue_InsertRequest( struct RequestQueue *rq, struct Request *req);
bool requestQueue_IsEmpty( struct RequestQueue *rq );
void requestQueue_RemoveRequest( struct RequestQueue *rq, struct Request *req );



#endif /* __IDE_HWIF_H__ */
