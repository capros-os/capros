/*
 * Copyright (C) 2002, Jonathan S. Shapiro.
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
#include <eros/machine/io.h>

#include "vmware_io.h"
#include "svga_reg.h"

void
vmwWrite(uint32_t card_id, uint32_t port, uint32_t index, uint32_t value)
{
  if (card_id == SVGA_LEGACY_PCI_ID) {
    outl(index, SVGA_LEGACY_BASE_PORT + 4 * SVGA_INDEX_PORT);
    outl(value, SVGA_LEGACY_BASE_PORT + 4 * SVGA_VALUE_PORT);
  }
  else {
    outl(index, port + SVGA_INDEX_PORT);
    outl(value, port + SVGA_VALUE_PORT);
  }
}

uint32_t
vmwRead(uint32_t card_id, uint32_t port, uint32_t index)
{
  if (card_id == SVGA_LEGACY_PCI_ID) {
    outl(index, SVGA_LEGACY_BASE_PORT + 4 * SVGA_INDEX_PORT);
    return inl(SVGA_LEGACY_BASE_PORT + 4 * SVGA_VALUE_PORT);
  }
  else {
    outl(index, port + SVGA_INDEX_PORT);
    return inl(port + SVGA_VALUE_PORT);
  }
}
