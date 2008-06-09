#ifndef __VOLUME_H__
#define __VOLUME_H__
/*
 * Copyright (C) 1998, 1999, 2001, Jonathan S. Shapiro.
 * Copyright (C) 2008, Strawberry Development Group.
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

#include <disk/LowVolume.h>
#include <disk/ErosTypes.h>
#include <disk/DiskNodeStruct.h>
#include <erosimg/DiskCkpt.h>
#include <erosimg/Intern.h>
#include <eros/Reserve.h>

/* EROS volume structure.  EROS volumes contain:
 * 
 * 	1. A Boot Page		(Page 0: must be error free)
 * 	2. A Division Table	(Page 1: must be error free)
 * 	3. A spare sector region
 * 	3. Other divisions as defined by the user
 */

/* In principle, these classes probably ought to live in distinct
 * header files.  In practice, there is essentially nothing you can do
 * with a volume unless you have the division table and the bad map
 * table.  Rather than keep the relationships in multiple files, it
 * seemed better to put them all in one place.
 */

struct ExecImage;
struct DiskCheckpoint;
struct CkptDirent;

typedef struct VolPagePot VolPagePot;
struct VolPagePot {
  ObCount count;
  uint8_t type;
  uint8_t allocCountUsed;	// TagAllocCountUsedMask or 0
};
  
typedef struct Volume Volume;
struct Volume {
  int working_fd;	/* If target file is compressed, this is the
			file descriptor of a temporary file containing
			an uncompressed copy of the target file.
			Otherwise this is the same as target_fd. */
  int target_fd;
  VolHdr volHdr;
  int needSyncHdr;
  int needSyncCkptLog;

  Division	divTable[NDIVENT];
  int topDiv;

  lid_t topLogLid;
  lid_t lastAvailLogLid;
  lid_t firstAvailLogLid;

  /* Managing the volume's checkpoint directory is a pain in the
   * butt.  The current logic works fine for creating new volumes and
   * for post-morten examination of existing volumes, and can be used
   * to rewrite the content of an existing page or cgroup on an
   * existing volume.  Unless we want to do a lot more bookkeeping,
   * though, adding a new cgroup or page to an existing ckpt log won't
   * work.
   */

  bool rewriting;

  const char * grubDir;	/* directory to write Grub files into */
  const char * suffix;	/* suffix for Grub files */
  uint32_t bootDrive;	/* boot drive for kernel command, for Grub */

  DiskCheckpoint *dskCkptHdr0;
  DiskCheckpoint *dskCkptHdr1;

  DiskCheckpoint *curDskCkpt;
  DiskCheckpoint *oldDskCkpt;
  
  struct CpuReserveInfo *reserveTable;
  
  ThreadDirent* threadDir;
  uint32_t nThreadDirent;
  uint32_t maxThreadDirent;
  
  CkptDirent* ckptDir;
  uint32_t maxCkptDirent;
  uint32_t nCkptDirent;

  lid_t curLogPotLid;

  bool	divNeedsInit[NDIVENT];
  bool  needDivInit;
  bool  needSyncDivisions;
};

#ifdef __cplusplus
extern "C" {
#endif

int vol_CompressTarget(Volume *);

void vol_WriteBootImage(Volume *, const char*);

int vol_AddAdjustableDivision(Volume *, DivType, uint32_t sz);
int vol_AddFixedDivision(Volume *, DivType, uint32_t start, uint32_t sz);

/* offset is in bytes relative to start of division */
void vol_WriteImageAtDivisionOffset(Volume *, int div, const struct ExecImage *pImage,
				    uint32_t offset);

bool vol_WriteNodeToLog(Volume *, OID oid, const DiskNodeStruct *pNode);

Volume *vol_Create(const char* filename, const char* bootImage);
Volume *vol_Open(const char* filename, bool forRewriting,
                 const char * grubDir, const char * suffix,
                 uint32_t bootDrive);
void vol_Close(Volume *);

/* For use by sysgen: */
void vol_ResetVolume(Volume *);

/* Division management logic: */
int vol_AddDivision(Volume *, DivType, uint32_t sz);
int vol_AddDivisionWithOid(Volume *, DivType, uint32_t sz, 
			   OID startOid);
void vol_DelDivision(Volume *, int ndx);
uint32_t vol_DivisionSetFlags(Volume *, int ndx, uint32_t flags);
uint32_t vol_DivisionClearFlags(Volume *, int ndx, uint32_t flags);
uint32_t vol_GetDivisionFlags(Volume *, int ndx);

void vol_WriteKernelImage(Volume *, int div, const struct ExecImage *pImage);

INLINE
int
vol_MaxDiv(Volume *pVolume)
{
  return pVolume->topDiv; 
}

INLINE
const Division *vol_GetDivision(Volume *pVolume, int i)
{
  return &pVolume->divTable[i]; 
}

INLINE
const VolHdr *vol_GetVolHdr(Volume *pVolume)
{
  return &pVolume->volHdr; 
}

bool vol_ReadLogPage(Volume *, const lid_t lid, uint8_t* buf);
bool vol_WriteLogPage(Volume *, const lid_t lid, const uint8_t* buf);
/* object I/O.  All of this assumes allocation/call count of 0! */
bool vol_ReadDataPage(Volume *, OID oid, uint8_t* buf);
bool vol_WriteDataPage(Volume *, OID oid, const uint8_t* buf);
bool vol_ReadNode(Volume *, OID oid, DiskNodeStruct *node);
bool vol_WriteNode(Volume *, OID oid, const DiskNodeStruct *node);
bool vol_GetPagePotInfo(Volume *, OID oid, VolPagePot *);
bool vol_ReadPagePotEntry(Volume *, OID oid, VolPagePot *);
bool vol_WritePagePotEntry(Volume *, OID oid, const VolPagePot *);
bool vol_ContainsPage(Volume *, OID oid);
bool vol_ContainsNode(Volume *, OID oid);
void vol_SetVolFlag(Volume *, VolHdrFlags);
void vol_ClearVolFlag(Volume *, VolHdrFlags);
void vol_SetIplSysId(Volume *, uint64_t dw);
bool vol_AddThread(Volume *, OID oid, ObCount count, uint16_t rsrvNdx);
void vol_SetReserve(Volume *, const CpuReserveInfo *rsrv);
const CpuReserveInfo *vol_GetReserve(const Volume *, uint32_t ndx);

INLINE uint32_t 
vol_NumDirent(const Volume *pVolume)
{
  return pVolume->nCkptDirent;
}

INLINE uint32_t 
vol_NumThread(const Volume *pVolume)
{
  return pVolume->nThreadDirent;
}

INLINE uint32_t 
vol_NumReserve(const Volume *pVolume)
{
  return MAX_CPU_RESERVE;
}

INLINE
CkptDirent 
vol_GetDirent(const Volume *pVolume, uint32_t ndx)
{
  return pVolume->ckptDir[ndx];
}

INLINE
ThreadDirent
vol_GetThread(const Volume *pVolume, uint32_t ndx)
{
  return pVolume->threadDir[ndx];
}

#ifdef __cplusplus
}
#endif

#endif /* __VOLUME_H__ */
