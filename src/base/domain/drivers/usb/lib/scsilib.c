/*
 * Copyright (C) 2008, Strawberry Development Group
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
/* This material is based upon work supported by the US Defense Advanced
Research Projects Agency under Contract No. W31P4Q-07-C-0070.
Approved for public release, distribution unlimited. */

/*
 * ...
 */

#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/usb.h>
#include <eros/Invoke.h>
#include <idl/capros/USBInterface.h>

#include <scsi/scsi.h>
#include <scsi/scsi_host.h>

#include <domain/assert.h>
#include "usbdev.h"

unsigned int scsi_logging_level;

#if 0
struct Scsi_Host *
scsi_host_alloc(struct scsi_host_template * sht, int privsize)
{
  assert(!"scsi_host_alloc called, unimplemented!");
  return 0;
}

int __must_check
scsi_add_host(struct Scsi_Host * shost, struct device * dev)
{
  assert(!"scsi_add_host called, unimplemented!");
  return 0;
}

void scsi_remove_host(struct Scsi_Host * shost)
{
  assert(!"scsi_remove_host called, unimplemented!");
}

struct Scsi_Host *scsi_host_get(struct Scsi_Host * shost)
{
  assert(!"scsi_host_get called, unimplemented!");
  return 0;
}

void scsi_host_put(struct Scsi_Host * shost)
{
  assert(!"scsi_host_put called, unimplemented!");
}
#endif

void scsi_scan_host(struct Scsi_Host * shost)
{
  assert(!"scsi_scan_host called, unimplemented!");
}

void scsi_report_device_reset(struct Scsi_Host *shost, int channel, int target)
{
  assert(!"scsi_report_device_reset called, unimplemented!");
}

void scsi_report_bus_reset(struct Scsi_Host *shost, int channel)
{
  assert(!"scsi_report_bus_reset called, unimplemented!");
}
