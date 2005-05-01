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

#include <kerninc/BootInfo.h>
#include "boot-asm.h"
#include "boot.h"

/* These are set from the boot2.S code. */
uint16_t BootHeadDrive;
uint16_t BootCylSec;

void
CaptureBootDisk(BootInfo *bi)
{
  uint32_t bootHead = (BootHeadDrive >> 8) & 0xffu;
  uint32_t bootCyl = 
    ((BootCylSec << 2) & 0x300u) | ((BootCylSec >> 8) & 0xffu);
  uint32_t bootSec = BootCylSec & 0x3fu;

  bi->bootDrive = BootHeadDrive & 0xffu;

  /* correct for the fact that our interfaces all use zero-relative
     sector numbers, while the DOS interface uses 1-relative sector
     numbering: */
  
  bootSec--;
  
  /* fetch the drive geometry info for the boot drive: */
  {
    BiosGeometry bg;

    get_diskinfo(bi->bootDrive, &bg);

    bi->bootGeom.heads = bg.heads;
    bi->bootGeom.cylinders = bg.cyls;
    bi->bootGeom.sectors = bg.secs;

    /* compute the starting sector number for the boot partition: */
    bi->bootStartSec = (bootCyl * bg.spcyl +
			bootHead * bg.secs +
			bootSec);

    printf("Booting from %s%d at C/H/S=%d/%d/%d, "
	   "geometry C/H/S=%d/%d/%d\n"
	   "startSec %d\n",

	   ((bi->bootDrive & 0xf0u) ? "hd" : "fd") ,
	   (int) (bi->bootDrive & 0xfu),

	   bootCyl,
	   bootHead,
	   bootSec,

	   bg.cyls,
	   bg.heads,
	   bg.secs,

	   bi->bootStartSec);
  }
}
