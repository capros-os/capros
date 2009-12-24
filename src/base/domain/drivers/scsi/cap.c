/*
 * Copyright (C) 2008, 2009, Strawberry Development Group
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
//#include <linux/spinlock.h>
//#include <linux/errno.h>
#include <scsi/scsi_host.h>

#include <domain/assert.h>
#include <eros/Invoke.h>
#include <idl/capros/Node.h>
#include <idl/capros/Forwarder.h>
#include <idl/capros/SuperNode.h>
#include <idl/capros/SpaceBank.h>
#include <idl/capros/Process.h>
#include <idl/capros/Errno.h>
#include <asm/SCSICtrl.h>
#include <idl/capros/SCSIHost.h>
#include <idl/capros/SCSIDevice32.h>

#include "scsi_priv.h"

#define keyInfoSCSIDevice 1

static inline unsigned int
boolToBit(bool in)
{
  return in != 0;
}

unsigned long capros_Errno_ExceptionToErrno(unsigned long excep);
unsigned long capros_Errno_ErrnoToException(unsigned long errno);

// From block/scsi_ioctl.c:
/* Command group 3 is reserved and should never be used.  */
const unsigned char scsi_command_size_tbl[8] =
{
        6, 10, 10, 12,
        16, 12, 10, 10
};

// Allocate a receive buffer big enough for anything we might receive.
union {
  capros_SCSIControl_SCSIHostTemplate hostt;
} msgReceiveBuffer;

// Allocate a reply buffer big enough for anything we might send.
union {
} msgReplyBuffer;


/*
 * Start here.
 */
void
driver_main(void)
{
  int retval;
  result_t result;

  Message Msg;
  Message * const msg = &Msg;  // to address it consistently

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
      // SCSIControl cap.
      switch (msg->rcv_code) {
      default:
        msg->snd_code = RC_capros_key_UnknownRequest;
        break;

      case OC_capros_key_getType:
        msg->snd_w1 = IKT_capros_SCSIControlAny;
        break;

      case OC_capros_SCSIControl_addHost:
      {
	/* This does the equivalent of Linux scsi_host_alloc AND
	scsi_add_host. */
        if (msg->rcv_sent < sizeof(capros_SCSIControl_SCSIHostTemplate)) {
          msg->snd_code = RC_capros_key_RequestError;
          break;
        }
        capros_SCSIControl_SCSIHostTemplate * capros_hostt
          = &msgReceiveBuffer.hostt;

        // Translate the CapROS template to a Linux template.
	struct scsi_host_template * linux_hostt
	  = kzalloc(sizeof(struct scsi_host_template), GFP_KERNEL);
	if (!linux_hostt) {
          msg->snd_code = RC_capros_Errno_NoMem;
          break;
	}
	linux_hostt->max_sectors = capros_hostt->maxTransferSize / 512;
	linux_hostt->can_queue   = capros_hostt->maxQueuedCommands;
	linux_hostt->cmd_per_lun = capros_hostt->maxCommandsPerLun;
        linux_hostt->this_id     = capros_hostt->reservedID;
	linux_hostt->sg_tablesize = capros_hostt->maxScatterGatherSegments;
	linux_hostt->use_clustering = boolToBit(capros_hostt->useClustering);
	linux_hostt->emulated    = boolToBit(capros_hostt->emulated);
	linux_hostt->skip_settle_delay
	  = boolToBit(capros_hostt->hostHandlesSettleDelay);

	linux_hostt->name = "unknown";

	struct Scsi_Host * shost = scsi_host_alloc(linux_hostt, 0);
	if (! shost) {
          msg->snd_code = RC_capros_Errno_NoMem;
	  goto noshost;
	}

	// Create a forwarder to hold the shost addr.
	result = capros_SpaceBank_alloc1(KR_BANK,
	  capros_Range_otForwarder, KR_TEMP1);
	if (result != RC_OK) {
          msg->snd_code = result;
	  goto noForwarder;
	}
	result = capros_Process_makeStartKey(KR_SELF, keyInfoSCSIDevice,
	           KR_TEMP0);
	assert(result == RC_OK);
	result = capros_Forwarder_swapTarget(KR_TEMP1, KR_TEMP0, KR_VOID);
	assert(result == RC_OK);
	result = capros_Forwarder_swapDataWord(KR_TEMP1,
	           (unsigned long)shost, 0);
	assert(result == RC_OK);

	struct device * parentDev = kzalloc(sizeof(struct device),
					GFP_KERNEL);
	if (!parentDev) {
          msg->snd_code = RC_capros_Errno_NoMem;
	  goto noParentDev;
        }
	device_initialize(parentDev);
	parentDev->coherent_dma_mask = capros_hostt->coherentDMAMask;

        // The SCSIDevice cap will be stored in the keystore.
        const capros_Node_extAddr_t devCapSlot = SCSIDevCapSlot(shost);
        result = capros_SuperNode_allocateRange(KR_KEYSTORE,
                   devCapSlot, devCapSlot );
        if (result != RC_OK) {
	  msg->snd_code = result;
	  goto noSnode;
        }

	retval = scsi_add_host(shost, parentDev);
	if (retval) {
          msg->snd_code = capros_Errno_ErrnoToException(retval);
          capros_SuperNode_deallocateRange(KR_KEYSTORE,
		(capros_Node_extAddr_t)shost,
		(capros_Node_extAddr_t)shost );
noSnode:
	  kfree(parentDev);
noParentDev:
	  capros_SpaceBank_free1(KR_BANK, KR_TEMP1);
noForwarder:
	  scsi_host_put(shost);
noshost:
	  kfree(linux_hostt);
          break;
	}

        // save the SCSIDevice cap
        result = capros_Node_swapSlotExtended(KR_KEYSTORE, devCapSlot,
                   KR_ARG(0), KR_VOID);

        // Do we need to store the non-opaque forwarder too?
	result = capros_Forwarder_getOpaqueForwarder(KR_TEMP1,
	           capros_Forwarder_sendWord, KR_TEMP1);
	assert(result == RC_OK);
	msg->snd_key0 = KR_TEMP1;

        break;
      }
      }	// end of switch (msg->rcv_code)
    } else {
      // SCSIHost cap.

      // The forwarder provided the following:
      struct Scsi_Host * shost = (struct Scsi_Host *) msg->rcv_w3;

      switch (msg->rcv_code) {
      default:
        msg->snd_code = RC_capros_key_UnknownRequest;
        break;

      case OC_capros_key_getType:
        msg->snd_w1 = IKT_capros_SCSIHost;
        break;

      case OC_capros_SCSIHost_scanHost:
      {
	// FIXME: Is it safe to do the following synchronously?
        // What if there is a bus reset during the scan?
	scsi_scan_host(shost);
        break;
      }

      case OC_capros_SCSIHost_reportBusReset:
      {
        break;
      }

      case OC_capros_SCSIHost_reportDeviceReset:
      {
        break;
      }

      case OC_capros_SCSIHost_removeHost:
      {
        break;
      }
      } // end of switch (msg->rcv_code)
    }
  }
}
