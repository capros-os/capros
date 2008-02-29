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
#include <asm/USBIntf.h>

#include <domain/assert.h>
#include "usbdev.h"

int usb_reset_composite_device(struct usb_device *udev,
                struct usb_interface *iface)
{
  BUG_ON("usb_reset_composite_device unimplemented!");
  return 0;
}

int usb_lock_device_for_reset(struct usb_device *udev,
                              const struct usb_interface *iface)
{
  BUG_ON("usb_lock_device_for_reset unimplemented!");
  return 0;
}

void usb_settoggle(struct usb_device * dev, unsigned char endpointNum,
  unsigned int out, unsigned int bit)
{
  BUG_ON("usb_settoggle unimplemented!");
}
