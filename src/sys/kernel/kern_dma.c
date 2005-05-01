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
#include <kerninc/dma.h>

/* FIX: this should migrate to a machine-specific header: */
#define MAX_DMA 8

struct chaninfo {
  unsigned allocated;
  const char *devname;
} dmachan[MAX_DMA] = {
  { 0, 0 },
  { 0, 0 },
  { 0, 0 },
  { 0, 0 },
#ifdef PC_DMA_CASCADE
  { 1, "cascade" },
#else
  { 0, 0},
#endif
  { 0, 0},
  { 0, 0},
  { 0, 0},
};

int
request_dma(unsigned chan, const char *devname)
{
  if (chan >= MAX_DMA)
    return -1;
  if (dmachan[chan].allocated)
    return -1;

  dmachan[chan].allocated = 1;
  dmachan[chan].devname = devname;

  return 0;
}

void
free_dma(unsigned chan)
{
  assert(dmachan[chan].allocated);
  dmachan[chan].allocated = 0;
  dmachan[chan].devname = 0;
}

void
init_dma()
{
}
