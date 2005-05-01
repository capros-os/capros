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

#include <kerninc/kernel.h>
#include <arch-kerninc/KernTune.h>
#include <kerninc/IoRegion.h>

static IoRegion regionList[KTUNE_NIOREGION];

/* replaces call to IoRegion constructor */
void
ioReg_Init()
{
  int i = 0;
  for (i = 0; i < KTUNE_NIOREGION; i++) {
    regionList[i].start = 0;
    regionList[i].end = 0;
  }
}

bool 
ioReg_IsAvailable(uint32_t start, uint32_t count)
{
  int i = 0;
  uint32_t end = start + count;
  
  for (i = 0; i < KTUNE_NIOREGION; i++) {
    uint32_t rgnStart = regionList[i].start;
    uint32_t rgnEnd = regionList[i].end;

    if (rgnStart >= end)
      continue;
    if (rgnEnd <= start)
      continue;

    return false;
  }

  return true;
}

bool
ioReg_Allocate(uint32_t start, uint32_t count, const char *drvrName)
{
  int i = 0;
  uint32_t end = 0;
  if (ioReg_IsAvailable(start, count) == false)
    return false;

  end = start + count;

  for (i = 0; i < KTUNE_NIOREGION; i++) {
    if (regionList[i].start == regionList[i].end) {
      regionList[i].start = start;
      regionList[i].end = end;
      regionList[i].name = drvrName;

      return true;
    }
  }

  fatal("Region list exhausted!\n");
  return false;
}

void
ioReg_Release(uint32_t start, uint32_t count)
{
  int i = 0;
#ifndef NDEBUG
  uint32_t end = start + count;
#endif

  for (i = 0; i < KTUNE_NIOREGION; i++) {
    if (regionList[i].start == start) {
      assert ( regionList[i].end == end );
      regionList[i].end = regionList[i].start;
      return;
    }
  }

  fatal("Freeing unallocated region!\n");
}

