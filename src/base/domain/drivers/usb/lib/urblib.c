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
#include <idl/capros/Process.h>
#include <idl/capros/USBHCD.h>
#include <asm/USBIntf.h>

//#include "hcd.h"
//#include "usb.h"
#include "usbdev.h"

unsigned long capros_Errno_ExceptionToErrno(unsigned long excep);
unsigned long capros_Errno_ErrnoToException(unsigned long errno);

/* This file defines the usb functions expected by Linux device drivers. */

// From urb.c:

#define to_urb(d) container_of(d, struct urb, kref)

static void urb_destroy(struct kref *kref)
{
	struct urb *urb = to_urb(kref);
	kfree(urb);
}

/**
 * usb_init_urb - initializes a urb so that it can be used by a USB driver
 * @urb: pointer to the urb to initialize
 *
 * Initializes a urb so that the USB subsystem can use it properly.
 *
 * If a urb is created with a call to usb_alloc_urb() it is not
 * necessary to call this function.  Only use this if you allocate the
 * space for a struct urb on your own.  If you call this function, be
 * careful when freeing the memory for your urb that it is no longer in
 * use by the USB core.
 *
 * Only use this function if you _really_ understand what you are doing.
 */
void usb_init_urb(struct urb *urb)
{
	if (urb) {
		memset(urb, 0, sizeof(*urb));
		kref_init(&urb->kref);
		spin_lock_init(&urb->lock);
	}
}

/**
 * usb_alloc_urb - creates a new urb for a USB driver to use
 * @iso_packets: number of iso packets for this urb
 * @mem_flags: the type of memory to allocate, see kmalloc() for a list of
 *	valid options for this.
 *
 * Creates an urb for the USB driver to use, initializes a few internal
 * structures, incrementes the usage counter, and returns a pointer to it.
 *
 * If no memory is available, NULL is returned.
 *
 * If the driver want to use this urb for interrupt, control, or bulk
 * endpoints, pass '0' as the number of iso packets.
 *
 * The driver must call usb_free_urb() when it is finished with the urb.
 */
struct urb *usb_alloc_urb(int iso_packets, gfp_t mem_flags)
{
	struct urb *urb;

	urb = kmalloc(sizeof(struct urb) +
		iso_packets * sizeof(struct usb_iso_packet_descriptor),
		mem_flags);
	if (!urb) {
		err("alloc_urb: kmalloc failed");
		return NULL;
	}
	usb_init_urb(urb);
	return urb;
}

/**
 * usb_free_urb - frees the memory used by a urb when all users of it are finished
 * @urb: pointer to the urb to free, may be NULL
 *
 * Must be called when a user of a urb is finished with it.  When the last user
 * of the urb calls this function, the memory of the urb is freed.
 *
 * Note: The transfer buffer associated with the urb is not freed, that must be
 * done elsewhere.
 */
void usb_free_urb(struct urb *urb)
{
	if (urb)
		kref_put(&urb->kref, urb_destroy);
}
		
		
/*-------------------------------------------------------------------*/

/**
 * usb_submit_urb - issue an asynchronous transfer request for an endpoint
 * @urb: pointer to the urb describing the request
 * @mem_flags: the type of memory to allocate, see kmalloc() for a list
 *	of valid options for this.
 *
 * This submits a transfer request, and transfers control of the URB
 * describing that request to the USB subsystem.  Request completion will
 * be indicated later, asynchronously, by calling the completion handler.
 * The three types of completion are success, error, and unlink
 * (a software-induced fault, also called "request cancellation").  
 *
 * URBs may be submitted in interrupt context.
 *
 * The caller must have correctly initialized the URB before submitting
 * it.  Functions such as usb_fill_bulk_urb() and usb_fill_control_urb() are
 * available to ensure that most fields are correctly initialized, for
 * the particular kind of transfer, although they will not initialize
 * any transfer flags.
 *
 * Successful submissions return 0; otherwise this routine returns a
 * negative error number.  If the submission is successful, the complete()
 * callback from the URB will be called exactly once, when the USB core and
 * Host Controller Driver (HCD) are finished with the URB.  When the completion
 * function is called, control of the URB is returned to the device
 * driver which issued the request.  The completion handler may then
 * immediately free or reuse that URB.
 *
 * With few exceptions, USB device drivers should never access URB fields
 * provided by usbcore or the HCD until its complete() is called.
 * The exceptions relate to periodic transfer scheduling.  For both
 * interrupt and isochronous urbs, as part of successful URB submission
 * urb->interval is modified to reflect the actual transfer period used
 * (normally some power of two units).  And for isochronous urbs,
 * urb->start_frame is modified to reflect when the URB's transfers were
 * scheduled to start.  Not all isochronous transfer scheduling policies
 * will work, but most host controller drivers should easily handle ISO
 * queues going from now until 10-200 msec into the future.
 *
 * For control endpoints, the synchronous usb_control_msg() call is
 * often used (in non-interrupt context) instead of this call.
 * That is often used through convenience wrappers, for the requests
 * that are standardized in the USB 2.0 specification.  For bulk
 * endpoints, a synchronous usb_bulk_msg() call is available.
 *
 * Request Queuing:
 *
 * URBs may be submitted to endpoints before previous ones complete, to
 * minimize the impact of interrupt latencies and system overhead on data
 * throughput.  With that queuing policy, an endpoint's queue would never
 * be empty.  This is required for continuous isochronous data streams,
 * and may also be required for some kinds of interrupt transfers. Such
 * queuing also maximizes bandwidth utilization by letting USB controllers
 * start work on later requests before driver software has finished the
 * completion processing for earlier (successful) requests.
 *
 * As of Linux 2.6, all USB endpoint transfer queues support depths greater
 * than one.  This was previously a HCD-specific behavior, except for ISO
 * transfers.  Non-isochronous endpoint queues are inactive during cleanup
 * after faults (transfer errors or cancellation).
 *
 * Reserved Bandwidth Transfers:
 *
 * Periodic transfers (interrupt or isochronous) are performed repeatedly,
 * using the interval specified in the urb.  Submitting the first urb to
 * the endpoint reserves the bandwidth necessary to make those transfers.
 * If the USB subsystem can't allocate sufficient bandwidth to perform
 * the periodic request, submitting such a periodic request should fail.
 *
 * Device drivers must explicitly request that repetition, by ensuring that
 * some URB is always on the endpoint's queue (except possibly for short
 * periods during completion callacks).  When there is no longer an urb
 * queued, the endpoint's bandwidth reservation is canceled.  This means
 * drivers can use their completion handlers to ensure they keep bandwidth
 * they need, by reinitializing and resubmitting the just-completed urb
 * until the driver longer needs that periodic bandwidth.
 *
 * Memory Flags:
 *
 * The general rules for how to decide which mem_flags to use
 * are the same as for kmalloc.  There are four
 * different possible values; GFP_KERNEL, GFP_NOFS, GFP_NOIO and
 * GFP_ATOMIC.
 *
 * GFP_NOFS is not ever used, as it has not been implemented yet.
 *
 * GFP_ATOMIC is used when
 *   (a) you are inside a completion handler, an interrupt, bottom half,
 *       tasklet or timer, or
 *   (b) you are holding a spinlock or rwlock (does not apply to
 *       semaphores), or
 *   (c) current->state != TASK_RUNNING, this is the case only after
 *       you've changed it.
 * 
 * GFP_NOIO is used in the block io path and error handling of storage
 * devices.
 *
 * All other situations use GFP_KERNEL.
 *
 * Some more specific rules for mem_flags can be inferred, such as
 *  (1) start_xmit, timeout, and receive methods of network drivers must
 *      use GFP_ATOMIC (they are called with a spinlock held);
 *  (2) queuecommand methods of scsi drivers must use GFP_ATOMIC (also
 *      called with a spinlock held);
 *  (3) If you use a kernel thread with a network driver you must use
 *      GFP_NOIO, unless (b) or (c) apply;
 *  (4) after you have done a down() you can use GFP_KERNEL, unless (b) or (c)
 *      apply or your are in a storage driver's block io path;
 *  (5) USB probe and disconnect can use GFP_KERNEL unless (b) or (c) apply; and
 *  (6) changing firmware on a running storage or net device uses
 *      GFP_NOIO, unless b) or c) apply
 *
 */
/* Submit an urb and wait for the completion routine to finish. */
int usb_submit_urb_wait(struct urb * urb, gfp_t mem_flags)
{
  result_t result;
  int pipe, temp;
  int numPkts = 0;

	if (!urb || urb->hcpriv || !urb->complete)
		return -EINVAL;

	urb->status = -EINPROGRESS;
	urb->actual_length = 0;

	pipe = urb->pipe;
	temp = usb_pipetype(pipe);

  size_t caprosUrbSize;
  capros_USB_urb * caprosUrb;

  if (temp == PIPE_ISOCHRONOUS) {
    numPkts = urb->number_of_packets;

    if (numPkts <= 0)
      return -EINVAL;

    caprosUrbSize = sizeof(capros_USB_urb)
            + numPkts * sizeof(capros_USB_isoPacketDescriptor);
  } else {
    caprosUrbSize = sizeof(capros_USB_urb);
  }

  caprosUrb = (capros_USB_urb *)alloca(caprosUrbSize);
  caprosUrb->endpoint = urb->pipe;
  caprosUrb->transfer_flags = urb->transfer_flags;
  caprosUrb->transfer_dma = urb->transfer_dma;
  caprosUrb->transfer_buffer_length = urb->transfer_buffer_length;
  caprosUrb->setup_dma = urb->setup_dma;
  caprosUrb->interval = urb->interval;

  if (temp == PIPE_ISOCHRONOUS) {
    int i;

    for (i = 0; i < numPkts; i++) {
      caprosUrb->iso_frame_desc[i].offset = urb->iso_frame_desc[i].offset;
      caprosUrb->iso_frame_desc[i].length = urb->iso_frame_desc[i].length;
    }
    caprosUrb->number_of_packets = numPkts;
    caprosUrb->start_frame = urb->start_frame;

    capros_USBInterface_urbIsoResult urbResult;
    result = capros_USBInterface_submitIsoUrb(KR_USBINTF, *caprosUrb,
               KR_VOID, KR_VOID, KR_VOID, KR_TEMP0, &urbResult);
    if (result == RC_OK) {
      urb->status = urbResult.status;
      urb->interval = urbResult.interval;
      urb->start_frame = urbResult.start_frame;
      urb->error_count = urbResult.error_count;
      for (i = 0; i < numPkts; i++) {
        urb->iso_frame_desc[i].status = urbResult.iso_frame_desc[i].status;
        urb->iso_frame_desc[i].actual_length
          = urbResult.iso_frame_desc[i].actual_length;
      }
    }
  } else {
    capros_USBInterface_urbResult urbResult;
    result = capros_USBInterface_submitUrb(KR_USBINTF, *caprosUrb,
               KR_VOID, KR_VOID, KR_VOID, KR_TEMP0, &urbResult);
    if (result == RC_OK) {
      urb->status = urbResult.status;
if (urb->status) printk("%s: urb status %d", __FUNCTION__, urb->status);////
      urb->actual_length = urbResult.actual_length;
      urb->interval = urbResult.interval;
    }
  }

  if (result != RC_OK) {
    // urb was not accepted
    unsigned long errno = capros_Errno_ExceptionToErrno(result);
    if (errno) {
printk("%s: returning errno %d from submitUrb", __FUNCTION__, errno);////
      return -errno;
    }
    // unknown errno
    kdprintf(KR_OSTREAM, "submitUrb got rc=0x%08x\n", result);////
    return -ENODEV;
  } else {
    long st = urb->status;
    // urb was accepted and transfer is now done.
    // BEWARE: KR_TEMP0 is live!
    (*urb->complete)(urb);
    if (st) {
      // If accepted and completed with nonzero status,
      // we must return to KR_TEMP0 after the completion routine. 
      Message Msg = {
        .snd_invKey = KR_TEMP0,
        .snd_code = RC_OK,
        .snd_w1 = 0,
        .snd_w2 = 0,
        .snd_w3 = 0,
        .snd_key0 = KR_VOID,
        .snd_key1 = KR_VOID,
        .snd_key2 = KR_VOID,
        .snd_rsmkey = KR_VOID,
        .snd_len = 0
      };
      PSEND(&Msg);
    }
    return 0;
  }
}


// Instead of usb_unlink_urb:
void
usb_unlink_endpoint(unsigned long endpoint)
{
  result_t result = capros_USBInterface_unlinkQueuedUrbs(KR_USBINTF, endpoint);
  assert(result == RC_OK);
}

// Instead of usb_kill_urb:
void
usb_kill_endpoint(unsigned long endpoint)
{
  result_t result = capros_USBInterface_rejectUrbs(KR_USBINTF, endpoint);
  assert(result == RC_OK);
}

void
usb_clear_rejecting(unsigned long endpoint)
{
  result_t result = capros_USBInterface_clearRejecting(KR_USBINTF, endpoint);
  assert(result == RC_OK);
}
