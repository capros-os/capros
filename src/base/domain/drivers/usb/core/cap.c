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

#include <stdlib.h>
#include <linux/spinlock.h>
#include <linux/errno.h>
#include <linux/usb.h>

#include <domain/assert.h>
#include <eros/Invoke.h>
#include <idl/capros/Node.h>
#include <idl/capros/SuperNode.h>
#include <idl/capros/SpaceBank.h>
#include <idl/capros/Forwarder.h>
#include <idl/capros/Process.h>
#include <idl/capros/Errno.h>
#include <idl/capros/USBHCD.h>
#include <asm/USBIntf.h>

#include "hcd.h"
#include "usb.h"

unsigned long capros_Errno_ExceptionToErrno(unsigned long excep);
unsigned long capros_Errno_ErrnoToException(unsigned long errno);

#define MAX_ISO_PACKETS 100

// Allocate a receive buffer big enough for anything we might receive.
union {
  struct {
    capros_USB_urb urb;
    capros_USB_isoPacketDescriptor packets[MAX_ISO_PACKETS];
  } usbUrb;
} msgReceiveBuffer;

// Allocate a reply buffer big enough for anything we might send.
union {
  capros_USBInterface_urbResult urbResult;
} msgReplyBuffer;


// Translate an endpoint type from USB standard to pipe.
static inline unsigned int
epType_StdToPipe(unsigned int epType)
{
  static unsigned char xlate[4] = {
    PIPE_CONTROL,	// USB_ENDPOINT_XFER_CONTROL
    PIPE_ISOCHRONOUS,	// USB_ENDPOINT_XFER_ISOC
    PIPE_BULK,		// USB_ENDPOINT_XFER_BULK
    PIPE_INTERRUPT	// USB_ENDPOINT_XFER_INT
  };

  return xlate[epType];
}

void
SendNewInterface(unsigned long /* cap_t */ mainProc,
  struct usb_interface * intf)
{
  result_t result;
  Message Msg;
  Message * const msg = &Msg;  // to address it consistently
  capros_USBHCD_InterfaceData nid;
  struct usb_device * udev = interface_to_usbdev(intf);
  unsigned long fwdSlot = forwarderSlot(udev, intf->localInterfaceNum);

  result = capros_SuperNode_allocateRange(KR_KEYSTORE, fwdSlot, fwdSlot+1);
  assert(result == RC_OK); // FIXME handle error

  result = capros_Process_makeStartKey(mainProc,
             (udev->devnum << 8)
               + intf->localInterfaceNum,
             KR_TEMP0);
  assert(result == RC_OK);

  // Wrap it in a forwarder so we can rescind it later.
  result = capros_SpaceBank_alloc1(KR_BANK, capros_Range_otForwarder, KR_TEMP1);
  assert(result == RC_OK); // FIXME handle error
  result = capros_Forwarder_swapTarget(KR_TEMP1, KR_TEMP0, KR_VOID);
  assert(result == RC_OK);
  // Save the forwarder.
  result = capros_Node_swapSlotExtended(KR_KEYSTORE, fwdSlot, KR_TEMP1,
             KR_VOID);
  assert(result == RC_OK);

  result = capros_Forwarder_getOpaqueForwarder(KR_TEMP1, 0, KR_TEMP0);
  assert(result == RC_OK);

  struct usb_hcd * hcd = bus_to_hcd(theBus);
  nid.DMAMask = hcd->self.controller->coherent_dma_mask;
  nid.speed = udev->speed;
  nid.deviceNum = udev->devnum;
  nid.id = * (capros_USB_InterfaceDescriptor *) & intf->cur_altsetting->desc;
  nid.dd = * (capros_USB_DeviceDescriptor *) & udev->descriptor;
  msg->snd_key0 = KR_TEMP0;
  msg->snd_key1 = msg->snd_key2 = msg->snd_rsmkey = KR_VOID;
  msg->snd_w1 = msg->snd_w2 = msg->snd_w3 = 0;
  msg->snd_code = RC_OK;
  msg->snd_len = sizeof(nid);
  msg->snd_data = &nid;
  msg->snd_invKey = KR_RETURN;
  PSEND(msg);
}

void
newInterface(struct usb_interface * intf)
{
  result_t result;

  mutex_lock(&newInterfacesMutex);
  if (waitingForNewInterfaces) {
    // Deliver the new interface key.
    result = capros_Node_getSlotExtended(KR_KEYSTORE, LKSN_NIWC, KR_RETURN);
    assert(result == RC_OK);
    waitingForNewInterfaces = false;
  
    mutex_unlock(&newInterfacesMutex);
  
    // Target process for caps is the main process.
    result = capros_Node_getSlotExtended(KR_KEYSTORE,
      LKSN_THREAD_PROCESS_KEYS+0, KR_TEMP1);
    assert(result == RC_OK);
  
    SendNewInterface(KR_TEMP1, intf);

    // Init link
    intf->link.next = intf->link.prev = &intf->link;
  } else {
    list_add(&intf->link, &newInterfacesList);
    mutex_unlock(&newInterfacesMutex);
  }
}

static void
urbComplete(struct urb * urb, void * retData, unsigned int retSize)
{
  result_t result;

  capros_Node_extAddr_t resumeCap = urbToCap(urb);
  result = capros_Node_getSlotExtended(KR_KEYSTORE, resumeCap, KR_RETURN);
  assert(result == RC_OK);

  /* This simplest protocol would be to always CALL the resume key
  to the caller of submitUrb. When his "completion routine" is finished,
  he would return to us to signal he is done. 

  If there is any error on the urb, that is what we must do, because
  on an error the urb queue for that endpoint is halted, and
  the end of the completion routine signals that the urb queue
  can be restarted.

  As an optimization, if there is no error, we SEND rather than CALL,
  so the caller of submitUrb does not need to call us back. */

  Message msgs;
  Message * const msg = &msgs;  // to address it consistently
  msg->snd_key0 = msg->snd_key1 = msg->snd_key2 = msg->snd_rsmkey = KR_VOID;
  msg->snd_w1 = msg->snd_w2 = msg->snd_w3 = 0;
  msg->snd_code = RC_OK;
  msg->snd_len = retSize;
  msg->snd_data = retData;
  msg->snd_invKey = KR_RETURN;

  if (urb->status) {
    msg->rcv_key0 = msg->rcv_key1 = msg->rcv_key2 = msg->rcv_rsmkey = KR_VOID;
    msg->rcv_limit = 0;
    msg->invType = IT_PCall;
    INVOKECAP(msg);
  } else {
    PSEND(msg);
  }
}

static void
DeviceUrbCompletion(struct urb * urb)
{
  capros_USBInterface_urbResult urbres = {
    .status = urb->status,
    .actual_length = urb->actual_length,
    .interval = urb->interval
  };

  urbComplete(urb, &urbres, sizeof(urbres));
}

static void
DeviceIsoUrbCompletion(struct urb * urb)
{
  int i;
  const int numPkts = urb->number_of_packets;
  const size_t urbresSize = sizeof(capros_USBInterface_urbIsoResult)
                 + numPkts * sizeof(capros_USBInterface_isoBufferResult);

  capros_USBInterface_urbIsoResult * urbres = alloca(urbresSize);

  urbres->status = urb->status;
  urbres->interval = urb->interval;
  urbres->start_frame = urb->start_frame;
  urbres->error_count = urb->error_count;

  for (i = 0; i < numPkts; i++) {
    urbres->iso_frame_desc[i].status = urb->iso_frame_desc[i].status;
    urbres->iso_frame_desc[i].actual_length
     = urb->iso_frame_desc[i].actual_length;
  }

  urbComplete(urb, &urbres, urbresSize);
}

static struct usb_host_endpoint *
getEp(struct usb_device * udev, unsigned long endpoint)
{
  unsigned int epnum = usb_pipeendpoint(endpoint);
  if (usb_pipeout(endpoint)) {
    return udev->ep_out[epnum];
  } else {
    return udev->ep_in[epnum];
  }
}

static void
HandleSubmitUrb(Message * msg, struct usb_device * udev, int numPkts)
{
  result_t result;
  int i;

  if (msg->rcv_sent < sizeof(capros_USB_urb)
                      + numPkts * sizeof(capros_USB_isoPacketDescriptor) ) {
    msg->snd_code = RC_capros_key_RequestError;
    return;
  }

  capros_USB_urb * inputUrb = &msgReceiveBuffer.usbUrb.urb;

  struct usb_host_endpoint * ep = getEp(udev, inputUrb->endpoint);

  unsigned int epType = ep->desc.bmAttributes & USB_ENDPOINT_XFERTYPE_MASK;

  if ((! (inputUrb->transfer_flags
          & capros_USB_transferFlagsEnum_noTransferDMAMap)	// no xfer dma
       && inputUrb->transfer_buffer_length > 0 )	// and has an xfer
      || (! (inputUrb->transfer_flags
             & capros_USB_transferFlagsEnum_noSetupDMAMap)	// no setup dma
          && epType == USB_ENDPOINT_XFER_CONTROL ) ) {	// and has setup
    msg->snd_code = RC_capros_USB_NoDMA;
    return;
  }

  struct urb * urb = usb_alloc_urb(numPkts, GFP_KERNEL);
  if (! urb) {
    msg->snd_code = RC_capros_Errno_NoMem;
    return;
  }

  // Allocate storage for the caller's resume cap.
  // Use the address of the urb as a unique address for the cap.
  capros_Node_extAddr_t resumeCap = urbToCap(urb);
  result = capros_SuperNode_allocateRange(KR_KEYSTORE, resumeCap, resumeCap);
  if (result != RC_OK) {
    msg->snd_code = result;
    goto fail0;
  }

  urb->dev = udev;
  urb->pipe = (inputUrb->endpoint & 0x78080) // endpoint num and direction
              | (udev->devnum << 8)
              | (epType_StdToPipe(epType) << 30);
  urb->transfer_flags = inputUrb->transfer_flags;
  urb->transfer_buffer = 0;	// should not use this
  urb->transfer_dma = inputUrb->transfer_dma;
  urb->transfer_buffer_length = inputUrb->transfer_buffer_length;
  urb->setup_packet = 0;	// should not use this
  urb->setup_dma = inputUrb->setup_dma;
  // Perhaps we ought to check that the DMA addresses and lengths
  // are in fact in a kernel DMA block.

  // In case int:
  urb->interval = inputUrb->interval;
  urb->start_frame = -1;

  for (i = 0; i < numPkts; i++ ) {
    urb->iso_frame_desc[i].offset = inputUrb->iso_frame_desc[i].offset;
    urb->iso_frame_desc[i].length = inputUrb->iso_frame_desc[i].length;
  }

  urb->complete = numPkts ? &DeviceIsoUrbCompletion : &DeviceUrbCompletion;
  // urb->context unused

  // The completion routine could be called as soon as the urb is
  // successfully submitted, so get ready for it now.
  result = capros_Node_swapSlotExtended(KR_KEYSTORE, resumeCap, KR_RETURN,
                                        KR_VOID);
  assert(result == RC_OK);

  int ret = usb_submit_urb(urb, GFP_KERNEL);

  if (ret) {
    msg->snd_code = capros_Errno_ErrnoToException(-ret);
    result = capros_SuperNode_deallocateRange(KR_KEYSTORE,
               resumeCap, resumeCap);
    (void)result;
fail0:
    usb_free_urb(urb);
    return;
  }

  // urb was accepted. 
  msg->snd_invKey = KR_VOID;	// Don't return to caller
  msg->snd_code = RC_OK;
}

void
HandleGetEndpointDescriptors(Message * msg,
  struct usb_device * udev,
  struct usb_interface * intf)
{
  int i;

  struct usb_host_interface * setting = intf->cur_altsetting;
  unsigned int numEndpoints = setting->desc.bNumEndpoints;
  size_t dataSize = sizeof(capros_USB_EndpointDescriptor) * numEndpoints;
  capros_USB_EndpointDescriptor * eps
    = (capros_USB_EndpointDescriptor *) alloca(dataSize);
  if (!eps) {
    msg->snd_code = RC_capros_Errno_NoMem;
    return;
  }

  // Assemble the descriptors.
  for (i = 0; i < numEndpoints; i++) {
    eps[i] = *(capros_USB_EndpointDescriptor *) & setting->endpoint[i].desc;
  }

  // Return the data.
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
    .snd_len = dataSize,
    .snd_data = eps
  };
  PSEND(&Msg);

  msg->snd_invKey = KR_VOID;
}

/*
 * Start here.
 */
void
driver_main(void)
{
  int retval;
  result_t result;

  Message msgs;
  Message * const msg = &msgs;  // to address it consistently

  // Allocate slots for resume keys to waiters:
  result = capros_SuperNode_allocateRange(KR_KEYSTORE,
                          LKSN_NIWC, LKSN_NIWC);
  if (result != RC_OK) {
    assert(false);      // FIXME handle error
  }

  retval = ksuspend_usb_init();
  assert(!retval);

  retval = usb_hub_init();
  assert(!retval);

  extern int __init capros_hcd_initialization(void);
  if (capros_hcd_initialization()) {
    assert(false);    // FIXME handle error
  }

  msg->rcv_key0 = KR_ARG(0);
  msg->rcv_key1 = msg->rcv_key2 = KR_VOID;
  msg->rcv_rsmkey = KR_RETURN;
  msg->rcv_data = &msgReceiveBuffer;
  msg->rcv_limit = sizeof(msgReceiveBuffer);
  
  msg->snd_invKey = KR_VOID;
  msg->snd_key0 = msg->snd_key1 = msg->snd_key2 = msg->snd_rsmkey = KR_VOID;
  msg->snd_len = 0;
  /* The void key is not picky about the other parameters,
     so it's OK to leave them uninitialized. */
  
  for (;;) {
    RETURN(msg);

    // Set defaults for return.
    msg->snd_invKey = KR_RETURN;
    msg->snd_code = RC_OK;
    msg->snd_key0 = msg->snd_key1 = msg->snd_key2 = msg->snd_rsmkey = KR_VOID;
    msg->snd_w1 = msg->snd_w2 = msg->snd_w3 = 0;
    msg->snd_len = 0;

    if (msg->rcv_keyInfo == 0) {
      // USBHCD cap.
      switch (msg->rcv_code) {
      default:
        msg->snd_code = RC_capros_key_UnknownRequest;
        break;

      case OC_capros_key_getType:
        msg->snd_w1 = IKT_capros_USBHCD;
        break;

      case OC_capros_USBHCD_getDMAMask:
      {
        struct usb_hcd * hcd = bus_to_hcd(theBus);
        msg->snd_w1 = hcd->self.controller->coherent_dma_mask;
        break;
      }
      
      case OC_capros_USBHCD_getNewInterface:
        mutex_lock(&newInterfacesMutex);
        if (waitingForNewInterfaces) {
          msg->snd_code = RC_capros_USB_Already;	// someone already waiting
        } else if (list_empty(&newInterfacesList)) {
          // Make him wait.
          capros_Node_swapSlotExtended(KR_KEYSTORE, LKSN_NIWC,
            KR_RETURN, KR_VOID);
          waitingForNewInterfaces = true;
          msg->snd_invKey = KR_VOID;
        } else {
          // Take an interface off the list.
          struct usb_interface * intf
            = list_first_entry(&newInterfacesList, struct usb_interface, link);
          list_del_init(&intf->link);
          // Deliver the new interface key.
          SendNewInterface(KR_SELF, intf);
          msg->snd_invKey = KR_VOID;
        }
        mutex_unlock(&newInterfacesMutex);
      }
    } else {
      // USBInterface cap.
      // msg->rcv_keyInfo has the device number (1-127) in the low 8 bits
      // and the interface number in the high 8 bits.
      unsigned int devnum = msg->rcv_keyInfo >> 8;
      assert(devnum != 0 && devnum < 128);
      unsigned int localIntfNum = msg->rcv_keyInfo & 0xff;

      struct usb_device * udev = theBus->devmap.udev[devnum];
      assert(udev);
      assert(udev->devnum == devnum);
      assert(localIntfNum < udev->actconfig->desc.bNumInterfaces);
      struct usb_interface * intf = udev->actconfig->interface[localIntfNum];

      switch (msg->rcv_code) {
      default:
        msg->snd_code = RC_capros_key_UnknownRequest;
        break;

      case OC_capros_key_getType:
        msg->snd_w1 = IKT_capros_USBInterface;
        break;

      case 2:	// getEndpointDescriptors
      {
        HandleGetEndpointDescriptors(msg, udev, intf);
        break;
      }

      case OC_capros_USBInterface_registerDriver:
      {
        result_t result = capros_Node_swapSlotExtended(KR_KEYSTORE,
            driverSlot(udev, localIntfNum),
            KR_ARG(0), KR_VOID);
        assert(result == RC_OK);
        intf->condition = USB_INTERFACE_BOUND;
        break;
      }

      case OC_capros_USBInterface_probeFailed:
      {
	usb_unbind_interface(&intf->dev);
        break;
      }

      case OC_capros_USBInterface_submitUrb:
        HandleSubmitUrb(msg, udev, 0);
        break;

      case OC_capros_USBInterface_submitIsoUrb:
      {
        if (msg->rcv_sent < sizeof(capros_USB_urb)) {
          msg->snd_code = RC_capros_key_RequestError;
          break;;
        }

        capros_USB_urb * inputUrb = &msgReceiveBuffer.usbUrb.urb;
        int numPkts = inputUrb->number_of_packets;

        if (numPkts <= 0 || numPkts > MAX_ISO_PACKETS) {
          msg->snd_code = RC_capros_key_RequestError;
          break;;
        }

        HandleSubmitUrb(msg, udev, numPkts);
        break;
      }

      case OC_capros_USBInterface_unlinkQueuedUrbs:
      {
        struct usb_host_endpoint * ep = getEp(udev, msg->rcv_w1);
        struct usb_hcd * hcd = bus_to_hcd(theBus);
        epUnlinkQueuedUrbs(ep, hcd, -ECONNRESET);
        break;
      }

      case OC_capros_USBInterface_rejectUrbs:
      {
        struct usb_host_endpoint * ep = getEp(udev, msg->rcv_w1);
        struct usb_hcd * hcd = bus_to_hcd(theBus);
        ep->rejecting = true;
        epUnlinkQueuedUrbs(ep, hcd, -ENOENT);
        break;
      }

      case OC_capros_USBInterface_clearRejecting:
      {
        struct usb_host_endpoint * ep = getEp(udev, msg->rcv_w1);
        ep->rejecting = false;
        break;
      }

      case OC_capros_USBInterface_getDMAMask:
      {
        struct usb_hcd * hcd = bus_to_hcd(theBus);
        msg->snd_w1 = hcd->self.controller->coherent_dma_mask;
        break;
      }

      }
    }
  }
}

#if 0 // CapROS
/*
 * Cleanup
 */
static void __exit usb_exit(void)
{
	usb_deregister_device_driver(&usb_generic_driver);
	usb_major_cleanup();
	usbfs_cleanup();
	usb_deregister(&usbfs_driver);
	usb_devio_cleanup();
	usb_hub_cleanup();
	usb_host_cleanup();
	bus_unregister(&usb_bus_type);
	ksuspend_usb_cleanup();
}
#endif // CapROS
