#ifndef __CMOS_H__
#define __CMOS_H__
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


/* Models the CMOS parameter memory of a PC. This includes only those
 * items that are of interest to the EROS kernel:
 */
 
/* CON */
#if 0    
struct CMOS {
    /* Number of extended memory pages, in Kilobytes: */
#if 0
  static uint32_t extendedMemorySize();
#endif
} ;
#endif
/* END CON */


/* Former member functions of CMOS */
uint8_t cmos_cmosByte(unsigned byte);
uint32_t cmos_fdType(int whichFd);
bool cmos_HaveFDC();

#endif /* __CMOS_H__ */
