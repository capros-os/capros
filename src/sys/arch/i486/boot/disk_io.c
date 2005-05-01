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

#include <eros/target.h>
#include <kerninc/BootInfo.h>
#include "boot-asm.h"
#include "boot.h"
#include "debug.h"

void
get_diskinfo(unsigned char drive, BiosGeometry *bg)
{
  uint32_t geomInfo = bios_diskinfo(drive);

  bg->drive = drive;
  bg->secs = geomInfo & 0xffu;
  bg->cyls = geomInfo >> 16;
  bg->heads = (geomInfo >> 8) & 0xffu;

  /* numbers reported by bios are max hd, cyl.  We want max+1. The spt
     value turns out to be okay because it is max relative to 1 */
  bg->heads++;
  bg->cyls++;

  bg->spcyl = bg->secs * bg->heads;
}

extern int putchar(int);

/* Read partition starting with sector 'sec', fetching 'count'
 * sectors into 'buf'.  'buf' is a PHYSICAL address.  We use the
 * area at SCRATCH_SEC:SCRATCH_OFFSET as a scratch buffer for I/O.
 */
void
read_sectors(unsigned char drive, unsigned long sector,
	     unsigned long count, void *buf)
{
  BiosGeometry bg;
  uint8_t *cbuf;

  DEBUG(disk)
    printf("\nread_sectors(0x%x, %ld, %ld, 0x%lx)\n",
	   drive, sector, count, (long)buf);

  if (drive == RAM_DISK_ID) {
    printf("read_sectors no longer groks ramdisks...\n");
    halt();
  }
  
  get_diskinfo(drive, &bg);

  if (bg.spcyl == 0 || bg.secs == 0) {
    printf("\nread_sectors: BIOS returned invalid information for disk %d.\n",
           drive);
    halt();
  } 
 
  cbuf = (uint8_t *) buf;
  
  while (count) {
    unsigned sec, cyl, head, nsec;

    sec = sector; /* bias by partition start */

    cyl = sec/bg.spcyl;
    sec %= bg.spcyl;
  
    head = sec/bg.secs;
    sec %= bg.secs;

    /* never read past the end of the track: */
    nsec = bg.secs - sec;
    if (nsec > count)
      nsec = count;

    if (nsec > SCRATCH_NSEC)
      nsec = SCRATCH_NSEC;
	
    twiddle();

    if (biosread(drive, cyl, head, sec, nsec,
		 SCRATCH_SEG, SCRATCH_OFFSET) != 0) {
      printf("Error: C:%d H:%d S:%d\n", cyl, head, sec);
      halt();
    }

    {
      uint32_t scratch_pa = (SCRATCH_SEG << 4) + SCRATCH_OFFSET;
      ppcpy((void *) scratch_pa, cbuf, nsec * EROS_SECTOR_SIZE);
    }

    sector += nsec;
    count -= nsec;
    cbuf += nsec * EROS_SECTOR_SIZE;
  }
}
