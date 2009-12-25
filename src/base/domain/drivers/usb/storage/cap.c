/*
 * Copyright (C) 2008, 2009, Strawberry Development Group.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2, or (at your option) any
 * later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 675 Mass Ave, Cambridge, MA 02139, USA.
 */
/* This material is based upon work supported by the US Defense Advanced
Research Projects Agency under Contract No. W31P4Q-07-C-0070.
Approved for public release, distribution unlimited. */

#include <alloca.h>
#include <linux/dma-mapping.h>
#include <scsi/scsi_cmnd.h>
#include <scsi/scsi_device.h>

#include <domain/assert.h>
#include <eros/Invoke.h>
#include <eros/cap-instr.h>
#include <idl/capros/USBDriverConstructor.h>
#include <idl/capros/USBDriverConstructorExtended.h>
#include <idl/capros/USBDriver.h>
#include <idl/capros/DevPrivs.h>
#include <asm/USBIntf.h>
#include <asm/SCSIDev.h>
#include <asm/SCSICtrl.h>
#include <asm/DMA.h>
#include <idl/capros/Process.h>
#include <idl/capros/Node.h>
#include <idl/capros/Errno.h>

#include "usb.h"
#include "scsiglue.h"
#include "debug.h"

/* Procedures in usb.c: */
int storage_probe(struct usb_interface *intf, 
                         const struct usb_device_id *id,
                         unsigned long deviceIDIndex);
void __exit usb_stor_exit(void);

capros_SCSIControl_SCSIHostTemplate capros_host_template = {
  .maxTransferSize = 240 * 512,
  .maxQueuedCommands = 1,
  .maxCommandsPerLun = 1,
  .reservedID = capros_SCSIControl_ThisIdNone,
  .maxScatterGatherSegments = capros_SCSIControl_SG_ALL,
  .useClustering = true,
  .emulated = true,
  .hostHandlesSettleDelay = true,
  .coherentDMAMask = DMA_BIT_MASK(capros_DMA_DMAAddressBits)
};

/* We will only queue one command at a time, so we can statically allocate
the srb.
We only transfer one area at a time, so we statically allocate
the scatterlist. */
static struct scatterlist theSG;
static struct scsi_cmnd theSRB;

unsigned long srbOpaque;

struct {
  struct Scsi_Host h;
  unsigned char u[sizeof(struct us_data)];
} hostAlloc = {
  .h = {
    .host_lock = &hostAlloc.h.default_lock,
    .max_id = 8
  }
};
struct Scsi_Host * theHost = &hostAlloc.h;
struct scsi_device theDevice = {
  .host = &hostAlloc.h
};

capros_USBDriverConstructorExtended_NewInterfaceData theNid;

/* N.B.: the following are different instances from the ones
in the core USB HCD. */
struct usb_host_interface theSetting;
struct usb_interface theIntf;
struct usb_device theUsbDev;

static void
TearDownStructures(void)
{
  usbdev_buffer_destroy();
}

static void
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
  theIntf.altsetting = &theSetting;	// we only know about one setting
  theIntf.cur_altsetting = &theSetting;
  theUsbDev.speed = theNid.intfData.speed;
  theUsbDev.devnum = theNid.intfData.deviceNum;
  theUsbDev.descriptor = *(struct usb_device_descriptor *) &theNid.intfData.dd;

  unsigned int numEndpoints = theNid.intfData.id.bNumEndpoints;
  // Endpoint 0 has no descriptor. There may not be any other endpoints:
  if (numEndpoints > 0) {
    struct usb_host_endpoint * theEndpoints;
    theEndpoints = (struct usb_host_endpoint *)
        kzalloc(sizeof(struct usb_host_endpoint) * numEndpoints, GFP_KERNEL);
    if (!theEndpoints)
      assert(false);// FIXME
    theSetting.endpoint = theEndpoints;

    const size_t epSize = sizeof(capros_USB_EndpointDescriptor) * numEndpoints;
    capros_USB_EndpointDescriptor * epDescrs
      = (capros_USB_EndpointDescriptor *) alloca(epSize);
    if (!epDescrs)
      assert(false);// FIXME

    // No IDL for capros_USBInterface_getEndpointDescriptors yet.
    Message Msg = {
      .snd_invKey = KR_USBINTF,
      .snd_code = 2,	// for now
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
  }
}

union {
  capros_SCSIDevice_SCSICommand cmd;
} MsgRcvBuf;

/* Note, we say we allow one queued command per lun, but the implementation
 * allows only one queued command period.
 * So we'd better hope there is only one lun per device. */

enum {
  cmdActive_no,
  cmdActive_yes		// LKSN_COMMANDREPLY has a resume cap
};
// commandActive is shared by multiple threads, so make it atomic:
atomic_t commandActive = ATOMIC_INIT(cmdActive_no);

static void
srb_done(struct scsi_cmnd * srb)
{
  result_t result;

  result = capros_Node_getSlotExtended(KR_KEYSTORE, LKSN_COMMANDREPLY,
		KR_TEMP0);
  assert(result == RC_OK);

  // LKSN_COMMANDREPLY is now free:
  atomic_set(&commandActive, cmdActive_no);

  Message Msg = {
    .snd_invKey = KR_TEMP0,
    .snd_code = RC_OK,
    .snd_w1 = srbOpaque,
    .snd_w2 = srb->result,
    .snd_w3 = scsi_bufflen(srb) - scsi_get_resid(srb),	// transferCount
    .snd_key0 = KR_VOID,
    .snd_key1 = KR_VOID,
    .snd_key2 = KR_VOID,
    .snd_rsmkey = KR_VOID,
    .snd_len = sizeof(capros_SCSIDevice_senseBuffer),
    .snd_data = &srb->sense_buffer
  };
  US_DEBUGP("srb_done returning\n");
  SEND(&Msg);	// non-prompt
}

static void
DoReadWrite(Message * msg, bool write)
{
  result_t result;

  capros_SCSIDevice_SCSICommand * sc = &MsgRcvBuf.cmd;

  if (msg->rcv_sent < sizeof(capros_SCSIDevice_SCSICommand)
      || sc->cmd_len > sizeof(MsgRcvBuf.cmd.cmnd) ) {
    msg->snd_code = RC_capros_key_RequestError;
    return;
  }

  if (atomic_read(&commandActive) != cmdActive_no) {
#if 1	// if catching bugs
    kdprintf(KR_OSTREAM, "SCSI Store I/O %#x busy", sc);
#endif
    msg->snd_code = RC_capros_Errno_Already;
    return;
  }

  // Set up the scsi_cmnd structure for the rest of the code. 
  memset(&theSRB, 0, sizeof(theSRB));
  theSRB.sdb.table.sgl = &theSG;
  theSRB.sdb.table.nents = 1;
  theSRB.sdb.table.orig_nents = 1;
  theSRB.cmnd = &theSRB.__cmd[0];
  memcpy(theSRB.cmnd, sc->cmnd, sc->cmd_len);
  theSRB.cmd_len = sc->cmd_len;
  theSRB.sdb.table.nents = 1;	// only one scatter/gather area
  theSRB.sc_data_direction = write ? DMA_TO_DEVICE : DMA_FROM_DEVICE;
  theSRB.sdb.length = sc->request_bufflen;
  sg_init_one(&theSG, NULL, sc->request_buffer_dma, sc->request_bufflen);
  srbOpaque = sc->opaque;

  theSRB.device = &theDevice;
  theDevice.lun = sc->lun;
  theDevice.id = sc->id;
  // Save the return cap now, in case queuecommand calls srb_done right away.
  /* Save in KR_KEYSTORE, because srb_done may be called from a
  different thread. */
  result = capros_Node_swapSlotExtended(KR_KEYSTORE, LKSN_COMMANDREPLY,
		KR_RETURN, KR_VOID);
  assert(result == RC_OK);
  atomic_set(&commandActive, cmdActive_yes);

  scsi_lock(theHost);
  int ret = queuecommand(&theSRB, &srb_done);
  scsi_unlock(theHost);
  switch (ret) {
  case SCSI_MLQUEUE_HOST_BUSY:
    atomic_set(&commandActive, cmdActive_no);
    // Shouldn't we have caught this above?
    msg->snd_code = RC_capros_Errno_Already;
    return;

  case 0:
    msg->snd_invKey = KR_VOID;
    break;

  default:
    assert(false);
  }
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

  printk("USB Storage driver called.\n");

  COPY_KEYREG(KR_ARG(0), KR_USBINTF);

  result = capros_Node_getSlotExtended(KR_CONSTIT, KC_SCSICONTROL,
             KR_SCSICONTROL);
  assert(result == RC_OK);

  // In a coroutine, reply to the caller and ask for the
  // capros_USBDriverConstructorExtended_NewInterfaceData. 
  result = CALL(&Msg);

  assert(result == OC_capros_USBDriverConstructorExtended_sendID);

  SetUpStructures();

  ret = storage_probe(&theIntf, (struct usb_device_id *)&theNid.deviceId,
                      theNid.deviceIdIndex);

  if (ret) {
    // Probe was unsuccessful. 
    capros_USBInterface_probeFailed(KR_USBINTF);
    return RC_capros_USBDriverConstructor_ProbeUnsuccessful;
  }
  // Probe was successful.
  result = capros_Process_makeStartKey(KR_SELF, keyInfoUSBDriver, KR_TEMP0);
  assert(result == RC_OK);
  result = capros_USBInterface_registerDriver(KR_USBINTF, KR_TEMP0);
  assert(result == RC_OK);

  capros_DevPrivs_declarePFHProcess(KR_DEVPRIVS, KR_SELF);

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
    Msg.rcv_data = &MsgRcvBuf;
    Msg.rcv_limit = sizeof(MsgRcvBuf);

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

    switch (Msg.rcv_keyInfo) {
    case keyInfoUSBDriver:
      switch (Msg.rcv_code) {
      default:
        Msg.snd_code = RC_capros_key_UnknownRequest;
        break;

      case OC_capros_key_getType:
        Msg.snd_w1 = IKT_capros_USBDriver;
        break;

      case OC_capros_USBDriver_disconnect:
        goto disconnect;

      case OC_capros_USBDriver_suspend:
assert(false);//// need to implement
        break;

      case OC_capros_USBDriver_resume:
assert(false);//// need to implement
        break;
      }
      break;

    case keyInfoSCSIDevice:
      switch (Msg.rcv_code) {
      default:
        Msg.snd_code = RC_capros_key_UnknownRequest;
        break;

      case OC_capros_key_getType:
        Msg.snd_w1 = IKT_capros_SCSIDeviceAny;
        break;

      case OC_capros_SCSIDevice32_getDMAMask:
      {
        capros_USBInterface32_dma_addr_t dmamask;
        result = capros_USBInterface32_getDMAMask(KR_USBINTF, &dmamask);
        assert(result == RC_OK);
        Msg.snd_w1 = dmamask;
        break;
      }

      case OC_capros_SCSIDevice_Read:
        DoReadWrite(&Msg, false);
        break;

      case OC_capros_SCSIDevice_Write:
        DoReadWrite(&Msg, true);
        break;
      }
      break;

    default: assert(false);
    }
  }

disconnect:
  usb_stor_disconnect(&theIntf);
  usb_stor_exit();
  TearDownStructures();
  // FIXME: there is a memory leak here somewhere
  return RC_OK;
}
