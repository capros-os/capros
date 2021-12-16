/*
 * Copyright (C) 2008, Strawberry Development Group.
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

/* USB test.
*/

#include <eros/target.h>
#include <eros/Invoke.h>
#include <idl/capros/USBHCD.h>
#include <idl/capros/USBInterface32.h>
#include <idl/capros/SpaceBank.h>
#include <idl/capros/GPT.h>
#include <idl/capros/SuperNode.h>
#include <idl/capros/Sleep.h>
#include <idl/capros/DevPrivs.h>

#include <linux/usb/ch9.h>

#include <domain/Runtime.h>

#include <domain/domdbg.h>
#include <domain/assert.h>

#define KR_USBHCD  KR_APP(0)
#define KR_OSTREAM KR_APP(1)
#define KR_SLEEP   KR_APP(2)
#define KR_DEVPRIVS KR_APP(3)
#define KR_SEG     KR_APP(4)
#define KR_USBIntf KR_APP(5)

#define dmaVirtAddr 0x1d000


const uint32_t __rt_stack_pointer = 0x20000;
const uint32_t __rt_unkept = 1;

#define ckOK \
  if (result != RC_OK) { \
    kdprintf(KR_OSTREAM, "Line %d result is 0x%08x!\n", __LINE__, result); \
  }

struct capros_USBHCD_InterfaceData nid;
#define devDescr nid.dd
#define intfDescr nid.id

int
main(void)
{
  int i;
  result_t result;
  capros_key_type theType;
  //unsigned long err;

  kprintf(KR_OSTREAM, "Starting.\n");

  result = capros_key_getType(KR_USBHCD, &theType);
  ckOK
  assert(theType == IKT_capros_USBHCD);

  unsigned long dmaMask;
  result = capros_USBHCD_getDMAMask(KR_USBHCD, &dmaMask);
  ckOK
  kprintf(KR_OSTREAM, "dmaMask=0x%x\n", dmaMask);

  // Allocate one page of DMA memory.
  capros_DevPrivs_addr_t physAddr;
  result = capros_DevPrivs_allocateDMAPages(KR_DEVPRIVS, 1,
             dmaMask, &physAddr, KR_TEMP0);
  ckOK

  // Map it.
  result = capros_GPT_setSlot(KR_SEG, dmaVirtAddr >> EROS_PAGE_LGSIZE,
                              KR_TEMP0);
  ckOK

  kprintf(KR_OSTREAM, "Waiting for interface key...\n");

  result = capros_USBHCD_getNewInterface(KR_USBHCD, &nid, KR_USBIntf);
  ckOK

  result = capros_key_getType(KR_USBIntf, &theType);
  ckOK
  assert(theType == IKT_capros_USBInterface);

  kprintf(KR_OSTREAM, "Got Device. dd.Class %d dd.SubClass %d dd.Protocol %d\n",
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

  // Try sending an urb.
  struct usb_ctrlrequest * setup = (struct usb_ctrlrequest *)dmaVirtAddr;
  setup->bRequestType = USB_DIR_IN | USB_TYPE_STANDARD | USB_RECIP_DEVICE;
  setup->bRequest = USB_REQ_GET_DESCRIPTOR;
  setup->wValue = USB_DT_DEVICE << 8;
  setup->wIndex = 0;
  setup->wLength = USB_DT_DEVICE_SIZE;

  capros_USBInterface32_urb urb;
  urb.endpoint = USB_DIR_IN;
  urb.transfer_flags = capros_USB_transferFlagsEnum_noTransferDMAMap
                     | capros_USB_transferFlagsEnum_noSetupDMAMap;
  urb.transfer_dma = physAddr + sizeof(*setup);
  urb.transfer_buffer_length = 18;
  urb.setup_dma = physAddr;
  urb.interval = 0;

  struct capros_USBInterface_urbResult urbres;
  result = capros_USBInterface32_submitUrb(KR_USBIntf, urb,
             KR_VOID, KR_VOID, KR_VOID, KR_TEMP0, &urbres);
  ckOK
  kprintf(KR_OSTREAM, "Status = %d, len=%d, ", urbres.status,
          urbres.actual_length);
  uint16_t * data16 = (uint16_t *)&setup[1];
  for (i = 0; i < setup->wLength / 2; i++) {
    kprintf(KR_OSTREAM, "0x%04x ", data16[i]);
  }

  kprintf(KR_OSTREAM, "\nDone.\n");

  return 0;
}

