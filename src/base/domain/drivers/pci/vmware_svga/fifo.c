/*
 * Copyright (C) 2003, Jonathan S. Shapiro.
 *
 * This file is part of the EROS Operating System distribution.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, 59 Temple Place - Suite 330 Boston, MA 02111-1307, USA.
 */
#include <eros/target.h>
#include <eros/Invoke.h>

#include "fifo.h"
#include "vmware_io.h"
#include "svga_reg.h"

uint32_t 
fifo_init(uint32_t *fifo, uint32_t size)
{
  fifo[SVGA_FIFO_MIN] = 4 * sizeof(uint32_t);
  fifo[SVGA_FIFO_MAX] = size;
  fifo[SVGA_FIFO_NEXT_CMD] = 4 * sizeof(uint32_t);
  fifo[SVGA_FIFO_STOP] = 4 * sizeof(uint32_t);

  /* Tell the device we're done configuring the command fifo queue. */
  VMWRITE(SVGA_REG_CONFIG_DONE, 1);

  return RC_OK;
}

uint32_t 
fifo_sync(uint32_t *fifo)
{
  VMWRITE(SVGA_REG_SYNC, 1);
  while (VMREAD(SVGA_REG_BUSY)) ;  

  return RC_OK;
}

uint32_t 
fifo_insert(uint32_t *fifo, uint32_t word)
{
  /* Check if we need to sync first */
  if ((fifo[SVGA_FIFO_NEXT_CMD] + sizeof(uint32_t) == fifo[SVGA_FIFO_STOP]) ||
      ((fifo[SVGA_FIFO_NEXT_CMD] == fifo[SVGA_FIFO_MAX] - sizeof(uint32_t)) &&
       (fifo[SVGA_FIFO_STOP] == fifo[SVGA_FIFO_MIN]))) {

    fifo_sync(fifo);
  } else

  fifo[fifo[SVGA_FIFO_NEXT_CMD] / sizeof(uint32_t)] = word;
  fifo[SVGA_FIFO_NEXT_CMD] += sizeof(uint32_t);
  if (fifo[SVGA_FIFO_NEXT_CMD] == fifo[SVGA_FIFO_MAX]) 
    fifo[SVGA_FIFO_NEXT_CMD] = fifo[SVGA_FIFO_MIN];

  return RC_OK;
}


