/*
 * Copyright (C) 2008, 2009, Strawberry Development Group
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
unsigned long capros_Errno_ExceptionToErrno(unsigned long);


/**
 * usb_get_dev - increments the reference count of the usb device structure
 * @dev: the device being referenced
 *
 * Each live reference to a device should be refcounted.
 *
 * Drivers for USB interfaces should normally record such references in
 * their probe() methods, when they bind to an interface, and release
 * them by calling usb_put_dev(), in their disconnect() methods.
 *
 * A pointer to the device with the incremented reference counter is returned.
 */
struct usb_device * usb_get_dev(struct usb_device * dev)
{
	if (dev)
		get_device(&dev->dev);
	return dev;
}

/**
 * usb_put_dev - release a use of the usb device structure
 * @dev: device that's been disconnected
 *
 * Must be called when a user of a device is finished with it.  When the last
 * user of the device calls this function, the memory of the device is freed.
 */
void usb_put_dev(struct usb_device * dev)
{
	if (dev)
		put_device(&dev->dev);
}

int usb_set_interface(struct usb_device * dev, int interface, int alternate)
{
  result_t result = capros_USBInterface_setAlternateSetting(KR_USBINTF,
	alternate);
  return capros_Errno_ExceptionToErrno(result);
}

int usb_reset_device(struct usb_device *udev)
{
  BUG_ON("usb_reset_device unimplemented!");
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

int usb_clear_halt(struct usb_device *dev /* unused */, int pipe)
{
  result_t result = capros_USBInterface_clearHalt(KR_USBINTF, pipe);
  return - capros_Errno_ExceptionToErrno(result);
}
