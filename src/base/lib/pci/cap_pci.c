/*
 * Copyright (C) 2010, Strawberry Development Group.
 *
 * This file is part of the CapROS Operating System.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this library; if not, write to the Free Software
 * Foundation, 59 Temple Place - Suite 330 Boston, MA 02111-1307, USA.
 */

#include <linuxk/linux-emul.h>
#include <linux/pci.h>
#include <eros/Invoke.h>
#include <idl/capros/PCIDev.h>
#include <domain/PCIDrvr.h>
#include <domain/assert.h>

static int
PCIDevResultToInt(result_t result)
{
  if (result != RC_OK) {
    switch (result) {
    default:
    case RC_capros_PCIDev_ConfigInval:
      return -EINVAL;
    case RC_capros_PCIDev_ConfigAddrMisaligned:
      return PCIBIOS_BAD_REGISTER_NUMBER;
    }
  }
  return PCIBIOS_SUCCESSFUL;
}

int pci_bus_read_config_byte(struct pci_bus *bus, unsigned int devfn,
                             int where, u8 *val)
{
  result_t result = capros_PCIDev_readConfig8(KR_PCIDrvr_PCIDev, where, val);
  return PCIDevResultToInt(result);
}

int pci_bus_read_config_word(struct pci_bus *bus, unsigned int devfn,
                             int where, u16 *val)
{
  result_t result = capros_PCIDev_readConfig16(KR_PCIDrvr_PCIDev, where, val);
  return PCIDevResultToInt(result);
}

int pci_bus_read_config_dword(struct pci_bus *bus, unsigned int devfn,
                              int where, u32 *val)
{
  result_t result = capros_PCIDev_readConfig32(KR_PCIDrvr_PCIDev, where, val);
  return PCIDevResultToInt(result);
}

int pci_bus_write_config_byte(struct pci_bus *bus, unsigned int devfn,
                              int where, u8 val)
{
  result_t result = capros_PCIDev_writeConfig8(KR_PCIDrvr_PCIDev, where, val);
  return PCIDevResultToInt(result);
}

int pci_bus_write_config_word(struct pci_bus *bus, unsigned int devfn,
                              int where, u16 val)
{
  result_t result = capros_PCIDev_writeConfig16(KR_PCIDrvr_PCIDev, where, val);
  return PCIDevResultToInt(result);
}

int pci_bus_write_config_dword(struct pci_bus *bus, unsigned int devfn,
                               int where, u32 val)
{
  result_t result = capros_PCIDev_writeConfig32(KR_PCIDrvr_PCIDev, where, val);
  return PCIDevResultToInt(result);
}

int pci_enable_device(struct pci_dev *dev)
{
  result_t result = capros_PCIDev_enable(KR_PCIDrvr_PCIDev);
  switch (result) {
  default:
  case RC_capros_PCIDev_ResourceConflict:
    return -EINVAL;
  case RC_OK:
    return 0;
  }
}

void
pci_disable_device(struct pci_dev *dev)
{
  result_t result = capros_PCIDev_disable(KR_PCIDrvr_PCIDev);
  assert(result == RC_OK);
}
