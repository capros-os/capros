/*
 * Copyright (C) 1998, 1999, 2001, Jonathan S. Shapiro.
 * Copyright (C) 2005, 2006, 2007, 2008, 2009, Strawberry Development Group.
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

#include <disk/TagPot.h>
#include <disk/CkptRoot.h>
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
  OID startOid = get_target_oid(&d->startOid);
  uint32_t relPage;
  uint32_t divStart;
  uint32_t pageOffset;

  assert(div_contains(d, oid));

  assert ((startOid % EROS_OBJECTS_PER_FRAME) == 0);

  relPage = FrameToRangeLoc(OIDToFrame(oid - startOid));

  divStart = d->start * EROS_SECTOR_SIZE;
  pageOffset = relPage * EROS_PAGE_SIZE;

  return divStart + pageOffset;
}

static void
vol_InitVolume(Volume *pVol)
{
  pVol->working_fd = -1;
  pVol->target_fd = -1;
  pVol->topDiv = 0;
  pVol->topLogLid = 0;
  pVol->lastAvailLogLid = 0;
  pVol->firstAvailLogLid = (2 * EROS_OBJECTS_PER_FRAME);

  pVol->dskCkptHdr0 = (CkptRoot *) malloc(EROS_PAGE_SIZE);
  pVol->dskCkptHdr1 = (CkptRoot *) malloc(EROS_PAGE_SIZE);
  pVol->curDskCkpt = pVol->dskCkptHdr0;
  pVol->oldDskCkpt = pVol->dskCkptHdr1;
  
  pVol->rewriting = true;

#if 0 // this is not working now ...
  pVol->ckptDir = 0;
  pVol->maxCkptDirent = 0;
  pVol->nCkptDirent = 0;

  pVol->threadDir = 0;
  pVol->maxThreadDirent = 0;
  pVol->nThreadDirent = 0;
#endif

  pVol->needSyncDivisions = false;
  pVol->needSyncHdr = false;
  pVol->needSyncCkptLog = false;
  pVol->needDivInit = false;
}

static void
vol_Init(Volume *pVol)
{
  vol_InitVolume(pVol);
}

static bool
vol_Read(Volume *pVol, uint32_t offset, void *buf, uint32_t sz)
{
  int e;

  assert(pVol->working_fd >= 0);

  e = lseek(pVol->working_fd, (int) offset, SEEK_SET);
  if (e < 0) {
    diag_debug(0, "lseek got %d\n", e);
    return false;
  }

  e = read(pVol->working_fd, buf, (int) sz);
  if (e != (int) sz) {
    diag_debug(0, "read, offset=%d, size=%d, got %d\n", offset, sz, e);
    return false;
  }

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

    /* Must have at least space for one pot and one frame. */
    if (nObFrames < 2)
      diag_fatal(1, "Range must have at least 3 frames.\n");

    /* Take out a pot for each whole or partial cluster. */
    nObFrames -= (nObFrames + (RangeLocsPerCluster-1)) / RangeLocsPerCluster;

    endOid = oid + (nObFrames * EROS_OBJECTS_PER_FRAME);
    break;

  case dt_Log:
    endOid = oid + (nObFrames * EROS_OBJECTS_PER_FRAME);
    break;

  default:
    diag_fatal(1, "Attempt to set OID on inappropriate division type\n");
    break;
  }
  
  if (oid % EROS_OBJECTS_PER_FRAME)
    diag_fatal(1, "Starting OID for range must be multiple of %d\n",
		EROS_OBJECTS_PER_FRAME);
    
  div = vol_AddAdjustableDivision(pVol, type, sz);
  
  if (type == dt_Log && endOid > pVol->topLogLid) {
    pVol->topLogLid = endOid;
    pVol->lastAvailLogLid = endOid;
  }
    
  put_target_oid(&pVol->divTable[div].startOid, oid);
  put_target_oid(&pVol->divTable[div].endOid, endOid);

  diag_printf("Division %d: %6d %6d [%6s] %s=[",
	       div, pVol->divTable[div].start, pVol->divTable[div].end,
	       div_TypeName(pVol->divTable[div].type),
	       (type == dt_Log) ? "LID" : "OID");
  diag_printOid(oid);
  diag_printf(", ");
  diag_printOid(endOid);
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
  put_target_oid(&pVol->divTable[div].startOid, 0);
  put_target_oid(&pVol->divTable[div].endOid, 0);
  
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

  pVol->needSyncHdr = 1;	/* if nothing else, volume size changed */
  
  pVol->divNeedsInit[div] = true;
  pVol->needDivInit = true;

  pVol->needSyncDivisions = true;
  
  return div;
}

// Select a free location on the volume for this division.
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

  for (oid = get_target_oid(&d->startOid);
       oid < get_target_oid(&d->endOid);
       oid += EROS_OBJECTS_PER_FRAME) {
    VolPagePot pagePot;
    pagePot.type = FRM_TYPE_DPAGE;
    pagePot.isZero = TagIsZero;
    pagePot.count = 0;
    vol_WritePagePotEntry(pVol, oid, &pagePot);
  }
}

static void
InitCkptRoot(Volume * pVol, CkptRoot * root, LID lid)
{
  int i;

  memset(root, 0, EROS_PAGE_SIZE);	// clear out any cruft

  root->versionNumber = CkptRootVersion;
  root->maxNPCount = 0;
  put_target_u64(&root->checkGenNum, 0);	// means no ckpt
  put_target_u64(&root->mostRecentGenerationNumber, 0);	// means no ckpt
  put_target_lid(&root->endLog, UNUSED_LID);
  for (i = 0; i < MaxUnmigratedGenerations; i++) {
    put_target_lid(&root->generations[i], UNUSED_LID);
  }
  root->integrityByte = IntegrityByteValue;

  vol_WriteLogPage(pVol, lid, root);
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
  
  if (get_target_oid(&d->startOid) == 0) {
    InitCkptRoot(pVol, pVol->dskCkptHdr0, CKPT_ROOT_0);
    InitCkptRoot(pVol, pVol->dskCkptHdr1, CKPT_ROOT_1);
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
  put_target_u64(&pVol->volHdr.iplSysId, dw);

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
  oid = get_target_oid(&d->startOid);

  divPages = (get_target_oid(&d->endOid) - oid) / EROS_OBJECTS_PER_FRAME;
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
    if ( !xi_SetImage(bootImage, bootName, 0, 0) )
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
#if 0 // this is not working now ...
  uint32_t i;

  for (i = 0; i < pVol->nCkptDirent; i++) {
    if ( pVol->ckptDir[i].oid == oid )
      return &pVol->ckptDir[i];
  }
#endif

  return 0;
}

#if 0 // this is not working now ...
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
#endif

static void
vol_LoadLogHeaders(Volume *pVol)
{
  vol_ReadLogPage(pVol, CKPT_ROOT_0, pVol->dskCkptHdr0);
  vol_ReadLogPage(pVol, CKPT_ROOT_1, pVol->dskCkptHdr1);

  if (get_target_u64(&pVol->dskCkptHdr0->mostRecentGenerationNumber) > 
      get_target_u64(&pVol->dskCkptHdr1->mostRecentGenerationNumber) ) {
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
#if 0	// This is not working now ...
  uint32_t logEnt = 0;
  uint32_t d;

  assert(pVol->curDskCkpt);

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
#endif
}

static void
vol_SyncDivTables(Volume *pVol)
{
  int div;

  if (!pVol->needSyncDivisions)
    return;
  
  diag_printf("Syncing div tables ...\n");
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
      diag_printf("Zeroing division ...\n");
      vol_ZeroDivision(pVol, i);

      switch (pVol->divTable[i].type) {
      case dt_Object:
      case dt_Kernel:
        diag_printf("Formating object division ...\n");
	vol_FormatObjectDivision(pVol, i);
        break;

      case dt_Log:
        diag_printf("Formating log division ...\n");
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
  vol_SetIplSysId(pVol, time(NULL));
			/* get a random value, hopefully unique  */
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
    diag_fatal(3, "Couldn't open target file.\n");
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

    LID endLid = get_target_lid(&pVol->divTable[i].endOid);
    if (pVol->divTable[i].type == dt_Log &&
	endLid > pVol->topLogLid) {
      pVol->topLogLid = endLid;
      pVol->lastAvailLogLid = endLid;
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

#if 0 // this is not working now ...
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
#endif

static void
vol_SyncHdr(Volume *pVol)
{
  char buf[EROS_PAGE_SIZE];
  int i;
  
  if (! pVol->needSyncHdr)
    return;

  diag_printf("Syncing header ...\n");
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
#if 0	// this is not working now ...
  uint32_t dirPgCount = 0;
  uint32_t startDirPage = 0;
  uint32_t curDirLid = 2*EROS_OBJECTS_PER_FRAME;
  uint8_t dirPage[EROS_PAGE_SIZE];
  CpuReserveInfo *cri = pVol->reserveTable;
  uint32_t residual = MAX_CPU_RESERVE;
  ThreadDirent *tde;
  ThreadDirPage *tdp;
  CkptDirent *cpd;
  CkptDirPage *dp;

  if (!pVol->needSyncCkptLog)
    return;
  
  diag_printf("Syncing ckpt log ...\n");
  assert(pVol->working_fd >= 0);

  assert((pVol->maxCkptDirent % ckdp_maxDirEnt) == 0);
  assert((pVol->maxThreadDirent % tdp_maxDirEnt) == 0);

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
  vol_WriteLogPage(pVol, CKPT_ROOT_0, pVol->dskCkptHdr0);
  vol_WriteLogPage(pVol, CKPT_ROOT_1, pVol->dskCkptHdr1);
  
  pVol->needSyncCkptLog = 0;
#endif
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
    diag_fatal(1, "Grub options no longer supported\n");
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

  relPage = (uint32_t) (loc - get_target_oid(&d->startOid));
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
  case FRM_TYPE_DPAGE:
    return (obOffset == 0);

  case FRM_TYPE_NODE:
    return (obOffset < DISK_NODES_PER_PAGE);
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
    case FRM_TYPE_DPAGE:
      obOffset *= EROS_PAGE_SIZE;
      break;
    case FRM_TYPE_NODE:
      obOffset *= sizeof(DiskNode);
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
    pPot->count = cpd->allocCount;
    pPot->type = cpd->type;
    pPot->isZero = CONTENT_LID(GetDiskObjectDescriptorLogLoc(cpd)) ? 0 : TagIsZero;

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

  if (pp.type != FRM_TYPE_DPAGE) {
    diag_printf("Requested OID not a page frame\n");
    return false;
  }

  if (pp.isZero) {
    memset(buf, 0, EROS_PAGE_SIZE);
    return true;
  }

  cpd = vol_LookupObject(pVol, oid);
    
  if (cpd) {
    LID lid = GetDiskObjectDescriptorLogLoc(cpd);
    assert (CONTENT_LID(lid));
    return vol_ReadLogPage(pVol, lid, buf);
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

      if (frameInfo.type != FRM_TYPE_DPAGE) {
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
    
  if (cpd && cpd->type != FRM_TYPE_DPAGE)
    return false;
  
  /* If a location has already been fabricated, write it there even if
   * it is a zero page.
   */
  LID lid = GetDiskObjectDescriptorLogLoc(cpd);
  if ( cpd && CONTENT_LID(lid) ) {
    return vol_WriteLogPage(pVol, lid, buf);
  }
  
  for (div = 0; div < pVol->topDiv; div++) {
    Division *d = &pVol->divTable[div];
    
    if (((d->type == dt_Object) || (d->type == dt_Kernel)) &&
	div_contains(d, oid)) {
      VolPagePot frameInfo;
      uint32_t offset;

      vol_ReadPagePotEntry(pVol, oid, &frameInfo);

      assert ((oid % EROS_OBJECTS_PER_FRAME) == 0);

      uint8_t zeroTag = isZeroPage ? TagIsZero : 0;
      if (frameInfo.type != FRM_TYPE_DPAGE
          || frameInfo.isZero != zeroTag) {
	diag_debug(1, "Re-tagging frame for OID %#llx to %d page\n", oid,
		   zeroTag);
	frameInfo.type = FRM_TYPE_DPAGE;
	frameInfo.isZero = zeroTag;
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
  
  diag_fatal(1, "Cannot write page with OID %#llx -- no home location\n",
	      oid);
  
  return false;
}

/* Log I/O support: */
bool
vol_ReadLogPage(Volume * pVol, const LID lid, void * buf)
{
  int div;

  for (div = 0; div < pVol->topDiv; div++) {
    Division *d = &pVol->divTable[div];
    
    if (d->type == dt_Log && div_contains(d, lid)) {
      uint32_t offset = vol_GetLogFrameVolOffset(pVol, div, lid);

      return vol_Read(pVol, offset, (uint8_t *)buf, EROS_PAGE_SIZE);
    }
  }

  return false;
}

bool
vol_WriteLogPage(Volume *pVol, const LID lid, const void * buf)
{
  int div;

  for (div = 0; div < pVol->topDiv; div++) {
    Division *d = &pVol->divTable[div];
    
    if (d->type == dt_Log && div_contains(d, lid)) {
      uint32_t offset = vol_GetLogFrameVolOffset(pVol, div, lid);

      if ( !vol_Write(pVol, offset, (const uint8_t *)buf, EROS_PAGE_SIZE) )
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
  OID startOid = get_target_oid(&d->startOid);
  uint32_t pagePotFrame;
  uint32_t pageOffset;
  uint32_t divStart;

  assert(d->type == dt_Object);
  assert(div_contains(d, oid));
  
  assert(OIDToObIndex(startOid) == 0);
  
  pagePotFrame = FrameToCluster(OIDToFrame(oid - startOid))
                 * RangeLocsPerCluster;

  pageOffset = pagePotFrame * EROS_PAGE_SIZE;
  divStart = d->start * EROS_SECTOR_SIZE;
  
  return divStart + pageOffset;
}

static int
vol_FindPagePotEntry(Volume *pVol, OID oid,
  uint8_t data[EROS_PAGE_SIZE], uint32_t * pOffset)
{
  int div;

  for (div = 0; div < pVol->topDiv; div++) {
    Division *d = &pVol->divTable[div];
    OID startOid = get_target_oid(&d->startOid);
    
    if ( d->type == dt_Object && div_contains(d, oid) ) {
      assert((startOid % EROS_OBJECTS_PER_FRAME) == 0);

      uint32_t offset = vol_GetOidPagePotVolOffset(pVol, div, oid);

      bool result = vol_Read(pVol, offset, data, EROS_PAGE_SIZE);
      if (! result)
	return -1;

      *pOffset = offset;
      return FrameIndexInCluster(OIDToFrame(oid - startOid));
    }
  }

  return -1;
}

bool
vol_ReadPagePotEntry(Volume *pVol, OID oid, VolPagePot *pPagePot)
{
  uint32_t offset;
  uint8_t data[EROS_PAGE_SIZE];
  int potEntry = vol_FindPagePotEntry(pVol, oid, data, &offset);
  if (potEntry >= 0) {
    TagPot * tp = (TagPot *)data;
    pPagePot->type = tp->tags[potEntry] & TagTypeMask;
    pPagePot->isZero = tp->tags[potEntry] & TagIsZero;
    pPagePot->count = tp->count[potEntry];

    return true;
  } else {
    return false;
  }
}

bool
vol_WritePagePotEntry(Volume *pVol, OID oid, const VolPagePot *pPagePot)
{
  uint32_t offset;
  uint8_t data[EROS_PAGE_SIZE];
  int potEntry = vol_FindPagePotEntry(pVol, oid, data, &offset);
  if (potEntry >= 0) {
    TagPot * tp = (TagPot *)data;
    tp->tags[potEntry] = pPagePot->type | pPagePot->isZero;
    tp->count[potEntry] = pPagePot->count;

    if ( !vol_Write(pVol, offset, data, EROS_PAGE_SIZE) )
      diag_fatal(5, "Volume write failed at offset %d.\n", offset);

    return true;
  } else {
    return false;
  }
}

bool
vol_ReadNode(Volume * pVol, OID oid, DiskNode * pNode)
{
  int div;
  CkptDirent* ccd = vol_LookupObject(pVol, oid);
    
  if (ccd && ccd->type == FRM_TYPE_NODE) {
    LID lid = GetDiskObjectDescriptorLogLoc(ccd);
    uint32_t i;
    uint8_t logPage[EROS_PAGE_SIZE];
    DiskNode * logPot = (DiskNode *) logPage;

    assert(CONTENT_LID(lid));
    
    if (vol_ReadLogPage(pVol, lid, logPage) == false)
      return false;

    for (i = 0; i < DISK_NODES_PER_PAGE; i++) {
      if (get_target_oid(&logPot[i].oid) == oid) {
	memcpy(pNode, &logPot[i], sizeof(DiskNode));
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

      return vol_Read(pVol, offset, pNode, sizeof(DiskNode));
    }
  }

  return false;
}

static bool
vol_FormatNodeFrame(Volume *pVol, int div, OID frameOID)
{
  char buf[EROS_PAGE_SIZE];
  DiskNode * pdn = (DiskNode *) buf;
  int nd;
  uint32_t offset;

  bzero (buf, EROS_PAGE_SIZE);
	
  for (nd = 0; nd < DISK_NODES_PER_PAGE; nd++) {
    init_DiskNodeKeys(&pdn[nd]);
    put_target_oid(&pdn[nd].oid, frameOID + nd);
  }

  offset = vol_GetOidVolOffset(pVol, div, frameOID);

  return vol_Write(pVol, offset, buf, EROS_PAGE_SIZE);
}

bool
vol_WriteNode(Volume *pVol, OID oid, const DiskNode * pNode)
{
  CkptDirent* ccd = vol_LookupObject(pVol, oid);
  LID lid = GetDiskObjectDescriptorLogLoc(ccd);
  int div;
    
  if (ccd && ccd->type != FRM_TYPE_NODE)
    return false;
  
  if (ccd && CONTENT_LID(lid)) {
    uint8_t logPage[EROS_PAGE_SIZE];
    DiskNode * logPot = (DiskNode *) logPage;
    uint32_t i;

    if (vol_ReadLogPage(pVol, lid, logPage) == false)
      return false;

    for (i = 0; i < DISK_NODES_PER_PAGE; i++) {
      if (get_target_oid(&logPot[i].oid) == oid) {
	memcpy(&logPot[i], pNode, sizeof(DiskNode));

	return vol_WriteLogPage(pVol, lid, logPage);
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

      if ( !vol_Write(pVol, offset, pNode, sizeof(DiskNode)) )
	diag_fatal(5, "Volume write failed at offset %d.\n", offset);

      return true;
    }
  }

  if (pVol->rewriting == false)
    return false;
  
  diag_fatal(1, "Cannot write node with OID 0x%08x%08x -- no home location\n",
	      (uint32_t)(oid>>32), (uint32_t)oid);

  return false;
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

  if (ccd && (ccd->type == FRM_TYPE_DPAGE))
    return true;
  
  if (vol_ReadPagePotEntry(pVol, oid, &frameInfo) == false)
    return false;
  
  if (frameInfo.type != FRM_TYPE_DPAGE)
    return false;
  
  return true;
}
