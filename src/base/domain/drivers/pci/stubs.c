/*
 * Copyright (C) 2010, Strawberry Development Group
 *
 * This file is part of the CapROS Operating System.
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

#include <domain/assert.h>
#include <linux/device.h>

int driver_register(struct device_driver *drv)
{
  assert(! " stubbed!");
  return 0;
}

void driver_unregister(struct device_driver *drv)
{
  assert(! " stubbed!");
}

void device_unregister(struct device *dev)
{
  assert(! " stubbed!");
}

void *devres_find(struct device *dev, dr_release_t release,
                         dr_match_t match, void *match_data)
{
  assert(! " stubbed!");
  return NULL;
}

void *devres_alloc(dr_release_t release, size_t size, gfp_t gfp)
{
  assert(! " stubbed!");
  return NULL;
}

void *devres_get(struct device *dev, void *new_res,
                        dr_match_t match, void *match_data)
{
  assert(! " stubbed!");
  return NULL;
}

#include <linux/interrupt.h>

void disable_irq(unsigned int irq)
{
  assert(! " stubbed!");
}

void enable_irq(unsigned int irq)
{
  assert(! " stubbed!");
}

#include <linux/pci.h>

struct bus_type pci_bus_type;

void pci_fixup_device(enum pci_fixup_pass pass, struct pci_dev *dev)
{
}

#if 0
char *pcibios_setup(char *str)
{
  assert(! " stubbed!");
  return NULL;
}

void pcibios_align_resource(void *a, struct resource *b, resource_size_t c,
                                resource_size_t d)
{
  assert(! " stubbed!");
}

#include <asm/pci.h>

void pcibios_set_master(struct pci_dev *dev)
{
  assert(! " stubbed!");
}
#endif

// from x86/kernel/e820.c
unsigned long pci_mem_start = 0xaeedbabe;

// from drivers/base/bus.c
int bus_add_device(struct device *dev)
{
  assert(! " stubbed!");
  return 0;
}
