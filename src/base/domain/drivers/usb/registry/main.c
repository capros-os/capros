/*
 * based on drivers/usb/usb.c which had the following copyrights:
 *	(C) Copyright Linus Torvalds 1999
 *	(C) Copyright Johannes Erdfelt 1999-2001
 *	(C) Copyright Andreas Gal 1999
 *	(C) Copyright Gregory P. Smith 1999
 *	(C) Copyright Deti Fliegl 1999 (new USB architecture)
 *	(C) Copyright Randy Dunlap 2000
 *	(C) Copyright David Brownell 2000-2004
 *	(C) Copyright Yggdrasil Computing, Inc. 2000
 *		(usb_device_id matching changes by Adam J. Richter)
 *	(C) Copyright Greg Kroah-Hartman 2002-2003
 * (C) Copyright 2005 Greg Kroah-Hartman <gregkh@suse.de>
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
 * The USB registry receives USBInterface caps and matches them
   with drivers.
 */

#include <string.h>
#include <asm/byteorder.h>
#include <linux/usb/ch9.h>
#include <eros/Invoke.h>
#include <idl/capros/USBHCD.h>
#include <idl/capros/USBDriverConstructor.h>
#include <idl/capros/USBDriverConstructorExtended.h>

#include <domain/assert.h>
#include "registry.h"

#define KR_USBHCD         KR_APP(0)
#define KR_OSTREAM        KR_APP(1)
#define KR_RequestorSnode KR_APP(2)
#define KR_USBIntf        KR_APP(5)

/* Bypass all the usual initialization. */
unsigned long __rt_stack_pointer = 0x20000;
unsigned long __rt_runtime_hook = 0;
uint32_t __rt_unkept = 1;

/* For now, we have a static list of drivers.
   Eventually the registry will accept new drivers
   and grow the list dynamically. */

extern struct usb_device_id storage_usb_ids;

#define NUM_DRIVERS 1

struct usb_driver_registration drivers[NUM_DRIVERS] = {
  {&storage_usb_ids, 0}
};

/* returns 0 if no match, 1 if match */
static bool usb_match_device(capros_USB_DeviceDescriptor * dd,
  const struct usb_device_id *id)
{
	if ((id->match_flags & USB_DEVICE_ID_MATCH_VENDOR) &&
	    id->idVendor != le16_to_cpu(dd->idVendor))
		return false;

	if ((id->match_flags & USB_DEVICE_ID_MATCH_PRODUCT) &&
	    id->idProduct != le16_to_cpu(dd->idProduct))
		return false;

	/* No need to test id->bcdDevice_lo != 0, since 0 is never
	   greater than any unsigned number. */
	if ((id->match_flags & USB_DEVICE_ID_MATCH_DEV_LO) &&
	    (id->bcdDevice_lo > le16_to_cpu(dd->bcdDevice)))
		return false;

	if ((id->match_flags & USB_DEVICE_ID_MATCH_DEV_HI) &&
	    (id->bcdDevice_hi < le16_to_cpu(dd->bcdDevice)))
		return false;

	if ((id->match_flags & USB_DEVICE_ID_MATCH_DEV_CLASS) &&
	    (id->bDeviceClass != dd->bDeviceClass))
		return false;

	if ((id->match_flags & USB_DEVICE_ID_MATCH_DEV_SUBCLASS) &&
	    (id->bDeviceSubClass!= dd->bDeviceSubClass))
		return false;

	if ((id->match_flags & USB_DEVICE_ID_MATCH_DEV_PROTOCOL) &&
	    (id->bDeviceProtocol != dd->bDeviceProtocol))
		return false;

	return true;
}

/* returns 0 if no match, 1 if match */
bool usb_match_one_id(capros_USBHCD_InterfaceData * nid,
		      const struct usb_device_id * id)
{
	capros_USB_InterfaceDescriptor * intfDesc = &nid->id;
	capros_USB_DeviceDescriptor * dd = &nid->dd;

	if (!usb_match_device(dd, id))
		return false;

	/* The interface class, subclass, and protocol should never be
	 * checked for a match if the device class is Vendor Specific,
	 * unless the match record specifies the Vendor ID. */
	if (dd->bDeviceClass == USB_CLASS_VENDOR_SPEC &&
			!(id->match_flags & USB_DEVICE_ID_MATCH_VENDOR) &&
			(id->match_flags & (USB_DEVICE_ID_MATCH_INT_CLASS |
				USB_DEVICE_ID_MATCH_INT_SUBCLASS |
				USB_DEVICE_ID_MATCH_INT_PROTOCOL)))
		return false;

	if ((id->match_flags & USB_DEVICE_ID_MATCH_INT_CLASS) &&
	    (id->bInterfaceClass != intfDesc->bInterfaceClass))
		return false;

	if ((id->match_flags & USB_DEVICE_ID_MATCH_INT_SUBCLASS) &&
	    (id->bInterfaceSubClass != intfDesc->bInterfaceSubClass))
		return false;

	if ((id->match_flags & USB_DEVICE_ID_MATCH_INT_PROTOCOL) &&
	    (id->bInterfaceProtocol != intfDesc->bInterfaceProtocol))
		return false;

	return true;
}

/**
 * usb_match_id - find first usb_device_id matching device or interface
 * @interface: the interface of interest
 * @id: array of usb_device_id structures, terminated by zero entry
 *
 * usb_match_id searches an array of usb_device_id's and returns
 * the first one matching the device or interface, or null.
 * This is used when binding (or rebinding) a driver to an interface.
 * Most USB device drivers will use this indirectly, through the usb core,
 * but some layered driver frameworks use it directly.
 * These device tables are exported with MODULE_DEVICE_TABLE, through
 * modutils, to support the driver loading functionality of USB hotplugging.
 *
 * What Matches:
 *
 * The "match_flags" element in a usb_device_id controls which
 * members are used.  If the corresponding bit is set, the
 * value in the device_id must match its corresponding member
 * in the device or interface descriptor, or else the device_id
 * does not match.
 *
 * "driver_info" is normally used only by device drivers,
 * but you can create a wildcard "matches anything" usb_device_id
 * as a driver's "modules.usbmap" entry if you provide an id with
 * only a nonzero "driver_info" field.  If you do this, the USB device
 * driver's probe() routine should use additional intelligence to
 * decide whether to bind to the specified interface.
 *
 * What Makes Good usb_device_id Tables:
 *
 * The match algorithm is very simple, so that intelligence in
 * driver selection must come from smart driver id records.
 * Unless you have good reasons to use another selection policy,
 * provide match elements only in related groups, and order match
 * specifiers from specific to general.  Use the macros provided
 * for that purpose if you can.
 *
 * The most specific match specifiers use device descriptor
 * data.  These are commonly used with product-specific matches;
 * the USB_DEVICE macro lets you provide vendor and product IDs,
 * and you can also match against ranges of product revisions.
 * These are widely used for devices with application or vendor
 * specific bDeviceClass values.
 *
 * Matches based on device class/subclass/protocol specifications
 * are slightly more general; use the USB_DEVICE_INFO macro, or
 * its siblings.  These are used with single-function devices
 * where bDeviceClass doesn't specify that each interface has
 * its own class.
 *
 * Matches based on interface class/subclass/protocol are the
 * most general; they let drivers bind to any interface on a
 * multiple-function device.  Use the USB_INTERFACE_INFO
 * macro, or its siblings, to match class-per-interface style
 * devices (as recorded in bInterfaceClass).
 *
 * Note that an entry created by USB_INTERFACE_INFO won't match
 * any interface if the device class is set to Vendor-Specific.
 * This is deliberate; according to the USB spec the meanings of
 * the interface class/subclass/protocol for these devices are also
 * vendor-specific, and hence matching against a standard product
 * class wouldn't work anyway.  If you really want to use an
 * interface-based match for such a device, create a match record
 * that also specifies the vendor ID.  (Unforunately there isn't a
 * standard macro for creating records like this.)
 *
 * Within those groups, remember that not all combinations are
 * meaningful.  For example, don't give a product version range
 * without vendor and product IDs; or specify a protocol without
 * its associated class and subclass.
 */
const struct usb_device_id * usb_match_id(capros_USBHCD_InterfaceData * nid,
					  const struct usb_device_id * id)
{
	/* It is important to check that id->driver_info is nonzero,
	   since an entry that is all zeroes except for a nonzero
	   id->driver_info is the way to create an entry that
	   indicates that the driver want to examine every
	   device and interface. */
	for (; id->idVendor || id->bDeviceClass || id->bInterfaceClass ||
	       id->driver_info; id++) {
		if (usb_match_one_id(nid, id))
			return id;
	}

	return NULL;
}

int
main(void)
{
  result_t result;
  int i;

  for (;;) {
    capros_USBDriverConstructorExtended_NewInterfaceData nid;
    result = capros_USBHCD_getNewInterface(KR_USBHCD,
               &nid.intfData, KR_USBIntf);
    assert(result == RC_OK);

    for (i = 0; i < NUM_DRIVERS; i++) {	// loop over all drivers
      const struct usb_device_id * id;
      id = usb_match_id(&nid.intfData, drivers[i].id_table);
      if (id) {		// found a match
        // Create an instance of the driver and pass the interface cap.
        // This is equivalent to Linux driver->probe().
        nid.deviceIdIndex = id - drivers[i].id_table;
        memcpy(&nid.deviceId, id, sizeof(nid.deviceId));
        result = capros_Node_getSlotExtended(KR_RequestorSnode,
                   drivers[i].driver_requestor,
                   KR_TEMP0);
        assert(result == RC_OK);
        result = capros_USBDriverConstructor_probe(KR_TEMP0,
                   KR_BANK, KR_SCHED, KR_USBIntf,
                   KR_VOID, KR_VOID, KR_VOID, KR_TEMP0);
        if (result == RC_OK) {
          // Driver called us back to get nid.
          result = capros_USBDriverConstructorExtended_sendID(KR_TEMP0, nid);
        }
        switch (result) {
        case RC_OK:
          kprintf(KR_OSTREAM, "Driver %d returned OK.\n", i);////
          goto found;

        default:
          kprintf(KR_OSTREAM, "Driver %d returned 0x%x\n", i, result);
        case RC_capros_USBDriverConstructor_ProbeUnsuccessful:
          break;	// continue trying drivers
        }
      }
    }
    kprintf(KR_OSTREAM, "No driver for USB device:\n");
#define devDescr nid.intfData.dd
#define intfDescr nid.intfData.id
    kprintf(KR_OSTREAM, "dd.Class %d dd.SubClass %d dd.Protocol %d\n",
            devDescr.bDeviceClass,
            devDescr.bDeviceSubClass,
            devDescr.bDeviceProtocol);
    kprintf(KR_OSTREAM, "dd.Vendor %d dd.Product %d dd.bcd %d\n",
            devDescr.idVendor,
            devDescr.idProduct,
            devDescr.bcdDevice);
    kprintf(KR_OSTREAM, "id.Class %d id.SubClass %d id.Protocol %d\n",
            intfDescr.bInterfaceClass,
            intfDescr.bInterfaceSubClass,
            intfDescr.bInterfaceProtocol);
#undef devDescr
#undef intfDescr

    found: ;
  }
}
