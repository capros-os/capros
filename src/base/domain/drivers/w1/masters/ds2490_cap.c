/*
 * Copyright (C) 2008, Strawberry Development Group.
 *
 * This file is part of the CapROS Operating System.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */

#include <stdlib.h>
#include <linux/kernel.h>
//#include <linux/mod_devicetable.h>
#include <eros/Invoke.h>
#include <idl/capros/Node.h>
#include <idl/capros/Process.h>
#include <idl/capros/USBDriverConstructor.h>
#include <idl/capros/USBDriverConstructorExtended.h>
#include <idl/capros/USBDriver.h>
#include <asm/USBIntf.h>
#include <domain/assert.h>
#include <eros/machine/cap-instr.h>

//#include "../w1_int.h"
//#include "../w1.h"

//#include "../../usb/lib/usbdev.h"
#include "ds2490.h"


capros_USBDriverConstructorExtended_NewInterfaceData theNid;
/* N.B.: the following are different instances from the ones
in the core USB HCD. */
// theEndpoints does not include EP0, NUM_EP does.
struct usb_host_endpoint theEndpoints[NUM_EP - 1];
struct usb_host_interface theSetting = {
  .endpoint = &theEndpoints[0]
};
struct usb_interface theIntf;
struct usb_device theUsbDev;

unsigned int w1bus_threadNum;

struct ds_device theDSDev;

int ds_probe(struct usb_interface *intf,
		    const struct usb_device_id *udev_id)
{
	struct usb_device *udev = interface_to_usbdev(intf);
	struct usb_endpoint_descriptor *endpoint;
	struct usb_host_interface *iface_desc;
	struct ds_device * dev = &theDSDev;
	int i, err;

	dev->udev = usb_get_dev(udev);
	memset(dev->ep, 0, sizeof(dev->ep));

	usb_set_intfdata(intf, dev);

	/* Set alternate setting 3, which gives us 1ms polling, 
	64 byte max packet size. */
	err = usb_set_interface(dev->udev, intf->altsetting[0].desc.bInterfaceNumber, 3);
	if (err) {
		printk(KERN_ERR "Failed to set alternative setting 3 for %d interface: err=%d.\n",
				intf->altsetting[0].desc.bInterfaceNumber, err);
		goto err_out_clear;
	}

#if 0 // is this necessary? Doesn't it reset the alt setting to zero?
	err = usb_reset_configuration(dev->udev);
	if (err) {
		printk(KERN_ERR "Failed to reset configuration: err=%d.\n", err);
		goto err_out_clear;
	}
#endif

	iface_desc = &intf->altsetting[0];

	/*
	 * This loop doesn't show control 0 endpoint,
	 * so we will fill only 1-3 endpoints entry.
	 */
	for (i = 0; i < iface_desc->desc.bNumEndpoints; ++i) {
		endpoint = &iface_desc->endpoint[i].desc;

		dev->ep[i+1] = endpoint->bEndpointAddress;
#if 1////
		printk("%d: addr=%x, size=%d, dir=%s, type=%x\n",
			i, endpoint->bEndpointAddress, le16_to_cpu(endpoint->wMaxPacketSize),
			(endpoint->bEndpointAddress & USB_DIR_IN)?"IN":"OUT",
			endpoint->bmAttributes & USB_ENDPOINT_XFERTYPE_MASK);
#endif
	}

	return 0;

err_out_clear:
	usb_set_intfdata(intf, NULL);
	usb_put_dev(dev->udev);
	return err;
}

static void ds_disconnect(struct usb_interface *intf)
{
	struct ds_device *dev;

	dev = usb_get_intfdata(intf);
	if (!dev)
		return;

	usb_set_intfdata(intf, NULL);

	usb_put_dev(dev->udev);
}

static void
TearDownStructures(void)
{
  usbdev_buffer_destroy();
}

static int
SetUpStructures(void)
{ 
  result_t result;
  int i;
  
  i = usbdev_buffer_create();
  assert(i == 0);
  
  device_initialize(&theUsbDev.dev);
  device_initialize(&theIntf.dev);
  
  // Set up the fields in theIntf and theUsbDev that we will need.
  
  theSetting.desc = *(struct usb_interface_descriptor *) &theNid.intfData.id;
  theIntf.dev.parent = &theUsbDev.dev;  
  theIntf.altsetting = &theSetting;     // we only know about one setting
  theIntf.cur_altsetting = &theSetting;
  theUsbDev.speed = theNid.intfData.speed;
  theUsbDev.devnum = theNid.intfData.deviceNum;
  theUsbDev.descriptor = *(struct usb_device_descriptor *) &theNid.intfData.dd;

  unsigned int numEndpoints = theNid.intfData.id.bNumEndpoints;
  // numEndpoints does not include EP0, NUM_EP does.
  if (numEndpoints != NUM_EP - 1)
    return -EINVAL;

  const size_t epSize = sizeof(capros_USB_EndpointDescriptor) * numEndpoints;
  capros_USB_EndpointDescriptor * epDescrs
    = (capros_USB_EndpointDescriptor *) alloca(epSize);
  if (!epDescrs)
    assert(false);// FIXME

  // No IDL for capros_USBInterface_getEndpointDescriptors yet.
  Message Msg = {
    .snd_invKey = KR_USBINTF,
    .snd_code = 2,    // for now
    .snd_w1 = 0,
    .snd_w2 = 0,
    .snd_w3 = 0,
    .snd_len = 0,
    .snd_key0 = KR_VOID,
    .snd_key1 = KR_VOID, 
    .snd_key2 = KR_VOID,
    .rcv_limit = epSize,
    .rcv_data = epDescrs,
    .rcv_key0 = KR_VOID,
    .rcv_key1 = KR_VOID,
    .rcv_key2 = KR_VOID,
    .rcv_rsmkey = KR_VOID,
  };
  result = CALL(&Msg);
  assert(result == RC_OK);
  assert(Msg.rcv_sent >= epSize);

  for (i = 0; i < numEndpoints; i++) {
    theEndpoints[i].desc = *(struct usb_endpoint_descriptor *)&epDescrs[i];
    unsigned int epNum = epDescrs[i].bEndpointAddress
                         & USB_ENDPOINT_NUMBER_MASK;
    if ((epDescrs[i].bEndpointAddress & USB_ENDPOINT_DIR_MASK) == USB_DIR_IN)
      theUsbDev.ep_in[epNum] = &theEndpoints[i];
    else
      theUsbDev.ep_out[epNum] = &theEndpoints[i];
  }
  return 0;
}

result_t
driver_main(void)
{
  /* We are entered here when the registry has a new device that is
  a potential match.
  This is the equivalent of the "probe" function.
  KR_ARG(0) is the interface cap.
  KR_RETURN is the resume cap to the registry.
  By this time the order code (OC_capros_USBDriverConstructor_probe)
  has been lost. */

  result_t result;
  int ret;
  Message Msg = {
    .snd_invKey = KR_RETURN,
    .snd_code = RC_OK,
    .snd_w1 = 0,
    .snd_w2 = 0,
    .snd_w3 = 0,
    .snd_key0 = KR_VOID,
    .snd_key1 = KR_VOID,
    .snd_key2 = KR_VOID,
    .snd_rsmkey = KR_VOID,
    .snd_len = 0,
    .rcv_key0 = KR_VOID,
    .rcv_key1 = KR_VOID,
    .rcv_key2 = KR_VOID,
    .rcv_rsmkey = KR_RETURN,
    .rcv_limit = sizeof(capros_USBDriverConstructorExtended_NewInterfaceData),
    .rcv_data = &theNid
  };

  printk("USB DS2490 driver called.\n");

  COPY_KEYREG(KR_ARG(0), KR_USBINTF);
    
  // In a coroutine, reply to the caller and ask for the
  // capros_USBDriverConstructorExtended_NewInterfaceData. 
  result = CALL(&Msg);
    
  assert(result == OC_capros_USBDriverConstructorExtended_sendID);
    
  ret = SetUpStructures();
  if (ret)
    goto nostructs;

  // Allocate the DMA memory we will need.
  cm = usb_buffer_alloc(NULL, sizeof(struct coherentMemory), GFP_KERNEL,
                        &coherentMemory_dma);
  if (!cm)
    goto failed;
    
  ret = ds_probe(&theIntf, (struct usb_device_id *)&theNid.deviceId);
  if (ret) {
    // Probe was unsuccessful. 
    goto failed;
  }
  // Probe was successful.

  // Create w1bus thread.
  result = lthread_new_thread(4096, &w1bus_thread, NULL, &w1bus_threadNum);
  if (result != RC_OK) {
    ds_disconnect(&theIntf);
failed:
    TearDownStructures();
nostructs:
    capros_USBInterface_probeFailed(KR_USBINTF);
    return RC_capros_USBDriverConstructor_ProbeUnsuccessful;
  }

  result = capros_Process_makeStartKey(KR_SELF, 0, KR_TEMP0);
  assert(result == RC_OK);
  result = capros_USBInterface_registerDriver(KR_USBINTF, KR_TEMP0);
  assert(result == RC_OK);

  // reply to the registry
  Msg.snd_invKey = KR_RETURN;
  Msg.snd_code = RC_OK;
  Msg.snd_key0 = KR_VOID;
  Msg.snd_key1 = KR_VOID;
  Msg.snd_key2 = KR_VOID;
  Msg.snd_rsmkey = KR_VOID;
  Msg.snd_len = 0;

  for (;;) {
    Msg.rcv_key0 = KR_VOID;
    Msg.rcv_key1 = KR_VOID;
    Msg.rcv_key2 = KR_VOID;
    Msg.rcv_rsmkey = KR_RETURN;
    Msg.rcv_limit = 0;

    RETURN(&Msg);

    // Set up defaults for return:
    Msg.snd_invKey = KR_RETURN;
    Msg.snd_code = RC_OK;
    Msg.snd_w1 = 0;
    Msg.snd_w2 = 0;
    Msg.snd_w3 = 0;
    Msg.snd_key0 = KR_VOID;
    Msg.snd_key1 = KR_VOID;
    Msg.snd_key2 = KR_VOID;
    Msg.snd_rsmkey = KR_VOID;
    Msg.snd_len = 0;

    switch (Msg.rcv_code) {
    default:
      Msg.snd_code = RC_capros_key_UnknownRequest;
      break;

    case OC_capros_key_getType:
      Msg.snd_w1 = IKT_capros_USBDriver;
      break;
  
    case OC_capros_USBDriver_disconnect:
      goto disconnect;
    }
  }

disconnect:
  ds_disconnect(&theIntf);
  TearDownStructures();
  return RC_OK;
}
