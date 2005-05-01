#ifndef __IOREGIONHXX__
#define __IOREGIONHXX__
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

struct IoRegion {
  uint32_t start;
  uint32_t end;			/* exclusive. start==end => unused */
  const char *name;		/* of driver */
};

typedef struct IoRegion IoRegion;


/* Former member functions of IoRegion */
void ioReg_Init();

bool ioReg_IsAvailable(uint32_t start, uint32_t count);
bool ioReg_Allocate(uint32_t start, uint32_t count, const char *drvr_name);
void ioReg_Release(uint32_t start, uint32_t count);



#endif /* __IOREGIONHXX__ */
