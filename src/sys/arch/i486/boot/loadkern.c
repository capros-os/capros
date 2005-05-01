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

#include <disk/LowVolume.h>
#include <disk/PagePot.h>
#include <kerninc/BootInfo.h>
#include "boot.h"
#include "boot-asm.h"
#include "debug.h"

Division
FindKernelDivision(BootInfo *bi)
{
  int div;
  Division* divtab;
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
    if (divtab->type == dt_Kernel)
      break;
    if (divtab->type == dt_Unused)
      break;
  }

  if (divtab->type == dt_Unused) {
    printf("Kernel division not found\n");
    halt();
  }
  else {
    DEBUG(load) printf("kernel at %ld end %ld\n",
		       divtab->start, divtab->end);
  }

  return *divtab;		/* calls memcpy! */
}

void
LoadKernel(BootInfo * bi)
{
  uint32_t kpages;
  uint32_t cursec;
  uint32_t pdest;
  uint32_t pg;

  Division kdiv = FindKernelDivision(bi);

  kpages = kdiv.endOid - kdiv.startOid;
  kpages /= EROS_OBJECTS_PER_FRAME;
    
  cursec = kdiv.start;

  /* Add bias for range header page with ckpt seq number */
  cursec += EROS_PAGE_SECTORS;

  /* Load the kernel into extended memory.  We will shift it when
     we are done with the I/O buffer */
  pdest = (uint32_t) BOOT2_HEAP_PTOP;
  
  printf(">> Loading kernel (%ld pages) @ 0x%lx... ", kpages, pdest);

  pg = 0;
    
  while (pg < kpages) {
    uint32_t npg;

    if ((pg % DATA_PAGES_PER_PAGE_CLUSTER) == 0)
      cursec += EROS_PAGE_SECTORS;

    npg = min(kpages - pg, DATA_PAGES_PER_PAGE_CLUSTER);

    if (bi->isRamImage) {
      uint32_t addr = RamDiskAddress + cursec * EROS_SECTOR_SIZE;
      ppcpy((void *) addr, (void *) pdest,
	    npg * EROS_PAGE_SECTORS * EROS_SECTOR_SIZE);
    }
    else {
      read_sectors(bi->bootDrive, cursec + bi->bootStartSec,
		   npg * EROS_PAGE_SECTORS,
		   (void *) pdest);
    }

    pdest += (npg * EROS_PAGE_SIZE);
    cursec += (npg * EROS_PAGE_SECTORS);
    pg += npg;
  }

  printf("done \001\n"); /* IBM character set happy face*/
  /* Now shift the kernel to its final location */
  if ((BOOT2_HEAP_PTOP) != KERNPBASE) {
    ppcpy ((void *) BOOT2_HEAP_PTOP,
	   (void *) KERNPBASE, kpages * EROS_PAGE_SIZE);

    printf("   Relocated kernel to @ 0x%x\n", KERNPBASE);
  }
}
