/*
 * Copyright (C) 1998, 1999, Jonathan S. Shapiro.
 * Copyright (C) 2005, Strawberry Development Group
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

/* TODO: This interface won't work with older ST506 or ESDI drives.
 * At the moment I have no plans to support such drives.  Ever.  I'm
 * probably the only person on the face of the planet who still owns
 * any that work :-).  Even the LINUX crew are now compiling those
 * drives out by default.
 */

/* #define VERBOSE_BUG */
#if 0
#include <kerninc/kernel.hxx>
#include <kerninc/MsgLog.hxx>
#include <kerninc/AutoConf.hxx>
#include <kerninc/IoRegion.hxx>
#include <kerninc/IRQ.hxx>
#include <kerninc/Machine.hxx>
#include <kerninc/SysTimer.hxx>
#include <kerninc/BlockDev.hxx>
#include <kerninc/IoRequest.hxx>
#ifdef VERBOSE_BUG
#include <arch/i486/kernel/lostart.hxx>
#endif
#include <arch/i486/kernel/SysConfig.hxx>
#include <arch/i486/kernel/CMOS.hxx>


#include "io.h"
#endif

#include "ide_ide.h"
#include "ide_drive.h"
#include "ide_hwif.h"
#include "ide_group.h"
//#include "IoRequest.h"

	      
#define dbg_probe	0x1	/* steps in taking snapshot */

/* Following should be an OR of some of the above */
#define dbg_flags   ( 0u )

#define DBCOND(x) (dbg_##x & dbg_flags)
#define DEBUG(x) if DBCOND(x)
#define DEBUG2(x,y) if ((dbg_##x|dbg_##y) & dbg_flags)

/* #define IDE_DEBUG */

#define INITIAL_MULT_COUNT 8	/* 4K per xfer max */
#define IS_PROMISE_DRIVE 0	/* for now */

#define FANCY_STATUS_DUMPS

/*
 * Probably not wise to fiddle with these
 */
#define ERROR_MAX	8	/* Max read/write errors per sector */
#define ERROR_RESET	3	/* Reset controller every 4th retry */
#define ERROR_RECAL	1	/* Recalibrate every 2nd retry */

#define DRIVE_SELECT_DELAY 10	/* in MILLISECONDS */
#define DRIVE_DETECT_DELAY 50


void bits_print( char *str, uint8_t v )
{

    kprintf( KR_OSTREAM, "%s: %d%d%d%d%d%d%d%d", str,
	     ( v & 0x80 ) ? 1 : 0, ( v & 0x40 ) ? 1 : 0,
	     ( v & 0x20 ) ? 1 : 0, ( v & 0x10 ) ? 1 : 0,
	     ( v & 0x08 ) ? 1 : 0, ( v & 0x04 ) ? 1 : 0,
	     ( v & 0x02 ) ? 1 : 0, ( v & 0x01 ) ? 1 : 0 );
}

void
init_ide_drive(struct ide_drive *drive)
{
    drive->present = false;
    drive->mounted = false;
    drive->needsMount = false;
    drive->removable = false;
    drive->media = med_disk;
    drive->ctl = 0x08;
#if 0
    drive->p_chs.heads = 0;
    drive-?p_chs.secs = 0;
    drive->p_chs.cyls = 0;
#endif
    drive->l_chs.hd = 0;
    drive->l_chs.sec = 0;
    drive->l_chs.cyl = 0;
    drive->b_chs.hd = 0;
    drive->b_chs.sec = 0;
    drive->b_chs.cyl = 0;
    drive->select.all = 0xa0; // 0xa0 is the docuemented corect answer here.  is VMWARE @#$#^%# up?
    drive->flags.all = 0;
    drive->flags.b.recal = 1; /* seek to track 0 */
    drive->flags.b.setGeom = 1; /* tell it its geometry */
    //drive->id = 0;
    drive->name[0] = 'h';
    drive->name[1] = 'd';
    drive->name[2] = '?';
    drive->name[3] = '?';
    drive->name[4] = 0;
}

/*
 * lba_capacity_is_ok() performs a sanity check on the claimed "lba_capacity"
 * value for this drive (from its reported identification information).
 *
 * Returns:	true if lba_capacity looks sensible
 *		false otherwise
 */
bool CheckLbaCapacity(struct ide_driveid *id)
{
  uint32_t lba_sects   = id->lba_capacity;
  uint32_t chs_sects   = id->cyls * id->heads * id->sectors;
  uint32_t _10_percent = chs_sects / 10;

  /* perform a rough sanity check on lba_sects:  within 10% is "okay" */
  if ((lba_sects - chs_sects) < _10_percent)
    return true;		/* lba_capacity is good */

  /* some drives have the word order reversed */
  lba_sects = (lba_sects << 16) | (lba_sects >> 16);
  if ((lba_sects - chs_sects) < _10_percent) {
    id->lba_capacity = lba_sects;	/* fix it */
    return true;		/* lba_capacity is (now) good */
  }

  return false;			/* lba_capacity value is bad */
}

/*
 * current_capacity() returns the capacity (in sectors) of a drive
 * according to its current geometry/LBA settings.
 *///ide_drive
uint32_t
drive_Capacity(struct ide_drive *drive)
{
  struct ide_driveid *drvid = ( struct ide_driveid*) &(drive->id);
#if 0
  kprintf( KR_OSTREAM, "l_chs: %d/%d/%d\n", drive->l_chs.cyl, drive->l_chs.hd, drive->l_chs.sec);
#endif
  
  uint32_t capacity = drive->l_chs.cyl * drive->l_chs.hd * drive->l_chs.sec;

  if (!drive->present)
    return 0;
  
  if (drive->media != med_disk)
    return UINT32_MAX;	/* cdrom or tape */

  if (!IS_PROMISE_DRIVE) {
    drive->select.b.lba = 0;
    /* Determine capacity, and use LBA if the drive properly supports it */
    if (drvid != NULL && (drvid->capability & 2)
	&& CheckLbaCapacity(drvid)) {

      if (drvid->lba_capacity >= capacity) {
	capacity = drvid->lba_capacity;
	drive->select.b.lba = 1;
      }
    }
  }

  return capacity;
#if 0
  return (capacity - drive->sect0);
#endif
}
/////////////////////////// SO FAR GOOD /////////////////////////////////
/*
 * This is used for most PIO data transfers *from* the IDE interface
 */
void
drive_InputData(struct ide_drive *drive, void *buffer, unsigned int wcount)
{
  uint16_t io_base  = drive->hwif->ioBase;
  uint16_t data_reg = io_base+IDE_DATA;

#if 0
  uint8_t io_32bit = drive->io_32bit;

  if (io_32bit) {
#if SUPPORT_VLB_SYNC
    if (io_32bit & 2) {
      cli();
      do_vlb_sync(drive->io_base+IDE_NSECS);
      insw(data_reg, buffer, wcount);
      if (drive->unmask)
	sti();
    } else
#endif /* SUPPORT_VLB_SYNC */
      insw(data_reg, buffer, wcount);
  } else
#endif
      //ins16(data_reg, buffer, wcount<<1);
      insw(data_reg, buffer, wcount<<1);
}
/////////////////////////////////////////////////////////////////////////////////////
/*
 * ide_multwrite() transfer sectors to the drive, being careful not to
 * overrun the drive's multCount limit.
 */
void
drive_MultWrite (struct ide_drive *drive, struct Request *req)
{
  uint32_t nsec = req->req_nsec;

  /* Can't do more than the block size limit: */
  if (nsec > drive->multCount)
      nsec = drive->multCount;

  drive_OutputData(drive, (void *) req->req_ioaddr, nsec * EROS_SECTOR_SIZE >> 2);

  req->nsec = nsec;
}
////////////////////////////////////////////////////////
/*
 * This is used for most PIO data transfers *from* the IDE interface
 */
void
drive_OutputData(struct ide_drive *drive, void *buffer, unsigned int wcount)
{
  uint16_t io_base  = drive->hwif->ioBase;
  uint16_t data_reg = io_base+IDE_DATA;

#if 0
  uint8_t io_32bit = drive->io_32bit;

  if (io_32bit) {
#if SUPPORT_VLB_SYNC
    if (io_32bit & 2) {
      cli();
      do_vlb_sync(drive->io_base+IDE_NSECS);
      insw(data_reg, buffer, wcount);
      if (drive->unmask)
	sti();
    } else
#endif /* SUPPORT_VLB_SYNC */
      insw(data_reg, buffer, wcount);
  } else
#endif

      //RMG outs16(data_reg, buffer, wcount<<1);
      outsw(data_reg, buffer, wcount<<1);
}
//////////////////////////////////////////////////////////////

void
ide_fixstring (uint8_t *s, const int bytecount, const int byteswap)
{
  uint8_t *p = s, *end = &s[bytecount & ~1]; /* bytecount must be even */

#ifdef RMG
  if (byteswap) {
    /* convert from big-endian to host byte order */
    for (p = end ; p != s;) {
      unsigned short *pp = (unsigned short *) (p -= 2);
      *pp = Machine::ntohhw(*pp);
    }
  }
#endif
  /* strip leading blanks */
  while (s != end && *s == ' ')
    ++s;

  /* compress internal blanks and strip trailing blanks */
  while (s != end && *s) {
    if (*s++ != ' ' || (s != end && *s && *s != ' '))
      *p++ = *(s-1);
  }

  /* wipe out trailing garbage */
  while (p != end)
    *p++ = '\0';
}

void
drive_AtapiIdentify(struct ide_drive *drive, uint8_t cmd)
{
  int bswap;
  /*  uint32_t capacity, check; */

  //id = (ide_driveid *) new (0) char[EROS_SECTOR_SIZE];
  
  drive_InputData(drive, &(drive->id), EROS_SECTOR_SIZE >> 2); /* read 512 bytes of id info */

  kprintf( KR_OSTREAM, "Got atapi data.\n");
  /*
   * EATA OPTION_SCSI controllers do a hardware ATA emulation:  ignore them
   */
  if ((drive->id.model[0] == 'P' && drive->id.model[1] == 'M')
      || (drive->id.model[0] == 'S' && drive->id.model[1] == 'K')) {
      kprintf( KR_OSTREAM, "%s: EATA OPTION_SCSI HBA %40s\n", drive->name, drive->id.model);
    drive->present = 0;
    return;
  }

  /*
   *  WIN_IDENTIFY returns little-endian info,
   *  WIN_PIDENTIFY *usually* returns little-endian info.
   */
  bswap = 1;
  if (cmd == WIN_PIDENTIFY) {
    if ((drive->id.model[0] == 'N' && drive->id.model[1] == 'E') /* NEC */
	|| (drive->id.model[0] == 'F' && drive->id.model[1] == 'X') /* Mitsumi */
	|| (drive->id.model[0] == 'P' && drive->id.model[1] == 'i'))/* Pioneer */
      bswap = 0;	/* Vertos drives may still be weird */
  }
  ide_fixstring (drive->id.model,     sizeof(drive->id.model),     bswap);
  ide_fixstring (drive->id.fw_rev,    sizeof(drive->id.fw_rev),    bswap);
  ide_fixstring (drive->id.serial_no, sizeof(drive->id.serial_no), bswap);

  /*
   * Check for an ATAPI device
   */
  if (cmd == WIN_PIDENTIFY) {
    uint8_t type = (drive->id.config >> 8) & 0x1f;
    kprintf( KR_OSTREAM, "%s: %s, ATAPI ", drive->name, drive->id.model);
    switch (type) {
    case 0:		/* Early cdrom models used zero */
    case 5:
	kprintf ( KR_OSTREAM, "CDROM drive\n");
      drive->media = med_cdrom;
      drive->present = 0;		/* TEMPORARY */
      drive->removable = 1;
      return;
    case 1:
#ifdef IDE_TAPE
	kprintf ( KR_OSTREAM, "TAPE drive");
      if (idetape_identify_device (drive,id)) {
	drive->media = ide_tape;
	drive->present = 1;
	drive->removeable = 1;
	if (drive->hwif->dmaproc != NULL &&
	    !drive->hwif->dmaproc(ide_dma_check, drive)) // unsure where this ..., drive)) came from
	    kprintf(KR_OSTREAM, ", DMA");
	    //MsgLog::printf("\n");
      }
      else {
	  drive->present = 0;
	  kprintf ( KR_OSTREAM, "ide-tape: the tape is not supported by this version of the driver\n");
      }
      return;
#else
      kprintf ( KR_OSTREAM, "TAPE ");
      break;
#endif /* CONFIG_BLK_DEV_IDETAPE */
    default:
	drive->present = 0;
	kprintf( KR_OSTREAM, "Type %d - Unknown device\n", type);
      return;
    }
    drive->present = 0;
    kprintf( KR_OSTREAM, "- not supported by this kernel\n");
    return;
  }

  /* check for removeable disks (eg. SYQUEST), ignore 'WD' drives */
  if (drive->id.config & (1<<7)) {	/* removeable disk ? */
    if (drive->id.model[0] != 'W' || drive->id.model[1] != 'D')
      drive->removable = 1;
  }

  /* SunDisk drives: treat as non-removeable, force one unit */
  if (drive->id.model[0] == 'S' && drive->id.model[1] == 'u') {
    drive->removable = 0;
    if (drive->select.b.unit) {
      drive->present = 0;
      return;
    }
  }
	
  drive->media = med_disk;
  /* Extract geometry if we did not already have one for the drive */
  if (!drive->present) 
      {
	  drive->present = 1;
	  drive->l_chs.cyl  = drive->b_chs.cyl  = drive->id.cyls;
	  drive->l_chs.hd   = drive->b_chs.hd   = drive->id.heads;
	  drive->l_chs.sec  = drive->b_chs.sec  = drive->id.sectors;
      }

  /* Handle logical geometry translation by the drive */
  if ( ( drive->id.field_valid & 1 ) && 
       ( drive->id.cur_cyls ) && 
       ( drive->id.cur_heads ) && 
       ( drive->id.cur_heads <= 16 ) && 
       ( drive->id.cur_sectors ) )
      {
      /*
       * Extract the physical drive geometry for our use.
       * Note that we purposely do *not* update the bios info.
       * This way, programs that use it (like fdisk) will
       * still have the same logical view as the BIOS does,
       * which keeps the partition table from being screwed.
       *
       * An exception to this is the cylinder count,
       * which we reexamine later on to correct for 1024 limitations.
       */
      drive->l_chs.cyl  = drive->id.cur_cyls;
      drive->l_chs.hd   = drive->id.cur_heads;
      drive->l_chs.sec  = drive->id.cur_sectors;

      /* check for word-swapped "capacity" field in id information */
      uint32_t capacity = drive->l_chs.cyl * drive->l_chs.hd * drive->l_chs.sec;
      uint32_t check = (drive->id.cur_capacity0 << 16) | drive->id.cur_capacity1;
      if (check == capacity) {	/* was it swapped? */
	/* yes, bring it into little-endian order: */
	drive->id.cur_capacity0 = (capacity >>  0) & 0xffff;
	drive->id.cur_capacity1 = (capacity >> 16) & 0xffff;
      }
    }

  /* Use physical geometry if what we have still makes no sense */
  if ((!drive->l_chs.hd || drive->l_chs.hd > 16) && drive->id.heads && drive->id.heads <= 16) {
    drive->l_chs.cyl  = drive->id.cyls;
    drive->l_chs.hd   = drive->id.heads;
    drive->l_chs.sec  = drive->id.sectors;
  }
  /* Correct the number of cyls if the bios value is too small */
  if (drive->l_chs.sec == drive->b_chs.sec && drive->l_chs.hd == drive->b_chs.hd) {
    if (drive->l_chs.cyl > drive->b_chs.cyl)
      drive->b_chs.cyl = drive->l_chs.cyl;
  }
  ///////////////////////////////////////////////////////////////
  (void) drive_Capacity( drive );
  
  kprintf( KR_OSTREAM, "%s: %40s, %dMB w/%dkB Cache, %sCHS=%d/%d/%d",
	   drive->name, drive->id.model, drive_Capacity( drive )/2048L,
	   drive->id.buf_size/2,
	   drive->select.b.lba ? "LBA, " : "",
	   drive->b_chs.cyl, drive->b_chs.hd, drive->b_chs.sec);
  
  drive->multCount = 0;
  if (drive->id.max_multsect) 
      {
	  drive->multReq = INITIAL_MULT_COUNT;
	  if (drive->multReq > drive->id.max_multsect)
	      {
		  drive->multReq = drive->id.max_multsect;
	      }
	  if (drive->multReq || ((drive->id.multsect_valid & 1) && drive->id.multsect)) 
	      {
		  drive->flags.b.setMultMode = 1;
	      }
      }

  if ( drive->hwif->dma_handler != 0 ) {	/* hwif supports DMA? */
    if ( !( drive->hwif->dma_handler( dma_check, drive ) ) )
	{
	    kprintf( KR_OSTREAM, ", DMA");
	}
  }

  //MsgLog::printf("\n");
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////

uint8_t
drive_Identify(struct ide_drive *drive, uint8_t cmd)
{
    uint32_t irqs = 0;
    uint16_t hd_status = IDE_CTL_ALTSTATUS; /* non-intrusive preferred */
    uint8_t rc = 1;
    
    kprintf( KR_OSTREAM, "drive_identify begin irq = %d", drive->hwif->irq );

    if (drive->hwif->irq == 0) {
	//IRQ::EndProbe(IRQ::BeginProbe()); /* clear pending interrupts */
	//irqs = IRQ::BeginProbe();	/* wait for interrupt */
	kprintf( KR_OSTREAM, " enable device IRQ " );
	PUT8(IDE_CTL_ALTSTATUS, drive->ctl); /* enable device IRQ */
    }
    
    capros_Sleep_sleep( KR_SLEEP, DRIVE_DETECT_DELAY );
    
    
    if ((GET8(IDE_CTL_ALTSTATUS) ^
	 GET8(IDE_STATUS)) &
	~INDEX_STAT) {
	kprintf( KR_OSTREAM, "%s: probing with STATUS instead of ALTSTATUS\n", drive->name);
	hd_status = IDE_STATUS;	/* use intrusive polling */
    }
    
    kprintf( KR_OSTREAM, " send command: 0x%04x", cmd );
    PUT8(IDE_CMD, cmd);
    
    uint64_t waitTill = ((cmd == WIN_IDENTIFY) ? WAIT_WORSTCASE : WAIT_PIDENTIFY);
    uint64_t haveWaited = 0;

    do {
	if ( haveWaited > waitTill )
	    {
		//if ( drive->hwif->irq == 0)
		//(void) IRQ::EndProbe(irqs);
		kprintf( KR_OSTREAM, "Drive timed out.\n");
		return 1;	/* drive timed-out */
	    }
	capros_Sleep_sleep( KR_SLEEP, DRIVE_DETECT_DELAY );
	haveWaited += DRIVE_DETECT_DELAY;

    } while (GET8(hd_status) & BUSY_STAT);

    //    Machine::SpinWaitMs(DRIVE_DETECT_DELAY);	/* wait for IRQ and DRQ_STAT */
    capros_Sleep_sleep( KR_SLEEP, DRIVE_DETECT_DELAY );
    uint8_t status = GET8(IDE_STATUS);
    bits_print( "identify status", status  );
    if ( OK_STAT( status,DRQ_STAT,BAD_R_STAT ) ) {
	kprintf( KR_OSTREAM, "Drive returned ID.  Fetching ATAPI info..." );
	/* Drive returned an ID.  Do an ATAPI identify: */
	//drive_AtapiIdentify( drive, cmd );

#if 0
	if ( drive->present && drive->media != med_tape ) {
	    /* tune the PIO mode... the BIOS ought to do this these days. */
	}
#endif

	rc = 0;			/* drive responded with ID */
    } else {
	kprintf( KR_OSTREAM, "Drive refused ID.\n");
	rc = 2;			/* drive refused ID */
    }

    if ( drive->hwif->irq == 0 ) {
	//	irqs = IRQ::EndProbe(irqs);	/* get irq number */

	if (irqs > 0)
	    drive->hwif->irq = irqs;
	else				/* Mmmm.. multiple IRQs */
	    {
		kprintf( KR_OSTREAM, "%s: IRQ probe failed (%d)\n", drive->name, irqs);
	    }
    }

    //DEBUG(probe)
    //    kprintf( KR_OSTREAM, "Pausing after drive probe...\n");

    return rc;
}

//??????????????????????????????????????????????????????????????????????????????????????????




void
Put8(struct ide_hwif *hwif, uint16_t reg, uint8_t value)
{

    kprintf( KR_OSTREAM, "Writing byte 0x%02x to hwifno %d ioBase 0x%04x reg 0x%04x\n",
	     value, hwif->ndx, hwif->ioBase, reg );

  //RMG  old_outb(reg + ioBase, value);
  outb( reg + hwif->ioBase, value );
}




uint32_t
drive_DoProbe(struct ide_drive *drive, uint8_t cmd)
{
    uint8_t ret;

  if (drive->present && (drive->media != med_disk) && (cmd == WIN_IDENTIFY)) {
      //MsgLog::fatal("Drive obviously not IDE\n");
      kprintf( KR_OSTREAM, "Drive obviously not IDE" );
    return 4;
  }

  kprintf( KR_OSTREAM, "probing for %s: present=%d, media=%d, probetype=%s hwif=0x%08x\n\n",
	   drive->name, drive->present, drive->media,
	   (cmd == WIN_IDENTIFY) ? "ATA" : "ATAPI",
	   drive->hwif);

  /* The following OUGHT to be redundant: */
  hwif_SelectDrive( drive->hwif, drive->ndx );
  //  Machine::SpinWaitMs(DRIVE_SELECT_DELAY);
  capros_Sleep_sleep( KR_SLEEP, DRIVE_SELECT_DELAY * 100 );

  ret = GET8( IDE_DRV_HD );
  bits_print( "ret", ret );
  bits_print( "all", drive->select.all );
  kprintf( KR_OSTREAM, "drive name = %s", drive->name );
  
  if ( ret != drive->select.all  && !drive->present ) {
      PUT8(IDE_DRV_HD, 0xa0); /* select drive 0 */
      kprintf( KR_OSTREAM, "Drive didn't select and not present\n");
      return 3;
  }
    
  uint8_t status = GET8(IDE_STATUS);
  uint8_t rc;

  if ( OK_STAT( status, READY_STAT, BUSY_STAT ) ||
      drive->present || 
       cmd == WIN_PIDENTIFY ) {
    
    /* try to identify the drive twice: */
      if  ( ( rc = drive_Identify( drive, cmd ) ) ) {}
      //rc = drive_Identify( drive, cmd );

    /* Make sure interrupt status is clear: */
    uint8_t status = GET8( IDE_STATUS );

    if (rc)
	kprintf( KR_OSTREAM, "%s: no response(status = 0x%02x)\n", drive->name,
		 status);

  }
  else {
      kprintf( KR_OSTREAM, "Drive didn't select\n");
      rc = 3;				/* not present or maybe ATAPI */
  }
  
  if (drive->select.b.unit != 0) {	/* unit not 0 */
      kprintf( KR_OSTREAM, "Reselect drive 0. ");
      PUT8(IDE_DRV_HD, 0xa0); /* select drive 0 */
      capros_Sleep_sleep( KR_SLEEP, DRIVE_SELECT_DELAY);

      /* Make sure interrupt status is clear: */
      uint8_t result = GET8( IDE_CTL_ALTSTATUS );
      kprintf( KR_OSTREAM, "Alt status : 0x%02x\n", result);
    
      (void) GET8(IDE_STATUS);
  }
  return rc;
}

void
drive_Probe( struct ide_drive *drive )
{
#if 0
  if (noprobe)			/* skip probing? */
    return;
#endif

  if ( drive_DoProbe( drive, WIN_IDENTIFY ) >= 2 ) {} //{ /* if !(success||timed-out) */
  //      (void) drive_DoProbe( drive, WIN_PIDENTIFY ); /* look for ATAPI device */
  //  }

  if ( !drive->present )
    return;			/* drive not found */

  drive->needsMount = true;
  
  /* Double check the results of the probe: */
  
  if (&(drive->id) == NULL) {		/* identification failed? */ //RMG need to set some value here??
      if (drive->media == med_disk) {
	  kprintf( KR_OSTREAM, "%s: old drive, CHS=%d/%d/%d\n",
		   drive->name, drive->b_chs.cyl, drive->b_chs.hd, drive->b_chs.sec);
    }
    else if (drive->media == med_cdrom) {
	kprintf( KR_OSTREAM, "%s: ATAPI CDROM?\n", drive->name);
    }
    else {
	drive->present = false;	/* nuke it */
    }
  }
}

void
drive_DoDriveCommand( struct ide_drive *drive, struct Request *req )
{
    kprintf( KR_OSTREAM, "Drive commands not implemented\n");
    // halt?
}

/*
 * do_rw_disk() issues READ and WRITE commands to a disk,
 * using LBA if supported, or CHS otherwise, to address sectors.
 * It also takes care of issuing special DRIVE_CMDs.
 */
#if 0
static inline void do_rw_disk (ide_drive_t *drive, struct request *rq, unsigned long block)
#endif
void
drive_DoDiskRequest(struct ide_drive *drive, struct Request *req)
{
  /* NOTE: Plug requests are handled in ide_hwif::StartRequest() */
  
    PUT8(IDE_CTL_ALTSTATUS, drive->ctl);

  /* We can do the whole transfer until proven otherwise. */
  req->nsec = req->req_nsec;
  
  uint8_t sec, hd;
  uint16_t cyl;
  uint8_t sel = 0;
  
  if (drive->select.b.lba) {
#if defined(IDE_DEBUG)
      kprintf(KR_OSTREAM, "%s: %sing: LBAsect=%d, sectors=%d, buffer=0x%08x\n",
	      drive->name, (req->cmd==Request::Read)?"read":"writ",
	      req->req_start, req->nsec, (unsigned long) req->req_ioaddr);
#endif
      sec = req->req_start;
      cyl = req->req_start >> 8;
    hd = (req->req_start >> 24) & 0x0f;
    
  } else {
    uint32_t track = req->req_start / drive->l_chs.sec;
    sec = req->req_start % drive->l_chs.sec;
    
    hd  = track % drive->l_chs.hd;
    cyl  = track / drive->l_chs.hd;

    sec++;

#if defined(IDE_DEBUG)
    kprintf(KR_OSTREAM, "%s: %sing: CHS=%d/%d/%d, sectors=%d, buffer=0x%08x\n",
	    drive->name, (req->cmd==Request::Read)?"read":"writ", cyl,
	    hd, sec, req->nsec, (unsigned long) req->req_ioaddr);
#endif
  }
  sel = hd | drive->select.all;

  /* IDE can always issue a request for more sectors than we need.  We
   * go ahead and issue the request for the complete read here.
   * 
   * FIX: Someday we need a strategy here for error recovery.
   */
  
  PUT8(IDE_NSECS, req->nsec);
  PUT8(IDE_SECTOR, sec);
  PUT8(IDE_CYL_LO, cyl);
  PUT8(IDE_CYL_HI, cyl >> 8);
  PUT8(IDE_DRV_HD, sel);

#if defined(IDE_DEBUG)
  kprintf(KR_OSTREAM, "Start %s: start %d nsec %d at 0x%08x\n",
	  ((req->cmd == Request::Read) ? "read" : "write"),
	  req->req_start, req->req_nsec, req->req_ioaddr);
#endif

#if 0
  if (IS_PROMISE_DRIVE) {
    if (drive->hwif->is_promise2 || rq->cmd == READ) {
      do_promise_io (drive, rq);
      return;
    }
  }
#endif

  if ( req->cmd == Read_Cmd ) {
#if defined(IDE_DEBUG)
    uint8_t status = GET8( IDE_STATUS );
    kprintf( KR_OSTREAM, "Status prior to initating: 0x%x\n", status );
#endif

    /* If we are able to use DMA, do so. */
    //assert (drive->use_dma == false);
    if (drive->use_dma && drive->hwif->dma_handler(dma_read, drive))
      return;

    hwif_SetHandler(drive->hwif, drive->ndx, &hwif_ReadIntr);
    PUT8(IDE_CMD, drive->multCount ? WIN_MULTREAD : WIN_READ);
#if defined(IDE_DEBUG)
    kprintf(KR_OSTREAM, "Return from DoDiskRequest\n");
#endif
    return;
  }

  if (req->cmd == Write_Cmd) {
    /* Use multwrite whenever possible. */

      //assert (use_dma == false);
    if (drive->use_dma && drive->hwif->dma_handler(dma_write, drive))
      return;

    PUT8(IDE_CMD, drive->multCount ? WIN_MULTWRITE : WIN_WRITE);

    if ( drive_WaitStatus( drive, DATA_READY, BAD_W_STAT, WAIT_DRQ) ) {
	kprintf( KR_OSTREAM, "%s: no DRQ after issuing %s\n", drive->name,
		 drive->multCount ? "MULTWRITE" : "WRITE");
	return;
    }

    if (drive->multCount) {
	drive->hwif->group->curReq = req;

	hwif_SetHandler (drive->hwif, drive->ndx, &hwif_MultWriteIntr);
	drive_MultWrite(drive, req);
    } else {
	hwif_SetHandler ( drive->hwif, drive->ndx, &hwif_WriteIntr);
	drive_OutputData( drive, (void*) req->req_ioaddr, EROS_SECTOR_SIZE >> 2);
    }

    return;
  }

#if 0
  kprintf( KR_OSTREAM, "%s: bad command: %d\n", drive->name, rq->cmd );
  ide_end_request( 0, HWGROUP(drive ) );
#endif
}

void
drive_EndDriveCmd( struct ide_drive *drive, uint8_t  stat , uint8_t  err  )
{
    kprintf(KR_OSTREAM, "Drive end command not implemented\n");
}


/*
 * Error reporting, in human readable form (luxurious, but a memory hog).
 */
uint8_t
drive_DumpStatus (struct ide_drive *drive, const char *msg, uint8_t stat, struct Request* req)
{
  uint8_t err = 0;

  kprintf( KR_OSTREAM, "%s: %s: status=0x%02x", drive->name, msg, stat);
#ifdef FANCY_STATUS_DUMPS
  if (drive->media == med_disk) {
      kprintf( KR_OSTREAM, " { ");
    if (stat & BUSY_STAT)
	{
	    kprintf( KR_OSTREAM, "Busy ");
	}
    else {
	      if (stat & READY_STAT)	kprintf( KR_OSTREAM, "DriveReady ");
	      if (stat & WRERR_STAT)	kprintf( KR_OSTREAM, "DeviceFault ");
	      if (stat & SEEK_STAT)	kprintf( KR_OSTREAM, "SeekComplete ");
	      if (stat & DRQ_STAT)	kprintf( KR_OSTREAM, "DataRequest ");
	      if (stat & ECC_STAT)	kprintf( KR_OSTREAM, "CorrectedError ");
	      if (stat & INDEX_STAT)	kprintf( KR_OSTREAM, "Index ");
	      if (stat & ERR_STAT)	kprintf( KR_OSTREAM, "Error ");
    }
        kprintf( KR_OSTREAM, "}");
  }
#endif	/* FANCY_STATUS_DUMPS */
  kprintf( KR_OSTREAM, "\n");
  if ((stat & (BUSY_STAT|ERR_STAT)) == ERR_STAT) {
      err = GET8(IDE_ERROR);
      
      kprintf( KR_OSTREAM, "%s: %s: error=0x%02x", drive->name, msg, err);
#ifdef FANCY_STATUS_DUMPS
      if ( drive->media == med_disk) {
	  kprintf( KR_OSTREAM, " { ");
	  if (err & BBD_ERR)	kprintf( KR_OSTREAM, "BadSector ");
	  if (err & ECC_ERR)	kprintf( KR_OSTREAM, "UncorrectableError ");
	  if (err & ID_ERR)	kprintf( KR_OSTREAM, "SectorIdNotFound ");
	  if (err & ABRT_ERR)	kprintf( KR_OSTREAM, "DriveStatusError ");
	  if (err & TRK0_ERR)	kprintf( KR_OSTREAM, "TrackZeroNotFound ");
	  if (err & MARK_ERR)	kprintf( KR_OSTREAM, "AddrMarkNotFound ");
	  kprintf( KR_OSTREAM, "}");

	  if (err & (BBD_ERR|ECC_ERR|ID_ERR|MARK_ERR)) {
	      uint8_t cur = GET8(IDE_DRV_HD);
	      if (cur & 0x40) {	/* using LBA? */
	    	  kprintf( KR_OSTREAM, ", LBAsect=%ld", (unsigned long)
			   ((cur&0xf)<<24)
			   |(GET8(IDE_CYL_HI)<<16)
			   |(GET8(IDE_CYL_LO)<<8)
			   | GET8(IDE_SECTOR));
	      } else {
	    	  kprintf( KR_OSTREAM, ", CHS=%d/%d/%d",
			   (GET8(IDE_CYL_HI)<<8) + GET8(IDE_CYL_LO),
			   cur & 0xf,	/* ?? WHY the limit ?? */
			   GET8(IDE_SECTOR));
	      }
	      if (req)
		  {
		      kprintf( KR_OSTREAM, ", sector=%ld", req->req_start);
		  }
	  }
      }
#endif	/* FANCY_STATUS_DUMPS */
      kprintf( KR_OSTREAM, "\n");
  }
  return err;
}

/*
 * drive_Error() takes action based on the error returned by the controller.
 */
void
drive_Error( struct ide_drive *drive, const char *msg, uint8_t stat)
{
    struct Request *req = NULL; //rq.GetNextRequest();
    uint8_t err;

    err = drive_DumpStatus(drive, msg, stat, req);
    if (req  == NULL)
	return;

    /* retry only "normal" I/O: */
    if (req->cmd == NUM_IOCMD) {
	req->nError = 1;
	drive_EndDriveCmd(drive, stat, err);
	return;
    }

    if ( stat & BUSY_STAT ) {		/* other bits are useless when BUSY */
	req->nError |= ERROR_RESET;
    }
    else {
	if ( drive->media == med_disk && (stat & ERR_STAT)) {
	    /* err has different meaning on cdrom and tape */
	    if (err & (BBD_ERR | ECC_ERR))	/* retries won't help these */
		req->nError = ERROR_MAX;
	    else if (err & TRK0_ERR)	/* help it find track zero */
		req->nError |= ERROR_RECAL;
	}
	if ((stat & DRQ_STAT) && req->cmd != Write_Cmd)
	    drive_FlushResidue( drive );
    }

    uint8_t status = GET8(IDE_STATUS);

    if (status & (BUSY_STAT|DRQ_STAT))
	req->nError |= ERROR_RESET;	/* Mmmm.. timing problem */

    if (req->nError >= ERROR_MAX) {
#ifdef CONFIG_BLK_DEV_IDETAPE
	if (drive->media == ide_tape) {
	    req->nError = 0;
	    idetape_end_request(0, HWGROUP(drive));
	}
	else
#endif /* CONFIG_BLK_DEV_IDETAPE */
	    /* Terminate the request: */
	    //Terminate( req );
    }
    else {
	if ((req->nError & ERROR_RESET) == ERROR_RESET) {
	    ++req->nError;
	    hwif_DoReset( drive->hwif, drive->ndx);
	    return;
	}
	else if ((req->nError & ERROR_RECAL) == ERROR_RECAL)
	    drive->flags.b.recal = 1;
	++req->nError;
    }

    kprintf( KR_OSTREAM, "A drive error occurred on drive %s\n",
	     drive->name);
}

/* FlushResidue() is invoked in response to a drive unexpectedly
 * having its DRQ_STAT bit set.  As an alternative to resetting the
 * drive, this routine tries to clear the condition by read a sector's
 * worth of data from the drive.  Of course, this may not help if the
 * drive is *waiting* for data from *us*.
 */
void
drive_FlushResidue( struct ide_drive *drive )
{
    uint32_t i = (drive->multCount ? drive->multCount : 1) * EROS_SECTOR_SIZE / 2; /* 16-bit xfers */

    while (i > 0) {
	unsigned long buffer[16];
	unsigned int wcount = (i > 16) ? 16 : i;
	i -= wcount;
	drive_InputData(drive, buffer, wcount);
    }
}

/*
 * do_special() is used to issue WIN_SPECIFY, WIN_RESTORE, and WIN_SETMULT
 * commands to a drive.  It used to do much more, but has been scaled back.
 */
void
drive_HandleDriveFlags( struct ide_drive *drive )
{
#if 0
 next:
#endif

    kprintf( KR_OSTREAM, "%s: do_special: 0x%02x\n", drive->name, drive->flags.all);

    if (drive->flags.b.setGeom) {
	drive->flags.b.setGeom = 0;
    
	if (drive->media == med_disk) {
	    PUT8(IDE_SECTOR, drive->l_chs.sec);
	    PUT8(IDE_CYL_LO, drive->l_chs.cyl);
	    PUT8(IDE_CYL_HI, drive->l_chs.cyl >> 8);
	    PUT8(IDE_DRV_HD, ((drive->l_chs.hd - 1) | (drive->select.all)) & 0xBFu);

	    if (!IS_PROMISE_DRIVE)
		hwif_DoCmd( drive->hwif, drive->ndx, WIN_SPECIFY, drive->l_chs.sec, &hwif_SetGeometryIntr);
	}
    }
    else if (drive->flags.b.recal) {
	drive->flags.b.recal = 0;
	if (drive->media == med_disk && !IS_PROMISE_DRIVE)
	    hwif_DoCmd( drive->hwif, drive->ndx, WIN_RESTORE, drive->l_chs.sec, &hwif_RecalibrateIntr);
    }
#if 0
    else if (drive->flags.b.set_pio) {
	ide_tuneproc_t *tuneproc = HWIF(drive)->tuneproc;
	drive->flags.b.set_pio = 0;
	if (tuneproc != NULL)
	    tuneproc(drive, drive->pio_req);
	goto next;
    }
#endif
    else if (drive->flags.b.setMultMode) {
	drive->flags.b.setMultMode = 0;
	if (drive->media == med_disk) { 
	    if (&(drive->id) && (drive->multReq > drive->id.max_multsect))
		drive->multReq = drive->id.max_multsect;
	    kprintf( KR_OSTREAM, "ship a WIN_SETMULT command multReq %d max %d\n",
		     drive->multReq, drive->id.max_multsect);
	    if (!IS_PROMISE_DRIVE)
		hwif_DoCmd( drive->hwif, drive->ndx, WIN_SETMULT, drive->multReq, &hwif_SetMultmodeIntr);
	}
	else
	    drive->multReq = 0;
    }
    else if (drive->flags.all) {
	uint8_t old_flags = drive->flags.all;
	old_flags = old_flags;
	drive->flags.all = 0;
	kprintf( KR_OSTREAM, "%s: bad special flag: 0x%02x\n", drive->name, old_flags);
    }
}

/* Busy wait for the drive to return the expected status, but not for
 * longer than timelimit.  Return true if there was an error.
 */
bool
drive_WaitStatus(struct ide_drive *drive, uint8_t good, uint8_t bad, uint32_t timelimit)
{
    //timeval tv;
    //  timelimit = Machine::MillisecondsToTicks(timelimit);
    //  timelimit += SysTimer::Now();
      
    //GetTimeNow();


  do {
    /* The IDE spec allows the drive 400ns to get it's act together,
     * This is incredibly stupid, as it guarantees at least a 1ms
     * delay whenever the status needs to be checked.  Common case is
     * that we will do the loop only once.
     * 
     * Note that this code needs to be redesigned, because the busy
     * wait loop has very bad implications for real-time.
     */
    
#ifdef VERBOSE_BUG
    uint32_t f = GetFlags();
    //    kprintf( KR_OSTREAM, "Before SpinWait: flags 0x%08x timer %s enabled.\n",
    //		   f,
    //		   IRQ::IsEnabled(0) ? "is" : "is not");
#endif

    //    Machine::SpinWaitUs(1);
    capros_Sleep_sleep( KR_SLEEP, 1 );

    uint8_t status = GET8(IDE_STATUS);
  
    if ( OK_STAT(status,good, bad) )
      return false;

    /* If the drive no longer purports to be busy, life is bad: */
    if ( (status & BUSY_STAT) == 0 ) {
      drive_Error(drive, "status error", status);
      return true;
    }
  } while( timelimit < 0); // RMG SysTimer::Now() );

  uint8_t status = GET8(IDE_STATUS);
  drive_Error(drive, "status timeout", status);
  return true;
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/* This routine will not be called unless there is really a request to
 * process.
 */
void
drive_DoRequest(struct ide_drive *drive, struct Request * req)
{
#if defined(IDE_DEBUG)
    kprintf( KR_OSTREAM, "selecting drive %d\n", ndx);
#endif
  
  hwif_SelectDrive(drive->hwif, drive->ndx);
  /* This causes infinite loop for some reason:
   *  Machine::SpinWaitMs(DRIVE_SELECT_DELAY);
   */
  // saw above note.  Is this sleep okay?
  capros_Sleep_sleep( KR_SLEEP, DRIVE_SELECT_DELAY );


  if ( drive_WaitStatus( drive, READY_STAT, BUSY_STAT|DRQ_STAT, WAIT_READY) ) {
      //    MsgLog::fatal("Drive not ready\n");
      kprintf( KR_OSTREAM, "Drive not ready\n");
      // fatal?  guess i should yank the its space bank.
    return;
  }
  
  if ( drive->flags.all ) {
      drive_HandleDriveFlags( drive );
#if defined(IDE_DEBUG)
    kprintf(KR_OSTREAM, "Handling driver flags\n");
#endif
    return;
  }

  if ( req->cmd >= NUM_IOCMD )
      drive_DoDriveCommand(drive, req);
  else {
    switch ( drive->media) {
    case med_disk:
	drive_DoDiskRequest( drive, req );
      break;
    default:
	break;
	kprintf( KR_OSTREAM, "Unimplemented media!\n");
    }
  }
}


/* functional units on the following list are known bad, and for the
 * time being we don't use them:
 */
#ifdef RMG
static struct BlackList {
  uint16_t vendor;
  uint16_t device;
  char *name;
} IdeBlackList[] = {
    { PCI_VENDOR_ID_CMD, PCI_DEVICE_ID_CMD_640, "CMD PCI0640B" },
    { PCI_VENDOR_ID_PCTECH, PCI_DEVICE_ID_PCTECH_RZ1000, "PC-TECH RZ1000" },
    { PCI_VENDOR_ID_PCTECH, PCI_DEVICE_ID_PCTECH_RZ1001, "PC-TECH RZ1001" }
};

const int nBlackguards = sizeof(IdeBlackList) / sizeof(BlackList);

void
InitChipsets()
{

    if (PciBios::Present()) {

	for (int i = 0; i < nBlackguards; i++) {
	    uint8_t bus;
	    uint8_t fn;

	    if (PciBios::FindDevice(IdeBlackList[i].vendor,
				    IdeBlackList[i].device,
				    0, bus, fn) != PciBios::DeviceNotFound) {
		kprintf( KR_OSTREAM, 
			 "FATAL: Your machine contains a PCI-IDE controller chip that has very\n"
			 "       serious flaws: the %s.\n", IdeBlackList[i].name);

		if (bus == 0) kprintf( KR_OSTREAM, 
				       "\n"
				       "       The offending chip is soldered into your motherboard.\n");
		kprintf( KR_OSTREAM, 
			 "\n"
			 "       Since these flaws can lead to corruption of data, and we have\n"
			 "       not had an opportunity to test our workarounds adequately, EROS\n"
			 "       currently does not run on machines containing these chips.\n"
			 "\n"
			 "       Windows 95 and Windows NT 3.5 or later include workarounds for\n"
			 "       these flaws.  If you are running earlier versions, it's past time\n"
			 "       to upgrade either your OS or your PCI board.\n"
			 "\n"
			 "       We apologize for any inconvenience this may have caused you.\n");
		halt('a');
	    }
	}
    }

}
#endif
