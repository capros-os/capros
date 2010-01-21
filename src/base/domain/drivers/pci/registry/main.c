/*
 * Copyright (C) 2009, Strawberry Development Group
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

/*
 * The PCI registry receives PCIDevice caps and matches them with drivers.
 */

#include <string.h>
#include <linux/mod_devicetable.h>
#include <eros/Invoke.h>
#include <idl/capros/Node.h>
#include <idl/capros/PCIBus.h>
#include <idl/capros/PCIDriverConstructor.h>

#include <domain/assert.h>

#define KR_PCIBus         KR_APP(0)
#define KR_OSTREAM        KR_APP(1)
#define KR_RequestorSnode KR_APP(2)
#define KR_PCIDev         KR_APP(5)

/* Bypass all the usual initialization. */
unsigned long __rt_stack_pointer = 0x20000;
unsigned long __rt_runtime_hook = 0;
uint32_t __rt_unkept = 1;

struct pci_driver_registration {
  struct pci_device_id * id_table;
  capros_Node_extAddr_t driver_requestor;
};

/* For now, we have a static list of drivers.
   Eventually the registry will accept new drivers
   and grow the list dynamically. */

extern struct pci_device_id usb_ehci_pci_ids;

struct pci_driver_registration drivers[] = {
  {&usb_ehci_pci_ids, 0},
//  {&eth_pci_ids, 1}
};
#define NUM_DRIVERS (sizeof(drivers) / sizeof(drivers[0]))

const struct pci_device_id * pci_match_id(capros_PCIBus_DeviceData * nid,
					  const struct pci_device_id * id)
{
  for (;
       id->vendor || id->subvendor || id->class_mask;
       id++) {
    // Compare the following with pci_match_one_device().
    if ((id->vendor == PCI_ANY_ID || id->vendor == nid->vendor) &&
        (id->device == PCI_ANY_ID || id->device == nid->device) &&
        (id->subvendor == PCI_ANY_ID || id->subvendor == nid->subsystemVendor) &&
        (id->subdevice == PCI_ANY_ID || id->subdevice == nid->subsystemDevice) &&
        !((id->class ^ nid->deviceClass) & id->class_mask))
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
    capros_PCIDriverConstructorExtended_NewDeviceData nid;
    result = capros_PCIBus_getNewDevice(KR_PCIBus,
               &nid.intfData, KR_PCIDev);
    assert(result == RC_OK);

    for (i = 0; i < NUM_DRIVERS; i++) {	// loop over all drivers
      const struct pci_device_id * id;
      id = pci_match_id(&nid.intfData, drivers[i].id_table);
      if (id) {		// found a match
        // Create an instance of the driver and pass the interface cap.
        // This is equivalent to Linux driver->probe().
        nid.deviceIdIndex = id - drivers[i].id_table;
        memcpy(&nid.deviceId, id, sizeof(nid.deviceId));
        result = capros_Node_getSlotExtended(KR_RequestorSnode,
                   drivers[i].driver_requestor,
                   KR_TEMP0);
        assert(result == RC_OK);
        result = capros_PCIDriverConstructor_probe(KR_TEMP0,
                   KR_BANK, KR_SCHED, KR_PCIDev,
                   KR_VOID, KR_VOID, KR_VOID, KR_TEMP0);
        if (result == RC_OK) {
          // Driver called us back to get nid.
          result = capros_PCIDriverConstructorExtended_sendID(KR_TEMP0, nid);
        }
        switch (result) {
        case RC_OK:
          kprintf(KR_OSTREAM, "Driver %d returned OK.\n", i);////
          goto found;

        default:
          kprintf(KR_OSTREAM, "Driver %d returned 0x%x\n", i, result);
        case RC_capros_PCIDriverConstructor_ProbeUnsuccessful:
          break;	// continue trying drivers
        }
      }
    }
    kprintf(KR_OSTREAM, "No driver for PCI device:\n");
#define dd nid.intfData
    kprintf(KR_OSTREAM, "Vendor %u Device %u SubV %u SubD %u Class %#x\n",
            dd.vendor, dd.device, dd.subsystemVendor,
            dd.subsystemDevice, dd.deviceClass);
#undef dd

    found: ;
  }
}
