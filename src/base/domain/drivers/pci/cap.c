/*
 * Copyright (C) 2010, Strawberry Development Group
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

#include <linuxk/lsync.h>
#include <domain/assert.h>
#include <eros/Invoke.h>
#include <idl/capros/key.h>
#include <idl/capros/Forwarder.h>
#include <idl/capros/Node.h>
#include <idl/capros/SuperNode.h>
#include <idl/capros/Process.h>
#include <idl/capros/SpaceBank.h>
//#include <idl/capros/Range.h>
#include <idl/capros/PCIBus.h>
#include <idl/capros/PCIDev.h>
#include <linux/pci.h>
#include <eros/machine/DevPrivs.h>

#define keyInfoPCIBus 0
#define keyInfoPCIDev 1

/* Define slots for keys in KR_KEYSTORE: */
#define LKSN_NIWC LKSN_APP	// resume cap to caller of getNewDevice
#define LKSN_INTERFACES LKSN_NIWC+1
/* Beginning at LKSN_INTERFACES, each interface has a pair of slots: */
static inline unsigned long
forwarderSlot(struct pci_dev * pdev)
{
  return LKSN_INTERFACES + ((pdev->bus->number << 8) + pdev->devfn) * 2;
}
static inline unsigned long
driverSlot(struct pci_dev * pdev)
{
  return forwarderSlot(pdev) + 1;
}

LIST_HEAD(newDevicesList);
DEFINE_MUTEX(newDevicesMutex);	// is this needed?
bool waitingForNewDevices = false;

void
SendNewInterface(unsigned long /* cap_t */ mainProc,
  struct pci_dev * pdev)
{
  result_t result;
  Message Msg;
  Message * const msg = &Msg;  // to address it consistently
  capros_PCIBus_DeviceData nid;
  unsigned long fwdSlot = forwarderSlot(pdev);

  result = capros_SuperNode_allocateRange(KR_KEYSTORE, fwdSlot, fwdSlot+1);
  assert(result == RC_OK); // FIXME handle error

  result = capros_Process_makeStartKey(mainProc, keyInfoPCIDev, KR_TEMP0);
  assert(result == RC_OK);

  // Wrap it in a forwarder so we can rescind it later,
  // and to hold the pdev address.
  result = capros_SpaceBank_alloc1(KR_BANK, capros_Range_otForwarder, KR_TEMP1);
  assert(result == RC_OK); // FIXME handle error
  result = capros_Forwarder_swapTarget(KR_TEMP1, KR_TEMP0, KR_VOID);
  assert(result == RC_OK);
  uint32_t unused;
  result = capros_Forwarder_swapDataWord(KR_TEMP1, (uint32_t)pdev, &unused);
  assert(result == RC_OK);
  // Save the forwarder.
  result = capros_Node_swapSlotExtended(KR_KEYSTORE, fwdSlot, KR_TEMP1,
             KR_VOID);
  assert(result == RC_OK);

  result = capros_Forwarder_getOpaqueForwarder(KR_TEMP1,
             capros_Forwarder_sendWord, KR_TEMP0);
  assert(result == RC_OK);

  // Initialize nid.
  nid.vendor = pdev->vendor;
  nid.device = pdev->device;
  nid.subsystemVendor = pdev->subsystem_vendor;
  nid.subsystemDevice = pdev->subsystem_device;
  nid.deviceClass = pdev->class;
printk("New PCI dev irq=%d\n", pdev->irq);////
  nid.irq = pdev->irq;

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
newPCIDevice(struct pci_dev * pdev)
{
  result_t result;

  mutex_lock(&newDevicesMutex);
  if (waitingForNewDevices) {
    // Deliver the new interface key.
    result = capros_Node_getSlotExtended(KR_KEYSTORE, LKSN_NIWC, KR_RETURN);
    assert(result == RC_OK);
    waitingForNewDevices = false;
  
    mutex_unlock(&newDevicesMutex);
  
    // Target process for caps is the main process.
    result = capros_Node_getSlotExtended(KR_KEYSTORE,
      LKSN_THREAD_PROCESS_KEYS+0, KR_TEMP1);
    assert(result == RC_OK);
  
    SendNewInterface(KR_TEMP1, pdev);

    // Init link
    pdev->link.next = pdev->link.prev = &pdev->link;
  } else {
    list_add(&pdev->link, &newDevicesList);
    mutex_unlock(&newDevicesMutex);
  }
}

void
ReadConfig(struct pci_dev * pdev,
  Message * msg, int len)
{
  // Compare with pci_read_config_byte, _word, and _dword.
  u32 data = 0;
  int where = msg->rcv_w1;
  if (where & (len - 1)) {
    msg->snd_code = RC_capros_PCIDev_ConfigAddrMisaligned;
  } else {
    // No need to lock because this is single-threaded.
    // We do not support multiple domains.
    int retval = pdev->bus->ops->read(pdev->bus, pdev->devfn, where, len,
                   &data);
    switch (retval) {
    default:	// unexpected return value
      kprintf(KR_OSTREAM, "%s:%d: Got %d\n", __FILE__, __LINE__, retval);
      // fall into -EINVAL case
    case -EINVAL:
      msg->snd_code = RC_capros_PCIDev_ConfigInval;
      break;
    case 0:
      msg->snd_w1 = data;
      // msg->snd_code is RC_OK
      break;
    }
  }
}

void
WriteConfig(struct pci_dev * pdev,
  Message * msg, int len)
{
  // Compare with pci_write_config_byte, _word, and _dword.
  int where = msg->rcv_w1;
  if (where & (len - 1)) {
    msg->snd_code = RC_capros_PCIDev_ConfigAddrMisaligned;
  } else {
    // No need to lock because this is single-threaded.
    // We do not support multiple domains.
    int retval = pdev->bus->ops->write(pdev->bus, pdev->devfn, where, len,
                   msg->rcv_w2);
    switch (retval) {
    default:	// unexpected return value
      kprintf(KR_OSTREAM, "%s:%d: Got %d\n", __FILE__, __LINE__, retval);
      // fall into -EINVAL case
    case -EINVAL:
      msg->snd_code = RC_capros_PCIDev_ConfigInval;
      break;
    case 0:
      // msg->snd_code is RC_OK
      break;
    }
  }
}

// This is called after architecture-specific initialization.
// It does not return.
void
pci_main(void)
{
  result_t result;
  int retval;
  Message Msg;
  Message * const msg = &Msg;  // to address it consistently

  // Allocate slots for resume keys to waiters:
  result = capros_SuperNode_allocateRange(KR_KEYSTORE,
                          LKSN_NIWC, LKSN_NIWC);
  if (result != RC_OK) {
    assert(false);      // FIXME handle error
  }

  msg->rcv_key0 = msg->rcv_key1 = msg->rcv_key2 = KR_VOID;
  msg->rcv_rsmkey = KR_RETURN;
  msg->rcv_limit = 0;
  
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

    // Get pdev from forwarder's data word.
    struct pci_dev * pdev = (struct pci_dev *) msg->rcv_w3;
    switch (msg->rcv_keyInfo) {
    case keyInfoPCIBus:
      switch (msg->rcv_code) {
      default:
        msg->snd_code = RC_capros_key_UnknownRequest;
        break;

      case OC_capros_key_getType:
        msg->snd_w1 = IKT_capros_PCIBus;
        break;

      case OC_capros_PCIBus_getNewDevice:
        mutex_lock(&newDevicesMutex);
        if (waitingForNewDevices) {
          msg->snd_code = RC_capros_PCIBus_Already;	// someone already waiting
        } else if (list_empty(&newDevicesList)) {
          // Make him wait.
          capros_Node_swapSlotExtended(KR_KEYSTORE, LKSN_NIWC,
            KR_RETURN, KR_VOID);
          waitingForNewDevices = true;
          msg->snd_invKey = KR_VOID;
        } else {
          // Take an interface off the list.
          struct pci_dev * pdev
            = list_first_entry(&newDevicesList, struct pci_dev, link);
          list_del_init(&pdev->link);
          // Deliver the new device key.
          SendNewInterface(KR_SELF, pdev);
          msg->snd_invKey = KR_VOID;
        }
        mutex_unlock(&newDevicesMutex);
        break;
      }
      break;

    case keyInfoPCIDev:
      switch (msg->rcv_code) {
      default:
        msg->snd_code = RC_capros_key_UnknownRequest;
        break;

      case OC_capros_key_getType:
        msg->snd_w1 = IKT_capros_PCIDev;
        break;

      case OC_capros_PCIDev_readConfig8:
        ReadConfig(pdev, msg, 1);
        break;

      case OC_capros_PCIDev_readConfig16:
        ReadConfig(pdev, msg, 2);
        break;

      case OC_capros_PCIDev_readConfig32:
        ReadConfig(pdev, msg, 4);
        break;

      case OC_capros_PCIDev_writeConfig8:
        WriteConfig(pdev, msg, 1);
        break;

      case OC_capros_PCIDev_writeConfig16:
        WriteConfig(pdev, msg, 2);
        break;

      case OC_capros_PCIDev_writeConfig32:
        WriteConfig(pdev, msg, 4);
        break;

      case OC_capros_PCIDev_getResource: ;
        uint32_t resourceNum = msg->rcv_w1;
        if (resourceNum >= PCI_NUM_RESOURCES)
          msg->snd_code = RC_capros_PCIDev_InvalidResource;
        else {
          unsigned long flags = pci_resource_flags(pdev, resourceNum);
          if (flags & IORESOURCE_IO) {
            msg->snd_w1 = capros_PCIDev_ResourceIO;
            msg->snd_w2 = pci_resource_end(pdev, resourceNum) + 1
                          - pci_resource_start(pdev, resourceNum);
            msg->snd_w3 = pci_resource_start(pdev, resourceNum);
          } else if (flags & IORESOURCE_MEM) {
            // Round to page boundaries.
            resource_size_t startPage = pci_resource_start(pdev, resourceNum)
                              & ~ (resource_size_t)(EROS_PAGE_SIZE - 1);
            resource_size_t endPage = (pci_resource_end(pdev, resourceNum)
                                + (EROS_PAGE_SIZE - 1))
                              & ~ (resource_size_t)(EROS_PAGE_SIZE - 1);
            // Use the following address as the location of the cap
            // in KR_KEYSTORE.
            capros_Node_extAddr_t pageCap
                = (cap_t) &pdev->resourceMemPublished[resourceNum];

            if (! pdev->resourceMemPublished[resourceNum]) {
              result = capros_SuperNode_allocateRange(KR_KEYSTORE,
                         pageCap, pageCap);
              assert(result == RC_OK);	// FIXME
              result = capros_DevPrivs_publishMem(KR_DEVPRIVS,
                         startPage, endPage, flags & IORESOURCE_READONLY,
                         KR_TEMP0);
              assert(result == RC_OK);	// FIXME
              result = capros_Node_swapSlotExtended(KR_KEYSTORE, pageCap,
                         KR_TEMP0, KR_VOID);
              assert(result == RC_OK);
            } else {
              result = capros_Node_getSlotExtended(KR_KEYSTORE, pageCap,
                         KR_TEMP0);
              assert(result == RC_OK);
            }

            msg->snd_w1 = capros_PCIDev_ResourceMem
                          | (flags & IORESOURCE_PREFETCH);
            assert(IORESOURCE_PREFETCH == capros_PCIDev_ResourcePrefetch);
            msg->snd_w2 = pci_resource_end(pdev, resourceNum) + 1
                          - pci_resource_start(pdev, resourceNum);
            msg->snd_w3 = pci_resource_start(pdev, resourceNum) - startPage;
            msg->snd_key0 = KR_TEMP0;
          } else {
            msg->snd_w1 = 0;
            msg->snd_w2 = 0;
            msg->snd_w3 = 0;
          }
        }
        break;

      case OC_capros_PCIDev_enable:
        retval = pci_enable_device(pdev);
        switch (retval) {
        default:
          kprintf(KR_OSTREAM,
                  "pci: pci_enable_device returned %d, unexpected\n", retval);
        case -EINVAL:
          msg->snd_code = RC_capros_PCIDev_ResourceConflict;
        case 0:
          break;
        }
        break;

      case OC_capros_PCIDev_disable:
        pci_disable_device(pdev);
        break;
      }
      break;
    }
  }
}
