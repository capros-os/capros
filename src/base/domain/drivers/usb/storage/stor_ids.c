/*
 * usb_device_id support by Adam J. Richter (adam@yggdrasil.com):
 *   (c) 2000 Yggdrasil Computing, Inc.
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

#include <linux/usb.h>
#include <linux/usb_usual.h>

/*
 * The entries in this table correspond, line for line,
 * with the entries of us_unusual_dev_list[].
 */

#define UNUSUAL_DEV(id_vendor, id_product, bcdDeviceMin, bcdDeviceMax, \
		    vendorName, productName,useProtocol, useTransport, \
		    initFunction, flags) \
{ USB_DEVICE_VER(id_vendor, id_product, bcdDeviceMin,bcdDeviceMax), \
  .driver_info = (flags)|(USB_US_TYPE_STOR<<24) }

#define USUAL_DEV(useProto, useTrans, useType) \
{ USB_INTERFACE_INFO(USB_CLASS_MASS_STORAGE, useProto, useTrans), \
  .driver_info = (USB_US_TYPE_STOR<<24) }

struct usb_device_id storage_usb_ids [] = {

#	include "unusual_devs.h"
#undef UNUSUAL_DEV
#undef USUAL_DEV
	/* Terminating entry */
	{ }
};
