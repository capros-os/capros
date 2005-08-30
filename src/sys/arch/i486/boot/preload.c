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

#include <disk/LowVolume.h>
#include <kerninc/BootInfo.h>
#include <kerninc/util.h>
#include "boot.h"
#include "boot-asm.h"
#include "debug.h"
#include <zlib/zlib.h>

void
do_preload(BootInfo *bi, Division *div)
{
  void *pdest;
  uint32_t sectors = div->end - div->start;
  uint32_t start = div->start;
  kpsize_t size;
  DivisionInfo *di = &bi->divInfo[bi->nDivInfo];

  if (bi->nDivInfo == MAX_PRELOAD) {
    printf("Unable to load more than %d preloaded ranges\n", MAX_PRELOAD);
    halt();
  }
  
  bi->nDivInfo++;

  /* Add bias for range header page with ckpt seq number */
  start += EROS_PAGE_SECTORS;
  sectors -= EROS_PAGE_SECTORS;
  size = sectors * EROS_SECTOR_SIZE;
  /* Round up to page size. */
  size = (size + (EROS_PAGE_SIZE-1)) & ~(kpsize_t)(EROS_PAGE_SIZE-1);

  /* Allocate space for the preloaded range: */
  pdest = 
    BootAlloc(size, EROS_PAGE_SIZE);

  /* In this case, pdest really should be the physical destination. */
  pdest = BOOT2PA(pdest, void *);
  BindRegion(bi, PtoKPA(pdest), size, MI_PRELOAD);

  if (bi->isRamImage) {
    uint32_t addr = RamDiskAddress + start * EROS_SECTOR_SIZE;
    ppcpy((void *) addr, (void *) pdest, size);
  }
  else {
    read_sectors(bi->bootDrive, start + bi->bootStartSec,
		 sectors, (void *) pdest);
  }

  di->startOid = div->startOid;
  di->endOid = div->endOid;
  di->flags = div->flags;
  di->type = div->type;
  di->where = PtoKPA(pdest);
  di->bound = di->where + size;
}

void
PreloadDivisions(BootInfo * bi)
{
  Division* divtab;
  int div;
  char buf[EROS_PAGE_SIZE];
  uint32_t divSector = DivTable;

  DEBUG(load) printf("Division table at sector %ld\n", divSector);

  if (bi->isRamImage) {
    uint32_t addr = RamDiskAddress + divSector * EROS_SECTOR_SIZE;
    ppcpy((void *) addr, BOOT2PA(buf, void*), 
	  EROS_PAGE_SECTORS * EROS_SECTOR_SIZE);
  }
  else {
    read_sectors(bi->bootDrive, divSector + bi->bootStartSec,
		 EROS_PAGE_SECTORS, BOOT2PA(buf, void*));
  }

  divtab = (Division *) buf;
  
  for(div = 0; div < NDIVENT; div++, divtab++) {
    if (divtab->flags & DF_PRELOAD)
      do_preload(bi, divtab);
  }

  printf("Divisions are preloaded...\n");

  if (CND_DEBUG(step)) {
    printf("Press any key to continue\n");
    waitkbd();
  }
}

void
ShowDivisions(BootInfo *bi)
{
  unsigned i;

  printf("In ShowDivisions()... %d divisions\n", bi->nDivInfo);

  for(i = 0; i < bi->nDivInfo; i++) {
    DivisionInfo *di = &bi->divInfo[i];

    printf("Preload %d from 0x%08x%08x to 0x%08x%08x flags 0x%x\n",
	   i,
	   (unsigned) (di->startOid >> 32),
	   (unsigned) (di->startOid),
	   (unsigned) (di->endOid >> 32),
	   (unsigned) (di->endOid),
	   di->flags);
  }
}
