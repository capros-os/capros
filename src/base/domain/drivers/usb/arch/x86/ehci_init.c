/*
 * Copyright (C) 2010, Strawberry Development Group.
 *
 * This file is part of the CapROS Operating System.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this library; if not, write to the Free Software
 * Foundation, 59 Temple Place - Suite 330 Boston, MA 02111-1307, USA.
 */

#include <linuxk/linux-emul.h>
#include <linuxk/lsync.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <domain/assert.h>
#include <eros/Invoke.h>
#include <eros/machine/cap-instr.h>
#include <idl/capros/PCIDriverConstructorExtended.h>
#include <linux/usb.h>
#include <linux/pci.h>
#include <domain/CMTEMaps.h>
#include "../../core/cap.h"
#include "../../core/hcd.h"

#define dbg_init 0x1

/* Following should be an OR of some of the above */
#define dbg_flags   ( 0u | dbg_init)////

#define DEBUG(x) if (dbg_##x & dbg_flags)

struct pci_dev thePCIDev;
capros_PCIDriverConstructorExtended_NewDeviceData theNdd;


extern int ehci_hcd_init(void);

int
capros_hcd_initialization(void)
{
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
    .rcv_limit = sizeof(capros_PCIDriverConstructorExtended_NewDeviceData),
    .rcv_data = &theNdd
  };

  DEBUG (init) kprintf(KR_OSTREAM, "PCI USB driver called.\n");

  /* We are called by the PCI registry using capros_PCIDriverConstructor_probe.
  KR_RETURN is the resume cap to the registry.
  KR_ARG(0) has the PCIDev key.
  Move it to a safer place. */
  COPY_KEYREG(KR_ARG(0), KR_USB_PCIDev);

  // In a coroutine, reply to the caller and ask for the
  // capros_PCIDriverConstructorExtended_NewDeviceData. 
  result = CALL(&Msg);

  assert(result == OC_capros_PCIDriverConstructorExtended_sendID);

  ret = ehci_hcd_init();
  if (ret) {
    DEBUG (init) kprintf(KR_OSTREAM, "ehci_hcd_init returned %d\n", ret);
    return ret;
  }

  // Set up thePCIDev in this process to be similar to the
  // corresponding structure in the PCI process.

  // Compare with pci_scan_device.
  INIT_LIST_HEAD(&thePCIDev.bus_list);
  thePCIDev.bus = NULL;
  // devfn should not be used:
  thePCIDev.devfn = ~0;
  thePCIDev.vendor = theNdd.intfData.vendor;
  thePCIDev.device = theNdd.intfData.device;
int pci_setup_device(struct pci_dev *dev);
  ret = pci_setup_device(&thePCIDev);
  assert(!ret);	// FIXME
  thePCIDev.dma_mask = 0xffffffff;
  dev_set_name(&thePCIDev.dev, "PCI EHCI device");
  // pci_setup_device returns.
  // pci_scan_device returns.
  // pci_scan_single_device calls pci_device_add.

  pci_device_add(&thePCIDev, thePCIDev.bus);

  /* We are substituting for bus_attach_device.
  bus_attach_device checks p->drivers_autoprobe, which
  was previously set by bus_register, so bus_attach_device calls device_attach.
  There being no dev->driver yet, device_attach scans all the drivers
  on the dev's bus.
  In particular it finds the device_driver ehci_pci_driver.driver
  part of the pci_driver ehci_pci_driver
  that was registered by a call to pci_register_driver
  in ehci_hcd_init, called just above.
  device_attach calls __device_attach for that driver and this device.
  __device_attach first calls driver_match_device.
  ehci_pci_driver.driver, being registered by pci_register_driver, has the bus
  pci_bus_type, whose match function pci_bus_match is called,
  which calls pci_match_device, which calls pci_match_id,
  which scans a table of ids calling pci_match_one_device.
  The CapROS PCI registry did the equivalent and found this driver.
  __device_attach then calls driver_probe_device which calls really_probe.

  really_probe sees that there is a pci_bus_type.probe, namely pci_device_probe,
  so it calls that, which calls __pci_device_probe.
  really_probe also sets dev->driver
  and leaves it set if the probe was successful.
  There is no pci_dev->driver yet, and there is a ehci_pci_driver.probe,
  namely usb_hcd_pci_probe, so:
    __pci_device_probe calls pci_match_device (redundantly?),
    then calls pci_call_probe which (in the simple case) calls local_pci_probe
    which calls usb_hcd_pci_probe.
  (Whew!)
  We must call the probe function: */

extern struct pci_driver ehci_pci_driver;
extern const struct hc_driver ehci_pci_hc_driver;

  thePCIDev.dev.driver = &ehci_pci_driver.driver;
  thePCIDev.irq = theNdd.intfData.irq;
  // Populate the resources.
  uint8_t resourceNum;
  for (resourceNum = 0; ; resourceNum++) {
    uint32_t flags, size, firstIOPort;
    result = capros_PCIDev_getResource(KR_USB_PCIDev, resourceNum,
               &flags, &size, &firstIOPort, KR_TEMP2);
    if (result == RC_capros_PCIDev_InvalidResource)
      break;
    assert(result == RC_OK);
    DEBUG (init) kprintf(KR_OSTREAM,
                         "ehci got resource %d f=%#x s=%#x fip=%#x\n",
                         resourceNum, flags, size, firstIOPort);
    thePCIDev.resource[resourceNum].flags = flags;
    if (flags & capros_PCIDev_ResourceIO) {
      thePCIDev.resource[resourceNum].start = firstIOPort;
    } else if (flags & capros_PCIDev_ResourceMem) {
      // firstIOPort has offset of start of resource from start of page.
      resource_size_t end = firstIOPort + size;
      // Round up to page boundary:
      resource_size_t endPage = (end + (EROS_PAGE_SIZE - 1))
                       & ~ (resource_size_t)(EROS_PAGE_SIZE - 1);
      long pageNum = maps_reserveAndMapBlock(KR_TEMP2,
                                             endPage >> EROS_PAGE_LGSIZE);
      DEBUG (init) kprintf(KR_OSTREAM, "resource endPg=%#x pgNum=%#x\n",
                           (unsigned int)endPage, pageNum);
      if (pageNum < 0) {
        assert(false);	// FIXME
      }
      thePCIDev.resource[resourceNum].start
        = (resource_size_t)(size_t)maps_pgOffsetToAddr(pageNum) + firstIOPort;
    } else {	// no resource here
    }
    thePCIDev.resource[resourceNum].end
        = thePCIDev.resource[resourceNum].start + size - 1;
  }

  struct pci_device_id * id = (struct pci_device_id *) &theNdd.deviceId;
  id->driver_data = (unsigned long) &ehci_pci_hc_driver;
  ret = (*ehci_pci_driver.probe)(&thePCIDev, id);
  if (ret) {
    DEBUG (init) kprintf(KR_OSTREAM,
                         "ehci_pci_driver.probe returned %d (irq=%d)\n",
                         ret, thePCIDev.irq);
    thePCIDev.dev.driver = NULL;
  }

  return ret;
}
