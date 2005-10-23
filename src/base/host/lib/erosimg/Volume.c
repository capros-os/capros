/*
 * Copyright (C) 1998, 1999, 2001, Jonathan S. Shapiro.
 * Copyright (C) 2005, Strawberry Development Group.
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

#include <assert.h>
#include <limits.h>
#include <stdlib.h>
#include <memory.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <time.h>

#include <a.out.h>

#include <sys/fcntl.h>
#include <sys/stat.h>

#include <disk/PagePot.h>
#include <erosimg/App.h>
#include <erosimg/Volume.h>
#include <erosimg/DiskDescrip.h>
#include <erosimg/ExecImage.h>
#include <erosimg/DiskKey.h>
#include <eros/Reserve.h>

#define BOOT "/eros/lib/boot/boot"
#define min(x,y) ((x) > (y) ? (y) : (x))

#define RESERVE_BYTES (MAX_CPU_RESERVE * sizeof(CpuReserveInfo))
#define RESERVES_PER_PAGE (EROS_PAGE_SIZE / sizeof(CpuReserveInfo))
#define RESERVE_PAGES ((RESERVE_BYTES + EROS_PAGE_SIZE - 1) / EROS_PAGE_SIZE)
    
static uint32_t
vol_GetOidFrameVolOffset(Volume *pVol, int ndx, OID oid)
{
  const Division *d = &pVol->divTable[ndx];
  uint32_t relPage;
  uint32_t divStart;
  uint32_t pageOffset;

  assert(div_contains(d, oid));

  assert ((d->startOid % EROS_OBJECTS_PER_FRAME) == 0);

  relPage = (uint32_t) ((oid - d->startOid) / EROS_OBJECTS_PER_FRAME);
  relPage += (relPage / DATA_PAGES_PER_PAGE_CLUSTER);
  relPage += 1;			/* clusters precede data */

  relPage += 1;			/* skip ckpt sequence pg */
  
  divStart = d->start * EROS_SECTOR_SIZE;
  pageOffset = relPage * EROS_PAGE_SIZE;

  return divStart + pageOffset;
}

static void
vol_InitVolume(Volume *pVol)
{
  int i;

  pVol->working_fd = -1;
  pVol->target_fd = -1;
  pVol->topDiv = 0;
  pVol->topLogLid = 0;
  pVol->lastAvailLogLid = 0;
  pVol->firstAvailLogLid = (2 * EROS_OBJECTS_PER_FRAME);

  pVol->dskCkptHdr0 = (DiskCheckpoint*) malloc(EROS_PAGE_SIZE);
  pVol->dskCkptHdr1 = (DiskCheckpoint*) malloc(EROS_PAGE_SIZE);
  pVol->curDskCkpt = pVol->dskCkptHdr0;
  pVol->oldDskCkpt = pVol->dskCkptHdr1;
  
  pVol->rewriting = true;

  pVol->ckptDir = 0;
  pVol->maxCkptDirent = 0;
  pVol->nCkptDirent = 0;

  pVol->threadDir = 0;
  pVol->maxThreadDirent = 0;
  pVol->nThreadDirent = 0;

  pVol->reserveTable = 
    (CpuReserveInfo *) malloc(sizeof(CpuReserveInfo) * MAX_CPU_RESERVE);
  bzero(pVol->reserveTable, sizeof(CpuReserveInfo) * MAX_CPU_RESERVE);
  
  /* Following just for now... */
  for (i = 0; i < STD_CPU_RESERVE; i++) {
    pVol->reserveTable[i].index = i;
    pVol->reserveTable[i].normPrio = i;
    pVol->reserveTable[i].rsrvPrio = -2;
  }
  for (i = STD_CPU_RESERVE; i < MAX_CPU_RESERVE; i++) {
    pVol->reserveTable[i].index = i;
    pVol->reserveTable[i].normPrio = -2;
    pVol->reserveTable[i].rsrvPrio = -2;
  }
  
  pVol->needSyncDivisions = false;
  pVol->needSyncHdr = false;
  pVol->needSyncCkptLog = false;
  pVol->needDivInit = false;

  pVol->curLogPotLid = UNDEF_LID;
}

static void
vol_Init(Volume *pVol)
{
  vol_InitVolume(pVol);
#ifdef DEAD_BAD_MAP /* spare ranges are a thing of the past */
  pVol->divTable = (Division *) pVol->divPage;
  pVol->badmap = (BadEnt*) &pVol->divTable[NDIVENT];
#endif
}

const CpuReserveInfo *
vol_GetReserve(const Volume *pVol, uint32_t ndx)
{
  if (ndx >= MAX_CPU_RESERVE)
    diag_fatal(1, "Reserve %d is out of range\n");
  
  return &pVol->reserveTable[ndx];
}

void
vol_SetReserve(Volume *pVol, const CpuReserveInfo *pCri)
{
  if (pCri->index >= MAX_CPU_RESERVE)
    diag_fatal(1, "Reserve %d is out of range\n");
  
  pVol->reserveTable[pCri->index] = *pCri;
}

static bool
vol_Read(Volume *pVol, uint32_t offset, void *buf, uint32_t sz)
{
  assert(pVol->working_fd >= 0);

  if (lseek(pVol->working_fd, (int) offset, SEEK_SET) < 0)
    return false;

  if (read(pVol->working_fd, buf, (int) sz) != (int) sz)
    return false;

  return true;
}

static bool
vol_Write(Volume *pVol, uint32_t offset, const void *buf, uint32_t sz)
{
  assert(pVol->working_fd >= 0);

  if (lseek(pVol->working_fd, (int) offset, SEEK_SET) < 0)
    return false;

  if (write(pVol->working_fd, buf, (int) sz) != (int) sz)
    return false;

  return true;
}


/* This version finds an empty spot big enough for the division and puts it
 * there.
 */
int
vol_AddDivisionWithOid(Volume *pVol, DivType type, uint32_t sz, OID oid)
{
  uint32_t nObFrames = sz / EROS_PAGE_SECTORS;
  OID endOid;
  int div;

  switch (type) {
  case dt_Kernel:
  case dt_Object:

    /* Must have at least space for the first page, one pot, and one frame. */
    if (nObFrames < 3)
      diag_fatal(1, "Range too small\n");

    /* Take out one for the first page, which is used to capture
     * seqno of most recent checkpoint.
     */
    nObFrames--;

    /* Take out a pot for each whole or partial cluster. */
    nObFrames -= (nObFrames + (PAGES_PER_PAGE_CLUSTER-1)) / PAGES_PER_PAGE_CLUSTER;

    endOid = oid + (nObFrames * EROS_OBJECTS_PER_FRAME);
    break;

  case dt_Log:
    endOid = oid + (nObFrames * EROS_OBJECTS_PER_FRAME);
    break;

  default:
    diag_fatal(1, "Attempt to set OID on inappropriate division type\n");
    break;
  }
  
  if ((type != dt_Log) && (oid % EROS_OBJECTS_PER_FRAME))
    diag_fatal(1, "Starting OID for range must be multiple of %d\n",
		EROS_OBJECTS_PER_FRAME);
    
  div = vol_AddAdjustableDivision(pVol, type, sz);
  
  if (type == dt_Log && endOid > pVol->topLogLid) {
    pVol->topLogLid = endOid;
    pVol->lastAvailLogLid = endOid;
  }
    
  pVol->divTable[div].startOid = oid;
  pVol->divTable[div].endOid = endOid;

  diag_printf("Division %d: %6d %6d [%6s] %s=[",
	       div, pVol->divTable[div].start, pVol->divTable[div].end,
	       div_TypeName(pVol->divTable[div].type),
	       (type == dt_Log) ? "LID" : "OID");
  diag_printOid(pVol->divTable[div].startOid);
  diag_printf(", ");
  diag_printOid(pVol->divTable[div].endOid);
  diag_printf(")\n");
  
  return div;
}

/* This version finds an empty spot big enough for the division and puts it
 * there.
 */
int
vol_AddDivision(Volume *pVol, DivType type, uint32_t sz)
{
  int div;

  switch (type) {
  case dt_Object:
  case dt_Log:
    diag_fatal(1, "Attempt to create %s division without OID\n",
		div_TypeName(type));
    break;
  default:
    break;
  }
  
  div = vol_AddAdjustableDivision(pVol, type, sz);
  diag_printf("Division %d: %6d %6d [%6s]\n",
	      div, pVol->divTable[div].start, 
	      pVol->divTable[div].end,
	      div_TypeName(pVol->divTable[div].type));

  return div;
}

/* The versions that follow add a division at a fixed location: */
static int
vol_DoAddDivision(Volume *pVol, DivType type, uint32_t start, uint32_t sz)
{
  uint32_t end;
  int i;
  int div;

  assert(pVol->working_fd >= 0);
  
  if (pVol->topDiv >= NDIVENT)
    diag_fatal(4, "Too many divisions\n");
  
  end = start + sz;
  
  /* Need to verify that this division does not overlap some other
   * division if it has nonzero size
   */
  
  if (sz) {
    for (i = 0; i < pVol->topDiv; i++) {
      if ((pVol->divTable[i].start != pVol->divTable[i].end) &&
	  ( (start >= pVol->divTable[i].start && start < pVol->divTable[i].end)
	    || (end > pVol->divTable[i].start && end <= pVol->divTable[i].end) ))
	diag_fatal(4, "Division range error\n");
    }
  }
  
  div = pVol->topDiv;
  pVol->topDiv++;
  
  pVol->divTable[div].type = type;
  pVol->divTable[div].start = start;
  pVol->divTable[div].end = end;
  pVol->divTable[div].startOid = 0;
  pVol->divTable[div].endOid = 0;
  
  if (type == dt_DivTbl) {
    /* Perfectly okay to have lots of division tables, but only two
     * end up in the volume header:
     */
    
    /* assert(!volHdr.AltDivTable); */
    if (!pVol->volHdr.DivTable)
      pVol->volHdr.DivTable = start;
    else
      pVol->volHdr.AltDivTable = start;
  }

  pVol->needSyncHdr = 1;		/* if nothing else, volume size changed */
  
#ifdef DEAD_BAD_MAP /* spare ranges are a thing of the past */
  if (type == dt_Spare)
    spareDivNdx = div;
#endif
  
  pVol->divNeedsInit[div] = true;
  pVol->needDivInit = true;

  pVol->needSyncDivisions = true;
  
  return div;
}

int
vol_AddAdjustableDivision(Volume *pVol, DivType type, uint32_t sz)
{
  int i;
  uint32_t start = 0;
  int div;

  assert(pVol->working_fd >= 0);
  
      /* make sure we do not collide with another division: */
  for (i = 0; i < pVol->topDiv; i++) {
    uint32_t end = start + sz;
	/* if some other division starts or ends in our proposed range,
	 * go to the end of the colliding division:
	 */
    if ((pVol->divTable[i].start >= start && pVol->divTable[i].start < end) ||
	(pVol->divTable[i].end > start && pVol->divTable[i].end <= end)) {
      start = pVol->divTable[i].end;
      i = -1;			/* restart for loop */
    }
  }
  
  div = vol_DoAddDivision(pVol, type, start, sz);

  return div;
}

int
vol_AddFixedDivision(Volume *pVol, DivType type, uint32_t start, uint32_t sz)
{
  int div = vol_DoAddDivision(pVol, type, start,  sz);
  
  diag_printf("Division %d: %6d %6d [%6s]\n",
	       div, start, start+sz, div_TypeName(pVol->divTable[div].type));

  return div;
}

static void
vol_FormatObjectDivision(Volume *pVol, int ndx)
{
  Division *d;
  OID oid;

  assert(pVol->working_fd >= 0);

  if (ndx >= pVol->topDiv)
    diag_fatal(1, "Attempt to format nonexistent division\n");
  
  d = &pVol->divTable[ndx];
  
  for (oid = d->startOid; oid < d->endOid; oid += EROS_OBJECTS_PER_FRAME) {
    VolPagePot pagePot;
    vol_ReadPagePotEntry(pVol, oid, &pagePot);
    pagePot.type = FRM_TYPE_ZDPAGE;
    pagePot.count = 0;
    vol_WritePagePotEntry(pVol, oid, &pagePot);
    oid++;
  }

  /* Set up a first page with suitable ckpt sequence number: */
  {
    uint8_t buf[EROS_PAGE_SIZE];
    uint64_t *seqNo = (uint64_t *) buf;

    *seqNo = 1;

    vol_Write(pVol, d->start * EROS_SECTOR_SIZE, buf, EROS_PAGE_SIZE);
  }
}

/* Once the log division has been zeroed, we simply need to write
 * suitable checkpoint header pages:
 */
static void
vol_FormatLogDivision(Volume *pVol, int ndx)
{
  Division *d;
  assert(pVol->working_fd >= 0);

  if (ndx >= pVol->topDiv)
    diag_fatal(1, "Attempt to format nonexistent division\n");
  
  d = &pVol->divTable[ndx];
  
  if (d->startOid == 0) {
    pVol->oldDskCkpt->hdr.sequenceNumber = 0;
    pVol->oldDskCkpt->hdr.hasMigrated = true;
    pVol->oldDskCkpt->hdr.maxLogLid = pVol->topLogLid;
    pVol->oldDskCkpt->hdr.nRsrvPage = 0;
    pVol->oldDskCkpt->hdr.nThreadPage = 0;
    pVol->oldDskCkpt->hdr.nDirPage = 0;

    pVol->curDskCkpt->hdr.sequenceNumber = 1;
    pVol->curDskCkpt->hdr.hasMigrated = false;
    pVol->curDskCkpt->hdr.maxLogLid = pVol->topLogLid;
    pVol->curDskCkpt->hdr.nRsrvPage = 0;
    pVol->curDskCkpt->hdr.nThreadPage = 0;
    pVol->curDskCkpt->hdr.nDirPage = 0;

    vol_WriteLogPage(pVol,(lid_t)0, (uint8_t *) pVol->dskCkptHdr0);
    vol_WriteLogPage(pVol, (lid_t)(1*EROS_OBJECTS_PER_FRAME), (uint8_t *) pVol->dskCkptHdr1);
  }

  pVol->needSyncCkptLog = true;
}

static void
vol_ZeroDivision(Volume *pVol, int ndx)
{
  uint8_t buf[EROS_SECTOR_SIZE];
  Division *d;
  uint32_t i;

  assert(pVol->working_fd >= 0);

  if (ndx >= pVol->topDiv)
    diag_fatal(1, "Attempt to zero nonexistent division\n");
  
  bzero(buf, EROS_SECTOR_SIZE);
  
  d = &pVol->divTable[ndx];
  
  for (i = d->start; i < d->end; i++)
    if ( !vol_Write(pVol, i * EROS_SECTOR_SIZE, buf, EROS_SECTOR_SIZE) )
      diag_fatal(1, "Couldn't zero division %d\n", ndx);

  pVol->divNeedsInit[ndx] = false;
}

void
vol_SetVolFlag(Volume *pVol, VolHdrFlags flag)
{
  pVol->volHdr.BootFlags |= flag;
  pVol->needSyncHdr = 1;
}

void
vol_ClearVolFlag(Volume *pVol, VolHdrFlags flag)
{
  pVol->volHdr.BootFlags &= ~flag;
  pVol->needSyncHdr = 1;
}

void
vol_SetIplSysId(Volume *pVol, uint64_t dw)
{
  pVol->volHdr.iplSysId = dw;

  pVol->needSyncHdr = 1;
}

void
vol_SetIplKey(Volume *pVol, const KeyBits *k)
{
  pVol->volHdr.iplKey = *k;

  pVol->needSyncHdr = 1;
}

void
vol_WriteKernelImage(Volume *pVol, int div, const ExecImage *pImage)
{
  const ExecRegion* er;
  uint32_t bytes;
  const uint8_t *fileImage;
  uint32_t divPages;
  Division *d;
  OID oid;

  vol_ZeroDivision(pVol, div);
  vol_FormatObjectDivision(pVol, div);
  pVol->divNeedsInit[div] = false;
  
  assert(pVol->working_fd >= 0);
  
  if (div >= pVol->topDiv)
    diag_fatal(1, "Attempt to write image %s on nonexistent division\n",
	       xi_GetName(pImage));

  if (xi_NumRegions(pImage) != 1)
    diag_fatal(1, "Image %s has inappropriate number of regions\n",
	       xi_GetName(pImage));

  er = xi_GetRegion(pImage, 0);

  /* If the image was built correctly, it should start at a page boundary: */
  if (er->vaddr % EROS_PAGE_SIZE)
    diag_fatal(1, "ELF Image %s has start vaddr at nonpage boundary\n",
	       xi_GetName(pImage));

  bytes = er->filesz;
  fileImage = xi_GetImage(pImage);
  fileImage += er->offset;
  
  d = &pVol->divTable[div];
  oid = d->startOid;

  divPages = (d->endOid - d->startOid) / EROS_OBJECTS_PER_FRAME;
  if (divPages * EROS_PAGE_SIZE < er->filesz)
    diag_fatal(1, "Image \"%s\" (%d bytes) will not fit in division %d (%d pages)\n",
		xi_GetName(pImage), bytes, div, divPages);

  while (bytes) {
    uint8_t buf[EROS_PAGE_SIZE];
    
    memset(buf, 0, EROS_PAGE_SIZE);
    memcpy(buf, fileImage, min (bytes, EROS_PAGE_SIZE));
    vol_WriteDataPage(pVol, oid, buf);

    fileImage += EROS_PAGE_SIZE;
    bytes -= min(bytes, EROS_PAGE_SIZE);
    oid += EROS_OBJECTS_PER_FRAME;
  }
}

/* NOTE!!! This only works for dense images, such as the kernel and
 * bootstrap code.  Domain images MUST be handled through the
 * DomainImage class.  Offset is in bytes.
 */
void
vol_WriteImageAtDivisionOffset(Volume *pVol, int div, const ExecImage *pImage,
			       uint32_t offset)
{
  const ExecRegion *er;
  uint32_t secs;
  uint32_t imageSz;
  uint8_t *imageBuf;
  const uint8_t *fileImage;
  Division *d;
  uint32_t divStart;
  uint32_t divEnd;

  assert(pVol->working_fd >= 0);
  
  if (div >= pVol->topDiv)
    diag_fatal(1, "Attempt to write image %s on nonexistent division\n",
	       xi_GetName(pImage));

  if (xi_NumRegions(pImage) != 1)
    diag_fatal(1, "Image %s has inappropriate number of regions\n",
	       xi_GetName(pImage));

  er = xi_GetRegion(pImage, 0);

  /* If the image was built correctly, it should start at a page boundary: */
  if (er->vaddr % EROS_PAGE_SIZE)
    diag_fatal(1, "ELF Image %s has start vaddr at nonpage boundary\n",
		xi_GetName(pImage));
    
  secs = er->filesz / EROS_SECTOR_SIZE;
  if (er->filesz % EROS_SECTOR_SIZE)
    secs++;
  
  imageSz = secs * EROS_SECTOR_SIZE;
  
  /* The following copy is necessary because the file itself may
   * contain garbage or other stuff at the tail of the alleged region,
   * so we must zero-extend to the end of the sector by hand.
   */
  
  imageBuf = (uint8_t *) malloc(imageSz);
  bzero(imageBuf, imageSz);

  fileImage = xi_GetImage(pImage);
  
  memcpy(imageBuf, &fileImage[er->offset], er->filesz);

  d = &pVol->divTable[div];
  
  divStart = d->start * EROS_SECTOR_SIZE;
  divEnd   = d->end * EROS_SECTOR_SIZE;

  if (divStart + offset + imageSz > divEnd)
    diag_fatal(1, "Image \"%s\" will not fit in division %d\n",
		xi_GetName(pImage), div);

  vol_Write(pVol, divStart + offset, imageBuf, imageSz);
  
  free(imageBuf);

  pVol->divNeedsInit[div] = false;
}

void
vol_DelDivision(Volume *pVol, int ndx)
{
  int i;
  assert(pVol->working_fd >= 0);

  if (ndx >= pVol->topDiv)
    diag_fatal(1, "Attempt to delete nonexistent division\n");

  switch (pVol->divTable[ndx].type) {
  case dt_Boot:
  case dt_DivTbl:
      diag_fatal(1, "Division type %s cannot be deleted\n",
		  div_TypeName(pVol->divTable[ndx].type));
      break;
  break;
  case dt_Spare:
    /* Can only delete spare division if no spare sectors are
     * in use.
     */
    break;
  }

  for (i = ndx+1; i < pVol->topDiv; i++)
    pVol->divTable[i-1] = pVol->divTable[i];
  pVol->divTable[i].type = dt_Unused;

  pVol->topDiv--;
  pVol->needSyncHdr = 1;		/* volume size may have changed */
}

uint32_t
vol_DivisionSetFlags(Volume *pVol, int ndx, uint32_t flags)
{
  assert(pVol->working_fd >= 0);

  if (ndx >= pVol->topDiv)
    diag_fatal(1, "Attempt to update flags for nonexistent division\n");

  pVol->divTable[ndx].flags |= flags;

  diag_printf("Division %d: flags set to [ ", ndx);
  if (pVol->divTable[ndx].flags & DF_PRELOAD)
    diag_printf("preload ");
  diag_printf("]\n");

  pVol->needSyncDivisions = true;

  return pVol->divTable[ndx].flags;
}

uint32_t
vol_DivisionClearFlags(Volume *pVol, int ndx, uint32_t flags)
{
  assert(pVol->working_fd >= 0);

  if (ndx >= pVol->topDiv)
    diag_fatal(1, "Attempt to update flags for nonexistent division\n");

  pVol->divTable[ndx].flags &= ~flags;

  diag_printf("Division %d: flags set to [ ", ndx);
  if (pVol->divTable[ndx].flags & DF_PRELOAD)
    diag_printf("preload ");
  diag_printf("]\n");

  pVol->needSyncDivisions = true;

  return pVol->divTable[ndx].flags;
}

uint32_t
vol_GetDivisionFlags(Volume *pVol, int ndx)
{
  assert(pVol->working_fd >= 0);

  if (ndx >= pVol->topDiv)
    diag_fatal(1, "Attempt to get flags for nonexistent division\n");

  return pVol->divTable[ndx].flags;
}

void
vol_WriteBootImage(Volume *pVol, const char *bootName)
{
  uint8_t buf[EROS_PAGE_SIZE];
  int i;

  assert(pVol->working_fd >= 0);

  if (bootName) {
    ExecImage *bootImage = xi_create();
    if ( !xi_SetImage(bootImage, bootName) )
      app_ExitWithCode(1);

    vol_WriteImageAtDivisionOffset(pVol, 0, bootImage, 0);
    xi_destroy(bootImage);
  }
  else {
    vol_ZeroDivision(pVol, 0);
  }
  
  if (!vol_Read(pVol, 0, buf, EROS_PAGE_SIZE))
    diag_fatal(1, "Unable to read boot sector\n");
  
  /* copy leading jump into volume header */
  memcpy(pVol->volHdr.code, buf, sizeof(pVol->volHdr.code));

  /* Recompute the image size from the division table: */
  pVol->volHdr.VolSectors = 0;
  if (bootName)
    pVol->volHdr.BootFlags |= VF_BOOT;
  else
    pVol->volHdr.BootFlags &= ~VF_BOOT;
  
  for (i = 0; i < pVol->topDiv; i++) {
    if (pVol->divTable[i].end > pVol->volHdr.VolSectors)
      pVol->volHdr.VolSectors = pVol->divTable[i].end;
  }
  
  /* put the volume header on the boot sector */
  memcpy(buf, &pVol->volHdr, sizeof(pVol->volHdr));

  if (!vol_Write(pVol, 0, buf, EROS_PAGE_SIZE))
    diag_fatal(1, "Unable to read boot sector\n");

  pVol->needSyncHdr = 0;
}

static CkptDirent*
vol_LookupObject(Volume *pVol, OID oid)
{
  uint32_t i;

  for (i = 0; i < pVol->nCkptDirent; i++) {
    if ( pVol->ckptDir[i].oid == oid )
      return &pVol->ckptDir[i];
  }

  return 0;
}

static lid_t
vol_AllocLogDirPage(Volume *pVol)
{
  lid_t theLid;

  if (pVol->lastAvailLogLid <= pVol->firstAvailLogLid)
    diag_fatal(3, "Log pages exhausted\n");

  theLid = pVol->firstAvailLogLid;
  pVol->firstAvailLogLid += EROS_OBJECTS_PER_FRAME;
  return theLid;
}

static void
vol_GrowCkptDir(Volume *pVol)
{
  CkptDirent *newDir;

  vol_AllocLogDirPage(pVol);

  pVol->maxCkptDirent += ckdp_maxDirEnt;

  newDir = (CkptDirent *) malloc(sizeof(CkptDirent) * pVol->maxCkptDirent);
  if (pVol->ckptDir)
    memcpy(newDir, pVol->ckptDir, pVol->nCkptDirent * sizeof(CkptDirent));

  free(pVol->ckptDir);
  pVol->ckptDir = newDir;
}

static void
vol_AddDirent(Volume *pVol, OID oid, ObCount allocCount, lid_t lid, uint8_t ckObType)
{
  CkptDirent* cpd = vol_LookupObject(pVol, oid);
  
  if (cpd == 0) {
    if (pVol->maxCkptDirent == pVol->nCkptDirent)
      vol_GrowCkptDir(pVol);

    cpd = &pVol->ckptDir[pVol->nCkptDirent];
    pVol->nCkptDirent++;
  }

  cpd->oid = oid;
  cpd->count = allocCount;
  cpd->lid = lid;
  cpd->type = ckObType;

  pVol->needSyncCkptLog = true;
}

static void
vol_LoadLogHeaders(Volume *pVol)
{
  vol_ReadLogPage(pVol, 0, (uint8_t*) pVol->dskCkptHdr0);
  vol_ReadLogPage(pVol, 1*EROS_OBJECTS_PER_FRAME, (uint8_t*) pVol->dskCkptHdr1);

  if (pVol->dskCkptHdr0->hdr.sequenceNumber > 
      pVol->dskCkptHdr1->hdr.sequenceNumber) {
    pVol->curDskCkpt = pVol->dskCkptHdr0;
    pVol->oldDskCkpt = pVol->dskCkptHdr1;
  }
  else {
    pVol->curDskCkpt = pVol->dskCkptHdr1;
    pVol->oldDskCkpt = pVol->dskCkptHdr0;
  }
}

static void
vol_LoadLogDirectory(Volume *pVol)
{
  uint32_t logEnt = 0;
  uint32_t d;

  assert(pVol->curDskCkpt);
  
  /* Read in the reserve table: */
  for (d = 0; d < pVol->curDskCkpt->hdr.nRsrvPage; d++, logEnt++) {
    uint8_t dirPage[EROS_PAGE_SIZE];
    ReserveDirPage *rdp;
    uint32_t entry;

    vol_ReadLogPage(pVol, pVol->curDskCkpt->dirPage[logEnt], dirPage);
    rdp = (ReserveDirPage*) dirPage;

    for (entry = 0; entry < rdp->hdr.nDirent; entry++) {
      CpuReserveInfo *cri = &rdp->entry[entry];
	
      memcpy(&pVol->reserveTable[cri->index], cri, sizeof(CpuReserveInfo));
    }
  }

  /* Read the thread directory from the current checkpoint. */
  for (d = 0; d < pVol->curDskCkpt->hdr.nThreadPage; d++, logEnt++) {
    uint8_t dirPage[EROS_PAGE_SIZE];
    ThreadDirPage *tdp;
    uint32_t entry;

    vol_ReadLogPage(pVol, pVol->curDskCkpt->dirPage[logEnt], dirPage);
    tdp = (ThreadDirPage*) dirPage;

    for (entry = 0; entry < tdp->hdr.nDirent; entry++) {
      ThreadDirent *tde = &tdp->entry[entry];
	
      vol_AddThread(pVol, tde->oid, tde->allocCount, tde->schedNdx);
    }
  }

  /* Read the object directory from the current checkpoint. */
  if (pVol->curDskCkpt->hdr.hasMigrated == false) {
    for (d = 0; d < pVol->curDskCkpt->hdr.nDirPage; d++, logEnt++) {
      uint8_t dirPage[EROS_PAGE_SIZE];
      CkptDirPage *dp;
      uint32_t entry;

      vol_ReadLogPage(pVol, pVol->curDskCkpt->dirPage[logEnt], dirPage);
      dp = (CkptDirPage*) dirPage;

      for (entry = 0; entry < dp->hdr.nDirent; entry++) {
	CkptDirent *de = &dp->entry[entry];
	
	vol_AddDirent(pVol, de->oid, de->count, de->lid, de->type);
      }
    }
  }
}

static void
vol_SyncDivTables(Volume *pVol)
{
  int div;

  if (!pVol->needSyncDivisions)
    return;
  
  assert(pVol->working_fd >= 0);
  
  for (div = 0; div < pVol->topDiv; div++) {
    Division *d = &pVol->divTable[div];
    
    if (d->type == dt_DivTbl) {
      uint8_t buf[EROS_PAGE_SIZE];
      
      uint32_t divStart = d->start * EROS_SECTOR_SIZE;

      if ( !vol_Read(pVol, divStart, buf, EROS_PAGE_SIZE) )
	diag_fatal(1, "Unable to read division table\n");

      memcpy(buf, pVol->divTable, NDIVENT * sizeof(Division));
      
      if ( !vol_Write(pVol, divStart, buf, EROS_PAGE_SIZE) )
	diag_fatal(1, "Unable to write division table\n");
    }
  }

  pVol->needSyncDivisions = false;
}

static void
vol_InitDivisions(Volume *pVol)
{
  int i;

  for (i = 0; i < pVol->topDiv; i++) {
    if (pVol->divNeedsInit[i]) {
      vol_ZeroDivision(pVol, i);

      switch (pVol->divTable[i].type) {
      case dt_Object:
      case dt_Kernel:
	vol_FormatObjectDivision(pVol, i);
        break;

      case dt_Log:
	vol_FormatLogDivision(pVol, i);
        break;

      }

      pVol->divNeedsInit[i] = false;
    }
  }

  pVol->needDivInit = false;
}

Volume *
vol_Create(const char* targname, const char* bootName)
{
  int i;

  Volume *pVol = (Volume *) malloc(sizeof(Volume));

  vol_Init(pVol);

#if 0
  if (!pDisk)
    diag_fatal(2, "Disk model undefined\n");
#endif
  
  pVol->grubDir = 0;
  pVol->suffix = 0;
  pVol->bootDrive = 0;

  assert(pVol->working_fd == -1);
  assert(pVol->target_fd == -1);
  
  pVol->volHdr.HdrVersion = VOLHDR_VERSION;
  pVol->volHdr.PageSize = EROS_PAGE_SIZE;
  pVol->volHdr.DivTable = 0;
  pVol->volHdr.AltDivTable = 0;
  pVol->volHdr.BootFlags = 0;
  pVol->volHdr.BootSectors = DISK_BOOTSTRAP_SECTORS;
  pVol->volHdr.VolSectors = 0;
  keyBits_InitToVoid(&pVol->volHdr.iplKey);
  pVol->volHdr.iplSysId = time(NULL); /* get a random value, hopefully unique  */
  pVol->volHdr.signature[0] = 'E';
  pVol->volHdr.signature[1] = 'R';
  pVol->volHdr.signature[2] = 'O';
  pVol->volHdr.signature[3] = 'S';
  
  /* Open the target file */
  pVol->target_fd = open(targname, O_RDWR|O_TRUNC);

  if (pVol->target_fd < 0 && errno == ENOENT)
    pVol->target_fd = open(targname, O_RDWR|O_CREAT, 0666);

  if (pVol->target_fd < 0) {
    free(pVol);
    return 0;
  }
  
  /* Either way, target is now clobbered, so arrange to delete it if
     we screw up. */
  app_AddTarget(targname);
  
  /* We assume uncompressed until told otherwise */
  pVol->working_fd = pVol->target_fd;
  
  for (i = 0; i < NDIVENT; i++)
    pVol->divNeedsInit[i] = false;

      /* Add division table entries for standard items: */
  vol_AddFixedDivision(pVol, dt_Boot, 0, DISK_BOOTSTRAP_SECTORS);
  vol_AddFixedDivision(pVol, dt_DivTbl, DISK_BOOTSTRAP_SECTORS,
		       EROS_PAGE_SECTORS); 

#if 0
  if (pDisk->badsecs)
    pVol->AddDivision(dt_Spare, pDisk->badsecs);
#endif

  /* Must initialize divisions first, or writing boot page and
   * sync'ing division tables will fail!
   */
  vol_InitDivisions(pVol);
  
  /* We write the boot page and division page immediately.  If
   * this fails, the disk is quite bad.
   */
  
  vol_WriteBootImage(pVol, bootName);
  
  vol_SyncDivTables(pVol);
  
  return pVol;
}

Volume *
vol_Open(const char* targname, bool rewriting,
         const char * grubDir, const char * suffix,
         uint32_t bootDrive)
{
  int i;

  Volume *pVol = (Volume *) malloc(sizeof(Volume));

  vol_Init(pVol);
  
  pVol->grubDir = grubDir;
  pVol->suffix = suffix;
  pVol->bootDrive = bootDrive;

  assert(pVol->working_fd == -1);
  assert(pVol->target_fd == -1);
  
  /* Open the target file descriptor -- we will do nothing to disturb
     its content until the call to Volume::Close() */
  if ((pVol->target_fd = open(targname, O_RDWR)) < 0) {
    free(pVol);
    return 0;
  }

  pVol->working_fd = pVol->target_fd;
  
  if (! vol_Read(pVol, 0, &pVol->volHdr, sizeof(pVol->volHdr)) )
    diag_fatal(3, "Couldn't read volume header\n");

  pVol->needSyncHdr = 0;
  
  if ( !vol_Read(pVol, EROS_SECTOR_SIZE * pVol->volHdr.DivTable,
		      &pVol->divTable, NDIVENT * sizeof(Division)) )
    diag_fatal(3, "Couldn't read primary division table\n");

  pVol->needSyncDivisions = 0;
  pVol->needSyncCkptLog = 0;

  for (i = 0; i < NDIVENT; i++)
    pVol->divNeedsInit[i] = false;

      /* interpret the newly loaded tables: */
  for (i = 0; i < NDIVENT; i++) {
    if (pVol->divTable[i].type == dt_Unused) {
      pVol->topDiv = i;
      break;
    }

    if (pVol->divTable[i].type == dt_Log &&
	pVol->divTable[i].endOid > pVol->topLogLid) {
      pVol->topLogLid = pVol->divTable[i].endOid;
      pVol->lastAvailLogLid = pVol->divTable[i].endOid;
    }
  }

  vol_LoadLogHeaders(pVol);

  if (rewriting == false && pVol->topLogLid) {
    /* We are opening for volume debugging rather than writing an
     * erosimage onto the volume.  We must therefore read in the
     * directory of the existing checkpoint log.
     */

    vol_LoadLogDirectory(pVol);
  }
  
  return pVol;
}

static lid_t
vol_AllocLogPage(Volume *pVol)
{
  if (pVol->lastAvailLogLid <= pVol->firstAvailLogLid)
    diag_fatal(3, "Log pages exhausted\n");

  pVol->lastAvailLogLid -= EROS_OBJECTS_PER_FRAME;

  return pVol->lastAvailLogLid;
}

static void
vol_GrowThreadDir(Volume *pVol)
{
  ThreadDirent *newDir;
  vol_AllocLogDirPage(pVol);

  pVol->maxThreadDirent += tdp_maxDirEnt;

  newDir = 
    (ThreadDirent *) malloc(sizeof(ThreadDirent) * pVol->maxThreadDirent);
  if (pVol->threadDir)
    memcpy(newDir, pVol->threadDir, pVol->nThreadDirent * sizeof(ThreadDirent));
  free(pVol->threadDir);
  pVol->threadDir = newDir;
}

static ThreadDirent*
vol_LookupThread(Volume *pVol, OID oid)
{
  uint32_t i;

  for (i = 0; i < pVol->nThreadDirent; i++) {
    if ( pVol->threadDir[i].oid == oid )
      return &pVol->threadDir[i];
  }

  return 0;
}

bool
vol_AddThread(Volume *pVol, OID oid, ObCount allocCount, uint16_t rsrvNdx)
{
  ThreadDirent* tde = vol_LookupThread(pVol, oid);
  if (tde == 0) {
    if (pVol->maxThreadDirent == pVol->nThreadDirent)
      vol_GrowThreadDir(pVol);

    tde = &pVol->threadDir[pVol->nThreadDirent];
    pVol->nThreadDirent++;
  }

  tde->oid = oid;
  tde->allocCount = allocCount;
  tde->schedNdx = rsrvNdx;

  pVol->needSyncCkptLog = true;

  return true;
}

static void
vol_SyncHdr(Volume *pVol)
{
  char buf[EROS_PAGE_SIZE];
  int i;
  
  assert(pVol->working_fd >= 0);

  vol_Read(pVol, 0, buf, EROS_PAGE_SIZE);

  /* Recompute the image size from the division table: */
  pVol->volHdr.VolSectors = 0;
  for (i = 0; i < pVol->topDiv; i++) {
    if (pVol->divTable[i].end > pVol->volHdr.VolSectors)
      pVol->volHdr.VolSectors = pVol->divTable[i].end;
  }
  
  memcpy(buf, &pVol->volHdr, sizeof(pVol->volHdr));

  vol_Write(pVol, 0, buf, EROS_PAGE_SIZE);

  pVol->needSyncHdr = 0;
}

static void
vol_SyncCkptLog(Volume *pVol)
{
  uint32_t dirPgCount = 0;
  uint32_t startDirPage = 0;
  uint32_t curDirLid = 2*EROS_OBJECTS_PER_FRAME;
  uint8_t dirPage[EROS_PAGE_SIZE];
  CpuReserveInfo *cri = pVol->reserveTable;
  uint32_t residual = MAX_CPU_RESERVE;
  ReserveDirPage *rdp;
  ThreadDirent *tde;
  ThreadDirPage *tdp;
  CkptDirent *cpd;
  CkptDirPage *dp;

  if (!pVol->needSyncCkptLog)
    return;
  
  assert(pVol->working_fd >= 0);

  assert((pVol->maxCkptDirent % ckdp_maxDirEnt) == 0);
  assert((pVol->maxThreadDirent % tdp_maxDirEnt) == 0);

  /* Write out the reserve table: */
  startDirPage = dirPgCount;
  
  rdp = (ReserveDirPage*) dirPage;

  while (residual) {
    uint32_t count = residual;
    if (count > rdp_maxDirEnt)
      count = rdp_maxDirEnt;

    rdp->hdr.nDirent = count;
    
    memcpy(rdp->entry, cri, sizeof(CpuReserveInfo) * count);
    vol_WriteLogPage(pVol, curDirLid, dirPage);

    pVol->curDskCkpt->dirPage[dirPgCount] = curDirLid;

    curDirLid += EROS_OBJECTS_PER_FRAME;
    dirPgCount++;
    cri += count;
    residual -= count;
  }
	 
  pVol->curDskCkpt->hdr.nRsrvPage = dirPgCount - startDirPage;

  /* Write out the thread directory: */

   tde = pVol->threadDir;
  residual = pVol->nThreadDirent;
  startDirPage = dirPgCount;

  tdp = (ThreadDirPage*) dirPage;

  while (residual) {
    uint32_t count = residual;
    if (count > tdp_maxDirEnt)
      count = tdp_maxDirEnt;

    tdp->hdr.nDirent = count;

    memcpy(tdp->entry, tde, sizeof(ThreadDirent) * count);
    vol_WriteLogPage(pVol, curDirLid, dirPage);

    pVol->curDskCkpt->dirPage[dirPgCount] = curDirLid;

    curDirLid += EROS_OBJECTS_PER_FRAME;
    dirPgCount++;
    tde += count;
    residual -= count;
  }
  
  pVol->curDskCkpt->hdr.nThreadPage = dirPgCount - startDirPage;

  /* Write out the object directory: */

  cpd = pVol->ckptDir;
  residual = pVol->nCkptDirent;
  startDirPage = dirPgCount;

  dp = (CkptDirPage*) dirPage;

  while (residual) {
    uint32_t count = residual;
    if (count > ckdp_maxDirEnt)
      count = ckdp_maxDirEnt;

    dp->hdr.nDirent = count;

    memcpy(dp->entry, cpd, sizeof(CkptDirent) * count);
    vol_WriteLogPage(pVol, curDirLid, dirPage);

    pVol->curDskCkpt->dirPage[dirPgCount] = curDirLid;

    curDirLid += EROS_OBJECTS_PER_FRAME;
    dirPgCount++;
    cpd += count;
    residual -= count;
  }
  
  pVol->curDskCkpt->hdr.nDirPage = dirPgCount - startDirPage;

  if (pVol->curDskCkpt->hdr.nDirPage)
    pVol->curDskCkpt->hdr.hasMigrated = false;
  else
    pVol->curDskCkpt->hdr.hasMigrated = true;

  /* I don't know which one is current -- just rewrite them both. */
  vol_WriteLogPage(pVol, (lid_t)0, (uint8_t *) pVol->dskCkptHdr0);
  vol_WriteLogPage(pVol, (lid_t)1*EROS_OBJECTS_PER_FRAME, (uint8_t *) pVol->dskCkptHdr1);
  
  pVol->needSyncCkptLog = 0;
}

void
vol_ResetVolume(Volume *pVol)
{
  int i;

  /* Used by sysgen to ensure that writing new data on an existing
   * volume causes no problems.
   */

  for (i = 0; i < pVol->topDiv; i++) {
    if (pVol->divTable[i].type == dt_Object)
      vol_FormatObjectDivision(pVol, i);

    if (pVol->divTable[i].type == dt_Log)
      vol_FormatLogDivision(pVol, i);
  }
}

typedef struct PreloadedModule PreloadedModule;
struct PreloadedModule {
  PreloadedModule * next;
  OID startOid;
  char fname[FILENAME_MAX+1];
};

void
vol_Close(Volume *pVol)
{
  /* If we are exiting on an error, there is no need to save anything,
   * since the file has been deleted.
   */
  if (app_IsAborting())
    return;
  
  vol_InitDivisions(pVol);
  vol_SyncHdr(pVol);
  vol_SyncDivTables(pVol);
  vol_SyncCkptLog(pVol);

  if (pVol->grubDir) {
    /* Write files for Grub. */
    int fd;
    FILE * file;
    char configFname[FILENAME_MAX+1];
    PreloadedModule * moduleList = 0;
    PreloadedModule * pm;
    int div;

    for (div = 0; div < pVol->topDiv; div++) {
      if (    pVol->divTable[div].type == dt_Object
          && (pVol->divTable[div].flags & DF_PRELOAD)) {
        /* Write module file. */
        pm = (PreloadedModule *)malloc(sizeof(PreloadedModule));
        if (!pm) {
          diag_fatal(1, "Out of memory\n");
        } else {
          char indexStr[12];
  
          pm->next = moduleList;	/* link in */
          moduleList = pm;
          pm->startOid = pVol->divTable[div].startOid;
          strncpy(pm->fname, pVol->grubDir, FILENAME_MAX);
          strncat(pm->fname, "/CapROS-PL-", FILENAME_MAX-strlen(pm->fname));
          sprintf(indexStr, "%d", div);
          strncat(pm->fname, indexStr, FILENAME_MAX-strlen(pm->fname));
          strncat(pm->fname, "-", FILENAME_MAX-strlen(pm->fname));
          strncat(pm->fname, pVol->suffix, FILENAME_MAX-strlen(pm->fname));

          /* Write file for Grub. */
          fd = open(pm->fname, O_WRONLY | O_CREAT | O_TRUNC, 0744);
          if (fd < 0) {
            diag_fatal(1, "Can't open Grub file\n");
          } else {
            /* Copy the division to the file. */
            int j;
            for (j = pVol->divTable[div].start + EROS_PAGE_SECTORS;
                     /* Adding EROS_PAGE_SECTORS above because
                        we skip the ckpt seq number page. */
                 j < pVol->divTable[div].end; j++) {
              char buf[EROS_SECTOR_SIZE];
              if (vol_Read(pVol, EROS_SECTOR_SIZE * j,
                           buf, EROS_SECTOR_SIZE)) {
                write(fd, buf, EROS_SECTOR_SIZE);
              }
            }
            close(fd);
          }
        }
      }
      /* The kernel file is not written here.
         It's copied as an ELF file. */
    }
    /* Write Grub configuration file. */
    strncpy(configFname, pVol->grubDir, FILENAME_MAX);
    strncat(configFname, "/CapROS-config-", FILENAME_MAX-strlen(configFname));
    strncat(configFname, pVol->suffix, FILENAME_MAX-strlen(configFname));
    if (! keyBits_IsType(&pVol->volHdr.iplKey, KKT_Process)) {
      diag_fatal(1, "No IPL key.\n");
    } else {
      unsigned int nameSkip = strlen(pVol->grubDir);
      OID iplOid;

        iplOid = pVol->volHdr.iplKey.u.unprep.oid;
        file = fopen(configFname, "w");
        if (file == 0) {
          diag_fatal(1, "Can't open Grub file\n");
        } else {
          fputs("default=0\n", file);
          fputs("timeout=0\n", file);
          fputs("title CapROS\n", file);
          fputs("\troot (hd0,1)\n", file);	/* Hopefully, the /boot partition */
          fputs("\tkernel --type=multiboot ", file);
          fputs("/CapROS-kernel-", file);	/* kernel file name */
          fputs(pVol->suffix, file);
          fprintf(file, " 0x%08lx%08lx",
                  (uint32_t) (iplOid >> 32),
                  (uint32_t) iplOid);
          fprintf(file, " 0x%08lx", pVol->bootDrive);
          if (pVol->volHdr.BootFlags & VF_DEBUG) {
            fputs(" debug", file);
          }
          fputs("\n", file);

          /* Output module commands. */
          for (pm = moduleList; pm; pm = pm->next) {
            fputs("\tmodule ", file);
            fputs(pm->fname+nameSkip, file);
            fprintf(file, " 0x%08lx%08lx\n",
                    (uint32_t) (pm->startOid >> 32),
                    (uint32_t) pm->startOid);
          }
          fclose(file);
        }
    }
  }

  if (pVol->working_fd == -1)
    return;
  
  if (fsync(pVol->working_fd) == -1)
    diag_fatal(1, "Final fsync failed with errno %d\n", errno);
  
  if (vol_CompressTarget(pVol) < 0)
    diag_fatal(1, "Unable to compress target volume -- errno %d\n", errno);
  
  close(pVol->target_fd);
  if (pVol->target_fd != pVol->working_fd)
    close(pVol->working_fd);
  
  pVol->target_fd = -1;
  pVol->working_fd = -1;
#if defined(__linux__) && 0
  sync();
  sleep(5);			/* linux sync is busted - schedules */
				/* the writes but doesn't do them. */
  sync();
  sleep(5);			/* linux sync is busted - schedules */
				/* the writes but doesn't do them. */
#endif
}

static uint32_t
vol_GetLogFrameVolOffset(Volume *pVol, int ndx, OID loc)
{
  const Division *d = &pVol->divTable[ndx];
  uint32_t relPage;
  uint32_t divStart;
  uint32_t pageOffset;

  assert(div_contains(d, loc));

  relPage = (uint32_t) (loc - d->startOid);
  relPage /= EROS_OBJECTS_PER_FRAME;
  divStart = d->start * EROS_SECTOR_SIZE;
  pageOffset = relPage * EROS_PAGE_SIZE;

  return divStart + pageOffset;
}
    
/* Return true/false according to whether this is a valid object OID
 * given the frame type:
 */
static bool
vol_ValidOid(Volume *pVol, int ndx, OID oid)
{
  const Division *d = &pVol->divTable[ndx];
  VolPagePot frameInfo;
  uint32_t obOffset;

  assert(div_contains(d, oid));

  assert (d->type != dt_Log);

  obOffset = (uint32_t) (oid % EROS_OBJECTS_PER_FRAME);

  vol_GetPagePotInfo(pVol, oid, &frameInfo);
  
  switch (frameInfo.type) {
  case FRM_TYPE_ZDPAGE:
  case FRM_TYPE_DPAGE:
    return (obOffset == 0) ? true : false;

  case FRM_TYPE_NODE:
    return (obOffset < DISK_NODES_PER_PAGE) ? true : false;
  default:
    return false;
  }
}

/* given a division index and a OID, return the volume-relative
 * offset of the object in that division.
 */
static uint32_t
vol_GetOidVolOffset(Volume *pVol, int ndx, OID oid)
{
  const Division *d = &pVol->divTable[ndx];
  uint32_t frameStart;
  uint32_t obOffset;

  assert(div_contains(d, oid));

  assert (d->type != dt_Log);
  
  frameStart = vol_GetOidFrameVolOffset(pVol, ndx, oid);

  obOffset = (uint32_t) (oid % EROS_OBJECTS_PER_FRAME);

  if (obOffset) {
    VolPagePot frameInfo;
    vol_GetPagePotInfo(pVol, oid, &frameInfo);
  
    switch (frameInfo.type) {
    case FRM_TYPE_ZDPAGE:
    case FRM_TYPE_DPAGE:
      obOffset *= EROS_PAGE_SIZE;
      break;
    case FRM_TYPE_NODE:
      obOffset *= sizeof(DiskNodeStruct);
      break;
    default:
      diag_fatal(1, "Unknown object type tag!\n");
    }
  }

  if (obOffset >= EROS_PAGE_SIZE)
    diag_fatal(1, "Bad OID (gives improper offset)\n");
    
  return frameStart + obOffset;
}

bool
vol_GetPagePotInfo(Volume *pVol, OID oid, VolPagePot *pPot)
{
  CkptDirent* cpd = vol_LookupObject(pVol, oid);
    
  if (cpd) {
    pPot->count = cpd->count;
    pPot->type = cpd->type;

    return true;
  }
  
  return vol_ReadPagePotEntry(pVol, oid, pPot);
}

/* Object I/O support: */
bool
vol_ReadDataPage(Volume *pVol, OID oid, uint8_t *buf)
{
  VolPagePot pp;
  CkptDirent *cpd;
  int div;

  if (oid % EROS_OBJECTS_PER_FRAME) {
    diag_printf("Page OID must be multiple of %d (0x%x)\n",
		 EROS_OBJECTS_PER_FRAME,
		 EROS_OBJECTS_PER_FRAME);
    return false;
  }
  
  if ( vol_GetPagePotInfo(pVol, oid, &pp) == false ) {
    diag_printf("Requested OID invalid\n");
    return false;
  }

  switch(pp.type) {
  case FRM_TYPE_ZDPAGE:
    memset(buf, 0, EROS_PAGE_SIZE);
    return true;
  case FRM_TYPE_DPAGE:
    break;
  default:
    diag_printf("Requested OID not a page frame\n");
    return false;
  }
  
  cpd = vol_LookupObject(pVol, oid);
    
  if (cpd) {
    assert (CONTENT_LID(cpd->lid));
    assert (cpd->type != FRM_TYPE_ZDPAGE);
    
    if (vol_ReadLogPage(pVol, cpd->lid, buf) == false)
      return false;

    return true;
  }

  for (div = 0; div < pVol->topDiv; div++) {
    Division *d = &pVol->divTable[div];
    
    if (((d->type == dt_Object) || (d->type == dt_Kernel)) &&
	div_contains(d, oid)) {
      VolPagePot frameInfo;
      uint32_t offset;

      if (vol_ValidOid(pVol, div, oid) == false) {
	diag_printf("Requested OID invalid\n");
	return false;
      }
      
      vol_ReadPagePotEntry(pVol, oid, &frameInfo);

      if ((frameInfo.type != FRM_TYPE_DPAGE) &&
	  (frameInfo.type != FRM_TYPE_ZDPAGE)) {
	diag_printf("Non-page frame\n");
	return false;
      }
      
      offset = vol_GetOidVolOffset(pVol, div, oid);

      return vol_Read(pVol, offset, buf, EROS_PAGE_SIZE);
    }
  }

  return false;
}

bool
vol_WriteDataPage(Volume *pVol, OID oid, const uint8_t *buf)
{
  bool isZeroPage = true;
  CkptDirent *cpd;
  uint32_t i;
  int div;

  for (i = 0; i < EROS_PAGE_SIZE; i++)
    if (buf[i]) {
      isZeroPage = false;
      break;
    }
    
  cpd = vol_LookupObject(pVol, oid);
    
  if (cpd && cpd->type != FRM_TYPE_DPAGE && cpd->type != FRM_TYPE_ZDPAGE)
    return false;
  
  /* If a location has already been fabricated, write it there even if
   * it is a zero page.
   */
  if ( cpd && CONTENT_LID(cpd->lid) ) {
    if (vol_WriteLogPage(pVol, cpd->lid, buf) == false)
      return false;

    return true;
  }
  
  for (div = 0; div < pVol->topDiv; div++) {
    Division *d = &pVol->divTable[div];
    
    if (((d->type == dt_Object) || (d->type == dt_Kernel)) &&
	div_contains(d, oid)) {
      VolPagePot frameInfo;
      uint8_t wantTag;
      uint32_t offset;

      vol_ReadPagePotEntry(pVol, oid, &frameInfo);

      assert ((oid % EROS_OBJECTS_PER_FRAME) == 0);

      wantTag = isZeroPage ? FRM_TYPE_ZDPAGE : FRM_TYPE_DPAGE;
      if (frameInfo.type != wantTag) {
	diag_debug(1, "Re-tagging frame for OID 0x%08x%08x to %d\n",
		    (uint32_t) (oid >> 32),
		    (uint32_t) (oid),
		    wantTag);
	frameInfo.type = wantTag;
	vol_WritePagePotEntry(pVol, oid, &frameInfo);
      }

      offset = vol_GetOidVolOffset(pVol, div, oid);

      if ( !vol_Write(pVol, offset, buf, EROS_PAGE_SIZE) )
	diag_fatal(5, "Volume write failed at offset %d.\n", offset);

      return true;
    }
  }
  
  if (pVol->rewriting == false)
    return false;
  
  /* If we get here, we didn't find what we wanted.  Allocate space
   * for this page in the checkpoint log.
   */

  diag_fatal(1, "Cannot write page with OID 0x%08x%08x -- no home location\n",
	      (uint32_t)(oid>>32), (uint32_t)oid);
  
  {
    lid_t lid = ZERO_LID;
    uint8_t type = FRM_TYPE_ZDPAGE;

    if (isZeroPage == false) {
      lid = vol_AllocLogPage(pVol);
      type = FRM_TYPE_DPAGE;
  
      if (vol_WriteLogPage(pVol, lid, buf) == false)
	return false;
    }
  
    vol_AddDirent(pVol, oid, 0, lid, type);
  }

  return true;
}

/* Log I/O support: */
bool
vol_ReadLogPage(Volume *pVol, const lid_t lid, uint8_t *buf)
{
  int div;

  for (div = 0; div < pVol->topDiv; div++) {
    Division *d = &pVol->divTable[div];
    
    if (d->type == dt_Log && div_contains(d, lid)) {
      uint32_t offset = vol_GetLogFrameVolOffset(pVol, div, lid);

      return vol_Read(pVol, offset, buf, EROS_PAGE_SIZE);
    }
  }

  return false;
}

bool
vol_WriteLogPage(Volume *pVol, const lid_t lid, const uint8_t *buf)
{
  int div;

  for (div = 0; div < pVol->topDiv; div++) {
    Division *d = &pVol->divTable[div];
    
    if (d->type == dt_Log && div_contains(d, lid)) {
      uint32_t offset = vol_GetLogFrameVolOffset(pVol, div, lid);

      if ( !vol_Write(pVol, offset, buf, EROS_PAGE_SIZE) )
	diag_fatal(5, "Volume write failed at offset %d.\n", offset);
    }
  }

  return true;
}

/* given a division index and a OID, return the volume-relative
 * offset of the page pot for that OID
 */
static uint32_t
vol_GetOidPagePotVolOffset(Volume *pVol, int ndx, OID oid)
{
  const Division *d = &pVol->divTable[ndx];
  uint32_t relPage;
  uint32_t whichCluster;
  uint32_t pagePotFrame;
  uint32_t pageOffset;
  uint32_t divStart;

  assert(d->type == dt_Object);
  assert(div_contains(d, oid));
  
  assert ((d->startOid % EROS_OBJECTS_PER_FRAME) == 0);
  
  relPage = (uint32_t) ((oid - d->startOid) / EROS_OBJECTS_PER_FRAME);
  whichCluster = (uint32_t) (relPage / DATA_PAGES_PER_PAGE_CLUSTER);
  pagePotFrame = whichCluster * PAGES_PER_PAGE_CLUSTER;

  pagePotFrame ++;		/* for ckpt sequence number pg */

  pageOffset = pagePotFrame * EROS_PAGE_SIZE;
  divStart = d->start * EROS_SECTOR_SIZE;
  
  return divStart + pageOffset;
}

bool
vol_ReadPagePotEntry(Volume *pVol, OID oid, VolPagePot *pPagePot)
{
  int div;

  for (div = 0; div < pVol->topDiv; div++) {
    Division *d = &pVol->divTable[div];
    uint8_t data[EROS_PAGE_SIZE];
    
    if ( d->type == dt_Object && div_contains(d, oid) ) {
      bool result;
      uint32_t offset;
      uint32_t relPage;
      uint32_t potEntry;
      PagePot *pp;

      assert ((d->startOid % EROS_OBJECTS_PER_FRAME) == 0);

      offset = vol_GetOidPagePotVolOffset(pVol, div, oid);

      result = vol_Read(pVol, offset, data, EROS_PAGE_SIZE);
      if (result == false)
	return result;

      relPage = (uint32_t) ((oid - d->startOid) / EROS_OBJECTS_PER_FRAME);
      
      pp = (PagePot *) data;
      potEntry = relPage % DATA_PAGES_PER_PAGE_CLUSTER;
      pPagePot->type = pp->type[potEntry];
      pPagePot->count = pp->count[potEntry];

      return true;
    }
  }

  return false;
}

bool
vol_WritePagePotEntry(Volume *pVol, OID oid, const VolPagePot *pPagePot)
{
  int div;

  for (div = 0; div < pVol->topDiv; div++) {
    Division *d = &pVol->divTable[div];
    uint8_t data[EROS_PAGE_SIZE];
    
    if ( d->type == dt_Object && div_contains(d, oid) ) {
      bool result;
      uint32_t offset;
      uint32_t relPage;
      PagePot *pp;
      uint32_t potEntry;

      assert ((d->startOid % EROS_OBJECTS_PER_FRAME) == 0);

      offset = vol_GetOidPagePotVolOffset(pVol, div, oid);

      result = vol_Read(pVol, offset, data, EROS_PAGE_SIZE);
      if (result == false)
	return result;

      relPage = (uint32_t) ((oid - d->startOid) / EROS_OBJECTS_PER_FRAME);
      
      pp = (PagePot *) data;
      potEntry = relPage % DATA_PAGES_PER_PAGE_CLUSTER;
      pp->type[potEntry] = pPagePot->type;
      pp->count[potEntry] = pPagePot->count;

      if ( !vol_Write(pVol, offset, data, EROS_PAGE_SIZE) )
	diag_fatal(5, "Volume write failed at offset %d.\n", offset);

      return true;
    }
  }

  return false;
}

bool
vol_ReadNode(Volume *pVol, OID oid, DiskNodeStruct *pNode)
{
  int div;
  CkptDirent* ccd = vol_LookupObject(pVol, oid);
    
  if (ccd && ccd->type == FRM_TYPE_NODE && ccd->lid != UNDEF_LID) {
    uint32_t i;
    uint8_t logPage[EROS_PAGE_SIZE];
    DiskNodeStruct* logPot = (DiskNodeStruct *) logPage;

    assert (CONTENT_LID(ccd->lid));
    
    if (vol_ReadLogPage(pVol, ccd->lid, logPage) == false)
      return false;

    for (i = 0; i < DISK_NODES_PER_PAGE; i++) {
      if (logPot[i].oid == oid) {
	memcpy(pNode, &logPot[i], sizeof(DiskNodeStruct));
	return true;
      }
    }

    return false;
  }
  
  for (div = 0; div < pVol->topDiv; div++) {
    Division *d = &pVol->divTable[div];
    
    if (d->type == dt_Object && div_contains(d, oid)) {
      uint32_t offset;
      VolPagePot frameInfo;

      if (vol_ValidOid(pVol, div, oid) == false) {
	diag_printf("Requested OID invalid\n");
	return false;
      }
      
      vol_ReadPagePotEntry(pVol, oid, &frameInfo);

      if (frameInfo.type != FRM_TYPE_NODE) {
	diag_printf("Non-node frame\n");
	return false;
      }
      
      offset = vol_GetOidVolOffset(pVol, div, oid);

      return vol_Read(pVol, offset, pNode, sizeof(DiskNodeStruct));
    }
  }

  return false;
}

bool
vol_WriteNodeToLog(Volume *pVol, OID oid, const DiskNodeStruct *pNode)
{
  uint8_t logPage[EROS_PAGE_SIZE];
  DiskNodeStruct* logPot = (DiskNodeStruct *) logPage;
  uint8_t ndx;

  assert (oid == pNode->oid);
  
  if (pVol->curLogPotLid == UNDEF_LID) {
    pVol->curLogPotLid = vol_AllocLogPage(pVol);
  }

  if (vol_ReadLogPage(pVol, pVol->curLogPotLid, logPage) == false)
    return false;

  ndx = pVol->curLogPotLid % EROS_OBJECTS_PER_FRAME;
  
  memcpy(&logPot[ndx], pNode, sizeof(DiskNodeStruct));

  if (vol_WriteLogPage(pVol, pVol->curLogPotLid, logPage) == false)
    return false;

  vol_AddDirent(pVol, oid, 0, pVol->curLogPotLid, FRM_TYPE_NODE);
  pVol->curLogPotLid++;
  
  if (pVol->curLogPotLid % EROS_OBJECTS_PER_FRAME == DISK_NODES_PER_PAGE)
    pVol->curLogPotLid = UNDEF_LID;

  return true;
}

static bool
vol_FormatNodeFrame(Volume *pVol, int div, OID frameOID)
{
  char buf[EROS_PAGE_SIZE];
  DiskNodeStruct *pdn = (DiskNodeStruct *) buf;
  int nd;
  uint32_t offset;

  bzero (buf, EROS_PAGE_SIZE);
	
  for (nd = 0; nd < EROS_NODES_PER_FRAME; nd++) {
    uint32_t slot;

    pdn[nd].oid = frameOID + nd;
    for (slot = 0; slot < EROS_NODE_SIZE; slot++)
      keyBits_InitToVoid(&pdn[nd].slot[slot]);
  }

  offset = vol_GetOidVolOffset(pVol, div, frameOID);

  return vol_Write(pVol, offset, buf, EROS_PAGE_SIZE);
}

bool
vol_WriteNode(Volume *pVol, OID oid, const DiskNodeStruct *pNode)
{
  CkptDirent* ccd = vol_LookupObject(pVol, oid);
  int div;
    
  if (ccd && ccd->type != FRM_TYPE_NODE)
    return false;
  
  if (ccd && CONTENT_LID(ccd->lid)) {
    uint8_t logPage[EROS_PAGE_SIZE];
    DiskNodeStruct* logPot = (DiskNodeStruct *) logPage;
    uint32_t i;

    if (vol_ReadLogPage(pVol, ccd->lid, logPage) == false)
      return false;

    for (i = 0; i < DISK_NODES_PER_PAGE; i++) {
      if (logPot[i].oid == oid) {
	memcpy(&logPot[i], pNode, sizeof(DiskNodeStruct));

	if (vol_WriteLogPage(pVol, ccd->lid, logPage) == false)
	  return false;

	return true;
      }
    }

    return false;
  }
  
  for (div = 0; div < pVol->topDiv; div++) {
    Division *d = &pVol->divTable[div];
    
    if (d->type == dt_Object && div_contains(d, oid)) {
      VolPagePot frameInfo;
      uint32_t offset;

      vol_ReadPagePotEntry(pVol, oid, &frameInfo);

      if (frameInfo.type != FRM_TYPE_NODE) {
	OID frameOID = oid & ~(EROS_OBJECTS_PER_FRAME - 1u);

	diag_debug(1, "Re-tagging frame for OID 0x%08x%08x to %d\n",
		    (uint32_t) (oid >> 32),
		    (uint32_t) (oid),
		    FRM_TYPE_NODE);

	if ( ! vol_FormatNodeFrame(pVol, div, frameOID) )
	  diag_fatal(5, "Unable to format node frame 0x%08x%08x\n",
		      (uint32_t) (frameOID >> 32),
		      (uint32_t) frameOID);

	frameInfo.type = FRM_TYPE_NODE;
	vol_WritePagePotEntry(pVol, oid, &frameInfo);
      }

      offset = vol_GetOidVolOffset(pVol, div, oid);

      if ( !vol_Write(pVol, offset, pNode, sizeof(DiskNodeStruct)) )
	diag_fatal(5, "Volume write failed at offset %d.\n", offset);

      return true;
    }
  }

  if (pVol->rewriting == false)
    return false;
  
  /* If we get here, we didn't find what we wanted.  Allocate space
   * for this node in the checkpoint log.
   */

  diag_fatal(1, "Cannot write node with OID 0x%08x%08x -- no home location\n",
	      (uint32_t)(oid>>32), (uint32_t)oid);
  if ( !vol_WriteNodeToLog(pVol, oid, pNode) )
    diag_fatal(5, "Volume write to log failed\n");

  return true;
}

	
bool
vol_ContainsNode(Volume *pVol, OID oid)
{
  CkptDirent* ccd = vol_LookupObject(pVol, oid);
  VolPagePot frameInfo;

  if (ccd && ccd->type == FRM_TYPE_NODE)
    return true;
  
  if (vol_ReadPagePotEntry(pVol, oid, &frameInfo) == false)
    return false;
  
  if (frameInfo.type != FRM_TYPE_NODE)
    return false;
  
  return true;
}

bool
vol_ContainsPage(Volume *pVol, OID oid)
{
  CkptDirent* ccd = vol_LookupObject(pVol, oid);
  VolPagePot frameInfo;

  if (ccd && (ccd->type == FRM_TYPE_DPAGE || ccd->type == FRM_TYPE_ZDPAGE))
    return true;
  
  if (vol_ReadPagePotEntry(pVol, oid, &frameInfo) == false)
    return false;
  
  if (frameInfo.type != FRM_TYPE_DPAGE && frameInfo.type != FRM_TYPE_ZDPAGE)
    return false;
  
  return true;
}
