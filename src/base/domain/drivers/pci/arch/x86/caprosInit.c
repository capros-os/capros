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

#include <linuxk/lsync.h>
#include <domain/assert.h>
#include <linux/pci.h>

void pci_main(void);
int pci_driver_init(void);
int pci_arch_init(void);
int pci_subsys_init(void);
int pci_init(void);

/*
 * Start here.
 */
void
driver_main(void)
{
  int retval;

  // Call initialization procedures in the order of Linux init phases:
  // postcore:
  retval = pci_driver_init();
  assert(!retval);

  // arch:
  retval = pci_arch_init();
  assert(!retval);

  // subsys:
  retval = pci_subsys_init();
  assert(!retval);

  // device:
  retval = pci_init();
  assert(!retval);

  pci_main();
}
