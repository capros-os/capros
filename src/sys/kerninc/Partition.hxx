#ifndef __PARTITION_HXX__
#define __PARTITION_HXX__
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

#include <kerninc/BlockDev.hxx>
#include <kerninc/IoRequest.hxx>
#include <disk/ErosTypes.h>
#include <disk/DiskKey.hxx>

#define EROS_PARTITION_TYPE  0x95

struct Partition {
  CHS      bios_start;		/* geometry as seen by BIOS */
  CHS      bios_end;		/* ditto */
  uint32_t     lba_start;		/* offset in disk as LBA. */
  uint32_t     nSecs;		/* size in sectors */

  uint8_t     type;		/* machine specific */
  bool     isBoot;		/* true if this is the boot partition */
  uint8_t     unit;		/* unit on block device */
  uint8_t     ndx;			/* index of partition on device */

  BlockDev *ctrlr;		/* Controller class */
  uint32_t     uniqueID;		/* used for partition keys */
  bool     isEros;		/* true if this is really an EROS partition
		 */
				/* with an iplSysID matching that of the
				 * booted partition
				 */
  bool     isMounted;
  
  Partition()
  {
    ctrlr = 0;
  }
  Partition(BlockDev *ctrlr, uint8_t unit, uint8_t ndx);

  void Mount();
  void Unmount();
  
  static void MountAll();
  
  void DoDeviceRead(ObjectHeader* pOb, uint32_t sector, uint32_t nSecs)
  {
    ctrlr->DoDeviceRead(unit, pOb, sector + lba_start, nSecs);
  }
  
  void DoDeviceWrite(ObjectHeader* pOb, uint32_t sector, uint32_t nSecs)
  {
    ctrlr->DoDeviceWrite(unit, pOb, sector + lba_start, nSecs);
  }
  
  void InsertRequest(Request* req)
  {
    req->req_start += lba_start;
    ctrlr->InsertRequest(req);
  }

  void *operator new(size_t);
  void operator delete(void *);
};

#endif /* __PARTITION_HXX__ */
