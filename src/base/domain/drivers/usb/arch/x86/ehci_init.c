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
#include <idl/capros/Process.h>
#include <idl/capros/Constructor.h>
#include <idl/capros/Node.h>
#include <linux/usb.h>
#include <linux/pci.h>
#include <domain/PCIDrvr.h>
#include "../../core/hcd.h"

#define dbg_init 0x1

/* Following should be an OR of some of the above */
#define dbg_flags   ( 0u | dbg_init)////

#define DEBUG(x) if (dbg_##x & dbg_flags)


extern int ehci_hcd_init(void);

int
capros_hcd_initialization(void)
{
  int ret;
  result_t result;

  DEBUG (init) kprintf(KR_OSTREAM, "PCI USB driver called.\n");

  PCIDriver_mainInit("PCI EHCI");

  ret = ehci_hcd_init();
  if (ret) {
    DEBUG (init) kprintf(KR_OSTREAM, "ehci_hcd_init returned %d\n", ret);
    return ret;
  }

  // Create USB Registry.
  // KC_APP2(0) has the constructor.
  result = capros_Node_getSlotExtended(KR_CONSTIT, KC_APP2(0), KR_TEMP0);
  assert(result == RC_OK);
  // Give it the USBHCD key.
  result = capros_Process_makeStartKey(KR_SELF, 0, KR_TEMP1);
  assert(result == RC_OK);
  result = capros_Constructor_request(KR_TEMP0, KR_BANK, KR_SCHED, KR_TEMP1,
             KR_VOID);
  assert(result == RC_OK);	// FIXME

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
