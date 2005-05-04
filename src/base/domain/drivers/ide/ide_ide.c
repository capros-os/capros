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
#if 0
#include <kerninc/kernel.hxx>
#include <kerninc/MsgLog.hxx>
#include <kerninc/AutoConf.hxx>
#include <kerninc/IoRegion.hxx>
#include <kerninc/IntAction.hxx>
#include <kerninc/IRQ.hxx>
#include <kerninc/BlockDev.hxx>
#include <kerninc/IoRequest.hxx>
#include <eros/Device.h>
#include <arch/i486/kernel/SysConfig.hxx>
#include <arch/i486/kernel/CMOS.hxx>
#endif


#include "ide_drive.h"
#include "ide_hwif.h"
#include "ide_group.h"
#include "constituents.h"
//#include "IoRequest.h"

bool Probe();
//static bool Attach();

#define OKAY(x,y) if (x != RC_OK) {}
// kprintf(KR_OSTREAM, y)

void
init_ide_drive( struct ide_drive *drive );


#if 0
#include "io.h"
#endif
/* #define IDE_DEBUG */

/* Driver for IDE interface devices and (also) older ST506 devices.
 * According to the ATA Faq, these interfaces are assigned as follows:
 * (REF: ATA Faq)
 * 
 *  Interface   CS0-decode   CS1-decode   IRQ
 *  1           0x1F0-0x1F7  0x3F6-0x3F7  14
 *  2           0x170-0x177  0x376-0x377  15 or 10 -- usually 15
 *  3           0x1E8-0x1EF  0x3EE-0x3EF  12 or 11
 *  4           0x168-0x16F  0x36E-0x36F  10 or 9
 * 
 * The current implementation does not configure interfaces 3 and 4,
 * because there is no real standard for port assignments on
 * interfaces 3 and 4, and I am reluctant to deploy what I cannot
 * test.
 */

const uint16_t ide_cs0[] = { 0x1F0, 0x170, 0x1E8, 0x168 };
const uint16_t ide_irq[] = { 14, 15, 12, 10 };

const char *ide_name[MAX_HWIF] = { "ide0" }; //, "ide1" };

#define BIOS_HD_BASE	 0x1F0

#define OK_TO_RESET_CONTROLLER 0

/* In theory, these should not be statically allocated, but it
 * complexifies things to do otherwise, and they don't really take up
 * all that much space.
 */

struct ide_group group_tbl[MAX_HWIF];
struct ide_hwif  hwif_tbl[MAX_HWIF];


uint8_t Get8(struct ide_hwif *hwif, uint16_t reg)
{
    uint8_t ret;
    ret = inb( reg + hwif->ioBase );
    kprintf( KR_OSTREAM, " GET8: ioBase: 0x%04x  reg: 0x%04x  ret = 0x%02x",
	     hwif->ioBase, reg, ret );

    return ret;
}

//ide_hwif::ide_hwif()
void 
init_ide_hwif(struct ide_hwif *hwif)
{
    int drv = 0;
    uint32_t hwifno = hwif - hwif_tbl;
  
    hwif->ndx = hwifno;
    hwif->irq = 0;		/* probe for this */
    hwif->ioBase = ide_cs0[hwifno];
    /* ctlBase = ide_cs1[hwifno]; */
    hwif->chipset = cs_unknown;
    hwif->noprobe = (hwifno > 1) ? true : false;
    hwif->present = false;

    /* Second unit may be present even if first unit isn't, so... */
    hwif->nUnits = MAX_DRIVE;
    //devClass = DEV_DISK_IDE;
    hwif->name = "ide";
  
    hwif->group = &group_tbl[hwifno];
    hwif->group->hwif[0] = hwif;

#if 0
    /* Given the number of bone-headed IDE controller chips out there,
     * and the fact that more are being discovered daily, proceed on the
     * assumption that the IDE interface chip is brain damaged until
     * proven otherwise.
     */
  
    hwif->serialized = true;
#endif

    hwif->int_handler = 0;
    hwif->dma_handler = 0;
  
    for (drv = 0; drv < MAX_DRIVE; drv++) {
	init_ide_drive( &(hwif->drive[drv]) );
	hwif->drive[drv].ndx = drv;
	hwif->drive[drv].hwif = hwif;
	hwif->drive[drv].select.all = (drv << 4) | 0xa0;
	hwif->drive[drv].name[0] = 'h';
	hwif->drive[drv].name[1] = 'd';
	hwif->drive[drv].name[2] = 'a' + hwifno;
	hwif->drive[drv].name[3] = '0' + drv;
	hwif->drive[drv].name[4] = 0;
    }
}

void
hwif_SetHandler(struct ide_hwif *hwif, uint8_t unit, bool (*handler)())
{
    //  assert (unit == cur_drive);
    //  assert(int_handler == 0);
  
    hwif->int_handler = handler;
    
#ifdef IDE_DEBUG
    kprintf( KR_OSTREAM, "Set int handler to 0x%08x\n", &handler);
#endif
}


/* Issue a simple drive command */
void
hwif_DoCmd(struct ide_hwif *hwif, uint8_t unit, uint8_t cmd, uint8_t nsec, bool (*handler)())
{
    struct ide_drive *drive = &(hwif->drive[unit]);
    
    hwif_SetHandler(hwif, unit, handler);
    PUT8(IDE_CTL_ALTSTATUS, drive[unit].ctl);
    PUT8(IDE_NSECS, nsec);
    PUT8(IDE_CMD, cmd);
}


/*
 * do_reset1() attempts to recover a confused drive by resetting it.
 * Unfortunately, resetting a disk drive actually resets all devices on
 * the same interface, so it can really be thought of as resetting the
 * interface rather than resetting the drive.
 *
 * ATAPI devices have their own reset mechanism which allows them to be
 * individually reset without clobbering other devices on the same interface.
 *
 * Unfortunately, the IDE interface does not generate an interrupt to let
 * us know when the reset operation has finished, so we must poll for this.
 * Equally poor, though, is the fact that this may a very long time to complete,
 * (up to 30 seconds worstcase).  So, instead of busy-waiting here for it,
 * we set a timer to poll at 50ms intervals.
 */
void
//DoReset1(uint8_t unit, bool /* suppressAtapi */)
hwif_DoReset1( struct ide_hwif *hwif,  uint8_t unit, bool suppressAtapi )
{
#ifdef CONFIG_BLK_DEV_IDEATAPI
    /* Interrupts may need to be disabled here... */
  
    /* For an ATAPI device, first try an ATAPI SRST. */
    if (hwif->drive[unit].media != med_disk) {
	//MsgLog::fatal("Reset1 on non-disk not implemented\n");
	kprintf( KR_OSTREAM, "Reset1 on non-disk not implemented");
	if (!suppressAtapi) {
	    if (!keep_settings)
		unmask = 0;
	    SelectDrive(unit);

	    sl_sleep( KR_SLEEP, 20 );
	    Put8(IDE_CMD, WIN_SRST);
#ifdef OLD
	    hwgroup->poll_timeout = jiffies + WAIT_WORSTCASE;
	    ide_set_handler (drive, &atapi_reset_pollfunc, HZ/20);
#endif
	    return;
	}
    }
#endif /* CONFIG_BLK_DEV_IDEATAPI */

    /*
     * First, reset any device state data we were maintaining
     * for any of the drives on this interface.
     */
    for ( unit = 0; unit < MAX_DRIVE; ++unit ) {
	struct ide_drive *pUnit = &(hwif->drive[unit]);
	pUnit->flags.all = 0;
	pUnit->flags.b.setGeom = 1;
	pUnit->flags.b.recal  = 1;

	if (OK_TO_RESET_CONTROLLER)
	    pUnit->multCount = 0;

#ifdef OLD
	if (!pUnit->keep_settings) {
	    pUnit->mult_req = 0;
	    pUnit->unmask = 0;
	    if (pUnit->using_dma) {
		pUnit->using_dma = 0;
		//kprintf( KR_OSTREAM, "%s: disabled DMA\n", pUnit->name);
	    }
	}
#endif

#ifdef OLD
	if (pUnit->mult_req != pUnit->mult_count)
	    pUnit->special.b.set_multmode = 1;
#endif
    }

#if OK_TO_RESET_CONTROLLER
    /*
     * Note that we also set nIEN while resetting the device,
     * to mask unwanted interrupts from the interface during the reset.
     * However, due to the design of PC hardware, this will cause an
     * immediate interrupt due to the edge transition it produces.
     * This single interrupt gives us a "fast poll" for drives that
     * recover from reset very quickly, saving us the first 50ms wait time.
     */
    OUT_BYTE(drive->ctl|6,IDE_CONTROL_REG);	/* set SRST and nIEN */
    //udelay(5);			/* more than enough time */
    sl_sleep( KR_SLEEP, 1 );
    OUT_BYTE(drive->ctl|2,IDE_CONTROL_REG);	/* clear SRST, leave nIEN */
    hwgroup->poll_timeout = jiffies + WAIT_WORSTCASE;
    ide_set_handler (drive, &reset_pollfunc, HZ/20);
#endif	/* OK_TO_RESET_CONTROLLER */
}

/*
 * ide_do_reset() is the entry point to the drive/interface reset code.
 */
void
hwif_DoReset(struct ide_hwif *hwif, uint8_t unit)
{
    hwif_DoReset1(hwif, unit, false);
#ifdef CONFIG_BLK_DEV_IDETAPE
    if (hwif->drive[unit]->media == ide_tape)
	hwif->drive[unit]->tape.reset_issued=1;
#endif /* CONFIG_BLK_DEV_IDETAPE */
}


void
hwif_SelectDrive(struct ide_hwif *hwif, uint32_t unit)
{
    kprintf( KR_OSTREAM, "trying to select on interface %d", hwif->ndx );
    Put8(hwif, IDE_DRV_HD, hwif->drive[unit].select.all);
}

/* 
 * FUNCTIONS FOR THE EROS BLOCK DEVICE INTERFACE
 * 
 */

void
hwif_MountUnit(struct ide_hwif *hwif, uint8_t unit)
{
    //  assert (unit < MAX_DRIVE);
    //  assert (drive[unit].present);
    //  assert (drive[unit].needsMount);

    //  assert (drive[unit].mounted == false);

    if (hwif->drive[unit].removable) {
	//MsgLog::fatal("Removable unit locking not yet implemented\n");
	kprintf( KR_OSTREAM, "Removable unit locking not yet implemented\n");
    }
  
    hwif->drive[unit].mounted = true;
    hwif->drive[unit].needsMount = false;
    hwif->totalMountedUnits++;
}

#ifdef RMG_MAYBE
void
GetUnitInfo(uint8_t unit, BlockUnitInfo& ui)
{
    //  assert (unit < MAX_DRIVE);

    if (drive[unit].present == false) {
	ui.isDisk = false;
    }
    else {
	ui.b_geom = drive[unit].b_chs;
	ui.d_geom = drive[unit].l_chs;
	ui.nSecs = drive[unit].Capacity();
	ui.isEros = true;		/* if any partition might be EROS */
	ui.isDisk = (drive[unit].media == IDE::med_disk) ? true : false;
	ui.isMounted = drive[unit].mounted;
	ui.needsMount = drive[unit].needsMount;
	ui.hasPartitions = true;
	ui.isBoot = false;
    
	/* FIX: If we're not on a DOS machine.... */
    
	if (ioBase == BIOS_HD_BASE && ((unit | 0x80) == unit))
	    ui.isBoot = true;
    }
}
#endif

void
hwif_InsertRequest(struct ide_hwif *hwif, struct Request *req)
{
#if 0
    kprintf( KR_OSTREAM, "Request ior=0x%08x inserted on hwif=0x%08x\n",
		   req, this);
#endif
  
    //  assert (req->unit < MAX_DRIVE);
    //  assert (drive[req->unit].mounted == true);
    //hwif->drive[req->unit].rq.InsertRequest(req); replaced by:
    requestQueue_InsertRequest( &(hwif->drive[req->unit].rq), req );
    hwif_StartIO( hwif );
}

/* INTERRUPT HANDLERS: */
bool
hwif_SetMultmodeIntr( struct ide_hwif *hwif )
{
    uint8_t status = Get8(hwif, IDE_STATUS);
    if (OK_STAT(status, READY_STAT, BAD_STAT)) {
	hwif->drive[hwif->cur_drive].multCount = hwif->drive[hwif->cur_drive].multReq;
    }    
    else {
	hwif->drive[hwif->cur_drive].multCount = 0;
	hwif->drive[hwif->cur_drive].multReq = 0;
	hwif->drive[hwif->cur_drive].flags.b.recal = 1;
	drive_DumpStatus( &(hwif->drive[hwif->cur_drive]), "Multmode interrupt", status, 0 );
    }
  

#ifdef IDE_DEBUG
    kprintf( KR_OSTREAM, "SetMultMode: xfer in %d sec blocks\n",
	     hwif->drive[hwif->cur_drive].multCount );
#endif

    return true;
}

bool
hwif_RecalibrateIntr( struct ide_hwif *hwif )
{
    uint8_t status = Get8(hwif, IDE_STATUS);

    if (!OK_STAT(status,READY_STAT,BAD_STAT))
	drive_Error( &(hwif->drive[hwif->cur_drive]), "recalibrate interrupt", status);

#ifdef IDE_DEBUG
    kprintf( KR_OSTREAM, "RecalIntr(): Set int handler to 0\n");
#endif
    hwif->int_handler = 0;

    return true;
}

bool
hwif_SetGeometryIntr( struct ide_hwif *hwif )
{
    uint8_t status = Get8( hwif, IDE_STATUS );

    if (!OK_STAT(status,READY_STAT,BAD_STAT))
	drive_Error( &(hwif->drive[hwif->cur_drive]), "set geometry intr", status);

#ifdef IDE_DEBUG
    kprintf( KR_OSTREAM, "SetGeomIntr(): Set int handler to 0\n");
#endif
    hwif->int_handler = 0;
  
    return true;
}

/* When using IDE drives, we always issue a request for the total
 * desired number of sectors.  If the drive supports READ MULTIPLE we
 * use it to reduce the number of generated interrupts, else we live
 * with taking one interrupt per sector.
 */
bool
hwif_ReadIntr(struct ide_hwif *hwif)
{
    uint8_t status = Get8(hwif, IDE_STATUS);

    if (!OK_STAT(status,DATA_READY,BAD_R_STAT)) {
	drive_Error( &(hwif->drive[hwif->cur_drive]), "read intr", status);
	return true;
    }

    uint32_t max_xfer = hwif->drive[hwif->cur_drive].multCount;

    if (max_xfer == 0) max_xfer = 1;
  
    /* What follows is really the same code as in MultWrite, and it
     * isn't clear to me why it is replicated here.
     */
  
    struct Request *req = hwif->group->curReq;
  
    uint32_t nsec = req->nsec;

    if (nsec > max_xfer)
	nsec = max_xfer;
  
#ifdef IDE_DEBUG
    kprintf( KR_OSTREAM, "Transfer was 0x%08x: %d sectors at %d nsec %d\n",
		   req->req_ioaddr, req->req_nsec, req->req_start, req->nsec);

    kprintf( KR_OSTREAM, "Transferring %d sectors of data\n", nsec);
#endif
  
    drive_InputData( &(hwif->drive[hwif->cur_drive]), (void*) req->req_ioaddr, nsec * EROS_SECTOR_SIZE >> 2);
  
    req->req_start += nsec;
    req->req_ioaddr += (EROS_SECTOR_SIZE * nsec);
    req->req_nsec -= nsec;
    req->nError = 0;
    req->nsec -= nsec;

#ifdef IDE_DEBUG
    kprintf( KR_OSTREAM, "Rd Residual at 0x%08x: %d sectors at %d nsec %d\n",
		   req->req_ioaddr, req->req_nsec, req->req_start,
		   req->nsec); 
#endif

    return (req->req_nsec) ? false : true;
}

bool
hwif_WriteIntr( struct ide_hwif *hwif )
{
    uint8_t status = Get8( hwif, IDE_STATUS);

    if (!OK_STAT(status,DRIVE_READY,BAD_W_STAT)) {
	drive_Error( &(hwif->drive[hwif->cur_drive]), "write intr", status);
	return true;
    }

    struct Request *req = hwif->group->curReq;
  
    uint32_t nsec = req->nsec;
  
    req->req_start += nsec;
    req->req_ioaddr += (EROS_SECTOR_SIZE * nsec);
    req->req_nsec -= nsec;
    req->nError = 0;
    req->nsec -= nsec;

#ifdef IDE_DEBUG
    kprintf( KR_OSTREAM, "Wr Residual at 0x%08x: %d sectors at %d nsec %d\n",
		   req->req_ioaddr, req->req_nsec, req->req_start,
		   req->nsec); 
#endif

    /* If the drive is ready and there is more to do, ship off the next
     * sector, else we will have to reschedule it later.
     */
    if (req->req_nsec && (status & DRQ_STAT)) {
	//    assert( status & DRQ_STAT );

	drive_OutputData( &(hwif->drive[hwif->cur_drive]), (void*) req->req_ioaddr,
			  EROS_SECTOR_SIZE >> 2);

	return false;		/* let the interrupt handler pointer stand */
    }

    return true;
}

/* This interrupt hits when the disk is ready for the next multisector
 * block write.  If we get the block size we want (4096), we won't
 * ever be writing more than 4096 bytes, but conceivably a
 * user-initiated raw disk I/O might, so we need to be prepared for
 * that case.
 */
bool
hwif_MultWriteIntr( struct ide_hwif *hwif )
{
    uint8_t status = Get8(hwif, IDE_STATUS);

    if (!OK_STAT(status,DRIVE_READY,BAD_W_STAT)) {
	drive_Error( &(hwif->drive[hwif->cur_drive]), "multwrite intr", status);
	return true;
    }

    struct Request *req = hwif->group->curReq;
  
    uint32_t nsec = req->nsec;
  
    req->req_start += nsec;
    req->req_ioaddr += (EROS_SECTOR_SIZE * nsec);
    req->req_nsec -= nsec;
    req->nError = 0;
    req->nsec -= nsec;

#ifdef IDE_DEBUG
    kprintf( KR_OSTREAM, "MWr Residual at 0x%08x: %d sectors at %d nsec %d\n",
		   req->req_ioaddr, req->req_nsec, req->req_start,
		   req->nsec); 
#endif
  
    /* If the drive is ready and there is more to do, ship off the next
     * block, else we will have to reschedule it later.
     */
    if (req->req_nsec && (status & DRQ_STAT)) {
	//    assert( status & DRQ_STAT );

	drive_MultWrite( &(hwif->drive[hwif->cur_drive]), req );

	return false;		/* let the interrupt handler pointer stand */
    }

    return true;
}

void
hwif_OnIntr(struct ide_hwif *hwif, struct IntAction* ia)
{
    struct ide_hwif *pHwif = NULL; //  (struct ide_hwif*) ia->drvr_ptr; NEED TO FIX RMG

    if (pHwif->int_handler == 0) {
	uint8_t status = Get8( pHwif, IDE_STATUS);
	drive_DumpStatus( &(pHwif->drive[0]), "orphaned interrupt", status, 0);
#ifdef OLD
	pHwif->drive[0].flags.b.recal = 1;
	pHwif->drive[1].flags.b.recal = 1;
	halt();
#endif
    }

    /* Call pointer to member that happens to be member of pHwif */
    // if ( (pHwif->*(pHwif->int_handler))() ) {  // HUH.. OLD CALL.  
    if (pHwif->int_handler()) {
#ifdef IDE_DEBUG
	kprintf( KR_OSTREAM, "OnIntr() Set int handler to 0\n");
#endif
	pHwif->int_handler = 0;
	pHwif->next = pHwif->readyChain;
	pHwif->readyChain = pHwif;

	// ActivateTask(); DUNNO WHAT TO DO HERE YET
    }
}

void
hwif_StartIO( struct ide_hwif *hwif )
{
    group_StartIO( hwif->group );
}

/* FIX: This needs a much more sophisticated selection policy.  Right
 * now it does round-robin issuance among all of the HWIF's in a group
 * to prevent starvation. On drives with write-back buffers this isn't
 * a big problem, but most drives still use write-through logic, and
 * on these drives the current policy results in bad rotational
 * delays.  At 7200 RPM, the on-paper mean rotational delay is
 * 60/(7200*2) or about 4.2 ms.  Since we write blocks sequentially,
 * it is damn likely that we will get close to worst case rotational
 * delay of 8ms (I need to measure this).
 * 
 * A better policy would be to have the StartIO() routine examine the
 * next request on the same drive and see if that request falls on the
 * same PHYSICAL cylinder **and track** as the previous request.  If
 * so, it should endeavour to issue the request eagerly.
 * 
 * There is no point doing more than a track on a modern drive -- head
 * to head delays closely approximate track to track delays, and we
 * might as well switch to the next interface.
 * 
 * Even better would be to do request merging, coalescing adjacent I/O
 * operations into a single operation using chained DMA.  This would
 * make maximal use of the disk cache and the rotational advantage.
 */

void
group_StartIO( struct ide_group *group )
{
    //  assert(this);
  
    do {
	if (group->curReq == 0) {
	    uint32_t i;
	    for ( i = 0; i < HWIFS_PER_GROUP; i++ ) {
		uint32_t which_hwif = (group->cur_hwif + i + 1) % HWIFS_PER_GROUP;

		if (group->hwif[which_hwif] == 0)
		    continue;

		//curReq = hwif[which_hwif]->GetNextRequest();
		group->curReq = hwif_GetNextRequest( group->hwif[which_hwif] );
		if (group->curReq) {
		    group->cur_hwif = which_hwif;
		    break;
		}
	    }
	}

	if (group->curReq == 0)
	    return;
  
#if defined(IDE_DEBUG)
	kprintf("Start cur_hwif = 0x%08x, req=0x%08x dio=0x%08x\n",
		group->hwif[ group->cur_hwif ], group->curReq, group->curReq->dio);
#endif
  
    } while ( hwif_StartRequest( group->hwif[group->cur_hwif], group->curReq ) );
}

struct Request*
hwif_GetNextRequest( struct ide_hwif *hwif)
{
    /* The point of this madness is to cycle through the configured
     * drives exactly once, while still doing round-robin initiation.
     */
    uint32_t i;
    
    
    for (i = 0; i < MAX_DRIVE; i++) {
	uint32_t unit = (hwif->cur_drive + i + 1) % MAX_DRIVE;
	
	if ( hwif->drive[unit].present && !requestQueue_IsEmpty( &(hwif->drive[unit].rq) ) ) {
	    hwif->cur_drive = unit;
	    return requestQueue_GetNextRequest( &( hwif->drive[unit].rq) );
	}
    }
    
    return 0;
}

/* Return true when request is completed. */
bool
hwif_StartRequest(struct ide_hwif *hwif, struct Request* req)
{
    /* We don't do split-seek on IDE drives, so just go ahead and commit
     * the request.
     */
    //  assert(this);
    //  assert(req);

#ifdef RMG_MAYBE  
    if (hwif->inDuplexWait) // is this part of BlockDev RMG
	return false;
#endif   
    if ( hwif->int_handler ) {
#if defined(IDE_DEBUG)
	kprintf( KR_OSTREAM, "HWIF already active (handler=0x%08x)\n", &int_handler);
#endif
	return false;
    }
  
    //    if ( ! req->Commit(this, &drive[req->unit]) ) {
    //	/* stalled by another controller */
    //#if defined(IDE_DEBUG)
    //	kprintf( KR_OSTREAM, "Stalled by another controller\n");
    //#endif
    //	return false;
    //    }

    if ( request_IsCompleted( req ) ) {
	struct RequestQueue *rq = &(hwif->drive[req->unit].rq);
    
	//    assert (req == group->curReq);
	    request_Finish( req );
	//    assert ( rq.ContainsRequest(req) );
	requestQueue_RemoveRequest( rq, req );
	hwif->group->curReq = 0;

#if defined(IDE_DEBUG)
	if (req->cmd == Plug)
	    kprintf( KR_OSTREAM, "IDE%d: plug passes\n", req->unit);
	kprintf( KR_OSTREAM, "Finish current request. More? %c\n",
		 rq.IsEmpty() ? 'n' : 'y');
#endif
	return true;
    }
  
#ifdef IDE_DEBUG
    kprintf( KR_OSTREAM, "Calling DoRequest w/ req=0x%08x...\n", req);
#endif
    drive_DoRequest( &(hwif->drive[req->unit]), req );
    return false;
}

void
hwif_Probe( struct ide_hwif *hwif )
{
    uint32_t unit;

    if ( hwif->noprobe ) {
	kprintf( KR_OSTREAM, "Skipping probe on IDE interface %d\n", hwif->ndx);
	return;
    }

    if (hwif->ioBase == BIOS_HD_BASE) {

	kprintf( KR_OSTREAM, "********************************************************");
	kprintf( KR_OSTREAM, "********************************************************");
	kprintf( KR_OSTREAM, "********************************************************");
	kprintf( KR_OSTREAM, "********************************************************");

	/* Check the CMOS configuration settings.  If a CMOS configuration
	 * is found, then this is either BIOS drive 0x80 or 0x81, and the
	 * interface is register compatible with the prehistoric register
	 * interface.  This implies that the drive in question is NOT, for
	 * example, a OPTION_SCSI drive.
	 * 
	 * FIX: On newer BIOS, CMOS appears to store info for four drives.
	 * How to get it if present?
	 */

#ifdef RMG
	uint8_t cmos_hds = CMOS::cmosByte(0x12);
	if (cmos_hds)
	    hwif->present = true;		/* HW interface present */
	
	uint8_t cmos_hd0 = (cmos_hds >> 4) & 0xfu;
	uint8_t cmos_hd1 = cmos_hds & 0xfu;
	
	/* If the drives are listed in the CMOS, extract their geometry
	 * from the BIOS tables.
	 */
	if (cmos_hd0) {
	    /* 
	     * form a longword representing all this gunk:
	     *       6 bit zero
	     *	10 bit cylinder
	     *	 8 bit head
	     *	 8 bit sector
	     */

	    hwif->drive[0].b_chs.cyl = SysConfig.driveGeom[0].cyls;
	    hwif->drive[0].b_chs.hd  = SysConfig.driveGeom[0].heads;
	    hwif->drive[0].b_chs.sec = SysConfig.driveGeom[0].secs;

	    hwif->drive[0].l_chs = hwif->drive[0].b_chs;

	    kprintf( KR_OSTREAM, "ide0: c/h/s=%d/%d/%d\n",
		     drive[0].b_chs.cyl,
		     drive[0].b_chs.hd,
		     drive[0].b_chs.sec);
      
	    drive[0].present = true;
	}
	if (cmos_hd1) {
	    hwif->drive[1].b_chs.cyl = SysConfig.driveGeom[1].cyls;
	    hwif->drive[1].b_chs.hd = SysConfig.driveGeom[1].heads;
	    hwif->drive[1].b_chs.sec = SysConfig.driveGeom[1].secs;

	    hwif->drive[1].l_chs = hwif->drive[1].b_chs;
	    kprintf( KR_OSTREAM, "ide1: c/h/s=%d/%d/%d\n",
		     hwif->drive[1].b_chs.cyl,
		     hwif->drive[1].b_chs.hd,
		     hwif->drive[1].b_chs.sec );
      
	    hwif->drive[1].present = true;
	}
#endif
    }



#ifdef RMG
    if ( IoRegion::IsAvailable( hwif->ioBase, 8 ) == false ||
	 IoRegion::IsAvailable( hwif->ioBase + IDE_CTL_ALTSTATUS, 2 ) == false ) {

	for ( unit = 0; unit < MAX_DRIVE; unit++ ) {
	    if ( hwif->drive[ unit ].present )
		kprintf( KR_OSTREAM, "IDE: hwif %d drive %d present but ports in use\n",
			 hwif->ndx, unit);
	}

	kprintf( KR_OSTREAM, "IDE hwif %d ports in use - skipping probe\n",
		 ndx);
	hwif->present = false;

	return;
    }
#endif
    // 1 -> MAX_DRIVE
    for ( unit = 0; unit < 1; unit++ ) {
	drive_Probe( &(hwif->drive[unit]) );
	
	if (hwif->drive[unit].present && !hwif->present) {
	    hwif->present = true;
#ifdef RMG
	    IoRegion::Allocate(ioBase, 8, "ide");
	    IoRegion::Allocate(ioBase + IDE_CTL_ALTSTATUS, 2, "ide");
#endif
	}
    }

    if (!hwif->present)
	return;

    if (!hwif->irq)
	hwif->irq = ide_irq[hwif->ndx];
    
    // can put in the invoation to get an interrupt going here
    kprintf( KR_OSTREAM, "Registering IDE driver (this=0x%08x)...\n", hwif);
    kprintf( KR_OSTREAM, "Registering IDE driver (this=0x%08x)...\n", hwif);

    //    Register();

    if (!hwif->irq) {
	kprintf( KR_OSTREAM, "ide%d disabled: no IRQ\n", hwif->ndx);
	kprintf( KR_OSTREAM, "ide%d disabled: no IRQ\n", hwif->ndx);
	/* FIX: should unregister */
	return;
    }

    {
	uint8_t result=Get8(hwif, IDE_CTL_ALTSTATUS);
	kprintf( KR_OSTREAM, "Before int register. Alt status : 0x%02x\n", result);
    }

    //  IntAction *ia = new IntAction(hwif->irq, ide_name[ndx], ide_hwif::OnIntr);
    //ia->drvr_ptr = this;
  
    //IRQ::WireExclusive(ia);

    kprintf( KR_OSTREAM, "Check pending interrupt: IRQ %d\n", hwif->irq);

    //IRQ::Enable(hwif->irq);
#if 0
    kprintf( KR_OSTREAM, "Intentional halt...\n");
    halt();
#endif
}


bool
Probe()
{
    int ctrlr = 0;

    // BLACKLIST CAN WAIT InitChipsets();
    // MAX_HWIF -> 1;
    for (ctrlr = 0; ctrlr < 1; ctrlr++)
	hwif_Probe( & ( hwif_tbl[ ctrlr ] ) );
#ifdef OLD
    /* assume first drive is a 1.44 */
    TheHardDiskCtrlr.unit[0]->mediaInfo  = &MediaParams[4];

    for(int i= 1; i <= HardDiskCtrlr::NUNITS;i++ )
	TheHardDiskCtrlr.unit[i] = 0; /* assume the other drives do */
    /* not exist */
  
    TheHardDiskCtrlr.Wakeup();
#endif
    return true;
}

#ifdef RMG
static bool
Attach()
{
  return true;
}
#endif

int 
ProcessRequest( Message *msg ) 
{

    //char buf[2000];
    msg->snd_len = 0;
    msg->snd_key0 = KR_VOID;
    msg->snd_key1 = KR_VOID;
    msg->snd_key2 = KR_VOID;
    msg->snd_rsmkey = KR_RETURN;
    msg->snd_code = RC_OK;
    msg->snd_w1 = 0;
    msg->snd_w2 = 0;
    msg->snd_w3 = 0;
    msg->snd_invKey = KR_VOID;

    switch (msg->rcv_code) {
    
    case OC_GIVEKEY:
	{
	  
	    return 1;
	}
      
    case READ:
	//kprintf( KR_OSTREAM, "DRIVE: READ" );
	//	doIo( msg );
	msg->snd_rsmkey = KR_VOID;
	return 1;
	
    case WRITE:
	//kprintf( KR_OSTREAM, "DRIVE: WRITE" );
	//	doIo( msg );
	return 1;

    case OC_INTERRUPT:
	return 1;

    default:  {}//  kprintf( KR_OSTREAM, "DRIVE: UNKNOWN SENDCODE" );
  
    }

    msg->snd_code = RC_eros_key_UnknownRequest;

    return 1;
}




void init_caps()
{

    OKAY( node_copy( KR_CONSTIT, KC_OSTREAM, KR_OSTREAM ), 
	  "drive.c: node_copy: KT_OSTREAM" );

    OKAY( node_copy( KR_CONSTIT, KC_SLEEP, KR_SLEEP ),
	  "drive.c: node_copy: KR_SLEEP" );

    OKAY( node_copy( KR_CONSTIT, KC_DEVICEPRIVS, KR_DEVICEPRIVS),
	  "drive.c: node_copy: KR_DEVICEPRIVS" );

    OKAY( process_swap( KR_SELF, ProcIoSpace, KR_DEVICEPRIVS, KR_VOID ), 
	  "drive.c: node_copy: KR_DEVICEPRIVS\n" ); 

    OKAY( process_make_start_key( KR_SELF, 0, KR_START ), 
	  "drive.c: node_copy: KR_START\n" );
}

void init_data()
{
    int32_t i;

    for ( i = 0; i < MAX_HWIF; i++ ){
	init_ide_group( &group_tbl[ i ] );
    }

    for ( i = 0; i < MAX_HWIF; i++ ) {
	init_ide_hwif( &hwif_tbl[ i ] );
    }
}



void init() {


    init_caps();
    kprintf( KR_OSTREAM, "Caps inited" );
    init_data();
    kprintf( KR_OSTREAM, "Data structures inited" );
    Probe();
    //start_interrupt();
}


int
main(void)
{
    Message msg;
  
    init();
    kprintf(KR_OSTREAM,
    	    "STARTING DRIVE ... [SUCCESS]");

      
    msg.snd_invKey = KR_RETURN;
    msg.snd_key0 = KR_START;
    msg.snd_key1 = KR_VOID;
    msg.snd_key2 = KR_VOID;
    msg.snd_rsmkey = KR_RETURN;
    msg.snd_data = 0;
    msg.snd_len = 0;
    msg.snd_code = 0;
    msg.snd_w1 = 0;
    msg.snd_w2 = 0;
    msg.snd_w3 = 0;

    msg.rcv_key0 = KR_IDECALL;
    msg.rcv_key1 = KR_VOID;
    msg.rcv_key2 = KR_VOID;
    msg.rcv_rsmkey = KR_RETURN;
    msg.rcv_data = 0;
    msg.rcv_len = 0;
    msg.rcv_code = 0;
    msg.rcv_w1 = 0;
    msg.rcv_w2 = 0;
    msg.rcv_w3 = 0;

    process_make_start_key(KR_SELF, 0, KR_START);

    do 
	{	    
	    RETURN(&msg);
	    //	    kprintf( KR_OSTREAM, "DRIVE: received request" );
	    msg.snd_invKey = KR_RETURN;
	    msg.snd_key0 = KR_VOID;
	    msg.snd_rsmkey = KR_RETURN;
	}
    while(ProcessRequest(&msg));
  
    return 0;
}


void request_Finish( struct Request *req) {}
bool request_IsCompleted( struct Request *req ) { return true; }

struct Request *requestQueue_GetNextRequest( struct RequestQueue *rq ) { return NULL; }
void requestQueue_InsertRequest( struct RequestQueue *rq, struct Request *req) {}
bool requestQueue_IsEmpty( struct RequestQueue *rq ) { return true; };
void requestQueue_RemoveRequest( struct RequestQueue *rq, struct Request *req ) {}


void
init_ide_group( struct ide_group *g)
{
      g->hwif[0] = 0;
      g->hwif[1] = 0;
      g->cur_hwif = 0;
      g->curReq = 0;
}
