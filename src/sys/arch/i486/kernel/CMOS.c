/*
 * Copyright (C) 1998, 1999, Jonathan S. Shapiro.
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

#include <kerninc/kernel.h>
#include <eros/i486/io.h>
#include "CMOS.h"

/* Models the CMOS parameter memory of a PC. This includes only those
 * items that are of interest to the EROS kernel:
 */

uint8_t
cmos_cmosByte(unsigned byte)
{
  uint8_t b1, b2;
  
  do {
    outb(byte, 0x70);
    b1 = inb(0x71);
    outb(byte, 0x70);
    b2 = inb(0x71);
  } while (b1 != b2);
  return b1;
}

#if 0
/* Number of extended memory pages, in Kilobytes: */
uint32_t
CMOS::extendedMemorySize()
{
  uint32_t nKbytes, nKbytes2;
  
  nKbytes = cmosByte(0x18);
  nKbytes2 = cmosByte(0x17);
  printf("nKbytes: %x nKbytes2: %x\n", nKbytes, nKbytes2);
  
  nKbytes = cmosByte(0x18u) << 8 | cmosByte(0x17u);
  return nKbytes;
}
#endif

uint32_t
cmos_fdType(int whichFd)
{
  uint32_t which = cmos_cmosByte(0x10);
    
  switch(whichFd) {
  case 0:
    return (which >> 4) & 0xff;
  case 1:
    return (which & 0xff);
  default:
    return 0;			/* no bios info */
  }

  /* NOTREACHED */
}

bool
cmos_HaveFDC()
{
  uint32_t which = cmos_cmosByte(0x14);

  return (which & 0x1u) ? true : false;
}

