/*
 * Copyright (c) 2004 Evgeniy Polyakov <johnpol@2ka.mipt.ru>
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

#include <linux/kernel.h>
#include <asm/types.h>
#include <linux/timer.h>

#include "ds2490.h"
#include <eros/Invoke.h>
#include <idl/capros/Node.h>
#include <idl/capros/Process.h>
#include <domain/assert.h>
#include <asm/USBIntf.h>

static void usb_w1_msg_timer_function(unsigned long data)
{
  usb_unlink_endpoint(data);
}

/* Returns 0 or a negative error number. */
static int ds_usb_io(capros_USB_urb * urb,
  unsigned long endpoint,	// endpoint # and direction
  unsigned int timeout,
  unsigned long * actual_length)
{
  result_t result;
  struct timer_list tim;

  urb->endpoint = endpoint;
  urb->transfer_flags =   capros_USB_transferFlagsEnum_noTransferDMAMap
                        | capros_USB_transferFlagsEnum_noSetupDMAMap;
  capros_USBInterface_urbResult urbResult;

  setup_timer(&tim, usb_w1_msg_timer_function, endpoint);
  mod_timer(&tim, jiffies + timeout);

  result = capros_USBInterface_submitUrb(KR_USBINTF, *urb,
             KR_VOID, KR_VOID, KR_VOID, KR_TEMP0, &urbResult);

  del_timer(&tim);

  if (result != RC_OK) {	// urb was not accepted
    unsigned long errno = capros_Errno_ExceptionToErrno(result);
    if (errno) {
      return -errno;
    }
    // unknown errno
    kdprintf(KR_OSTREAM, "submitUrb got rc=0x%08x\n", result);
    return -ENODEV;
  }

  if (actual_length)
    *actual_length = urbResult.actual_length;

  if (urbResult.status) {
    Message Msg = {
      .snd_invKey = KR_TEMP0,
      .snd_code = RC_OK,	// any value will do
      .snd_key0 = KR_VOID,
      .snd_key1 = KR_VOID,
      .snd_key2 = KR_VOID,
      .snd_rsmkey = KR_VOID,
      .snd_len = 0
    };
    PSEND(&Msg);
#if 0
    printk("submitUrb got status %d", urbResult.status);
#endif
    return urbResult.status;	// a negative error number
  }

  return 0;
}

static int ds_send_usb_control(u8 request, u16 value, u16 index)
{
  cm->ctrlreq.bRequestType = 0x40;
  cm->ctrlreq.bRequest = request;
  cm->ctrlreq.wValue = cpu_to_le16p(&value);
  cm->ctrlreq.wIndex = cpu_to_le16p(&index);
  cm->ctrlreq.wLength = 0;

  capros_USB_urb urb = {
    .transfer_buffer_length = 0,
    .setup_dma = DMAAddress(ctrlreq)
  };

  return ds_usb_io(&urb, theDSDev.ep[EP_CONTROL] << 15, 1000, 0);
}

static int ds_send_control_cmd(u16 value, u16 index)
{
	return ds_send_usb_control(CONTROL_CMD, value, index);
}

void
ModeCommand(u16 value, u16 index)
{
  int err = ds_send_usb_control(MODE_CMD, value, index);
  assert(!err);	// FIXME handle this somewhere
}

static int ds_send_control(u16 value, u16 index)
{
	return ds_send_usb_control(COMM_CMD, value, index);
}

/* Return value:
If < 0, a negative error number.
If >=0, the number of bytes received into *status. */
static int ds_recv_status_nodump(void)
{
  unsigned long count;
  int err;

  // Set up interrupt urb.
  int interval = theDSDev.udev->ep_in[EP_STATUS]->desc.bInterval;

  memset(&cm->status, 0, sizeof(cm->status));

  capros_USB_urb urb = {
    .transfer_dma = DMAAddress(status),
    .transfer_buffer_length = sizeof(struct ds_status),
    .interval = theDSDev.udev->speed == USB_SPEED_HIGH ? 1 << (interval - 1)
                                                   : interval
  };

  err = ds_usb_io(&urb, (theDSDev.ep[EP_STATUS] << 15) | USB_DIR_IN,
          100, &count);
  if (err < 0) {
	printk(KERN_ERR "Failed to read 1-wire data from 0x%x: err=%d.\n", theDSDev.ep[EP_STATUS], err);
	return err;
  }

  return count;
}

static inline void ds_dump_status(uint8_t val, char * str)
{
	printk("%45s: %02x\n", str, val);
}

void
dump_status(int count)
{
	struct ds_status * st = &cm->status;
	int i;

	printk("0x%x: count=%d, results: ", theDSDev.ep[EP_STATUS], count);
	for (i=0; i<count-16; ++i)
		printk("%02x ", st->results[i]);
	printk("\n");

	if (count >= 16) {
		ds_dump_status(st->enable, "enable flag");
		ds_dump_status(st->speed,  "1-wire speed");
		ds_dump_status(st->pullup_dur, "strong pullup duration");
		ds_dump_status(st->ppuls_dur, "programming pulse duration");
		ds_dump_status(st->pulldown_slew, "pulldown slew rate control");
		ds_dump_status(st->write1_time, "write-1 low time");
		ds_dump_status(st->write0_time,
                               "data sample offset/write-0 recovery time");
		ds_dump_status(st->reserved0, "reserved (test register)");
		ds_dump_status(st->status, "device status flags");
		ds_dump_status(st->command0, "communication command byte 1");
		ds_dump_status(st->command1, "communication command byte 2");
		ds_dump_status(st->command_buffer_status,
                               "communication command buffer status");
		ds_dump_status(st->data_out_buffer_status,
                               "1-wire data output buffer status");
		ds_dump_status(st->data_in_buffer_status,
                               "1-wire data input buffer status");
		ds_dump_status(st->reserved1, "reserved");
		ds_dump_status(st->reserved2, "reserved");
	}
}

static int ds_recv_status(void)
{
	int count, err = 0;

	count = ds_recv_status_nodump();
	if (count < 0)
		return err;

	struct ds_status * st = &cm->status;
	dump_status(count);

	if (st->status & ST_EPOF) {
		printk(KERN_INFO "Resetting device after ST_EPOF.\n");
		err = ds_send_control_cmd(CTL_RESET_DEVICE, 0);
		if (err)
			return err;
		count = ds_recv_status_nodump();
		if (count < 0)
			return err;
	}
#if 0
	if (st->status & ST_IDLE) {
		printk(KERN_INFO "Resetting pulse after ST_IDLE.\n");
		err = ds_start_pulse(PULLUP_PULSE_DURATION);
		if (err)
			return err;
	}
#endif

	return err;
}

static int ds_recv_data(dma_addr_t buf_dma, int len)
{
  unsigned long count;
  int err;

  // Set up bulk receive.
  unsigned long endpoint = (theDSDev.ep[EP_DATA_IN] << 15) | USB_DIR_IN;
  capros_USB_urb urb = {
    .transfer_dma = buf_dma,
    .transfer_buffer_length = len,
  };

  err = ds_usb_io(&urb, endpoint, 1000, &count);
  if (err < 0) {
	printk(KERN_INFO "err=%d, Clearing ep0x%x.\n", err,
		theDSDev.ep[EP_DATA_IN]);
	usb_clear_halt(NULL, endpoint);
	ds_recv_status();
	return err;
  }

#if 0
	{
		int i;

		printk("%s: count=%d: ", __func__, count);
		for (i=0; i<count; ++i)
			printk("%02x ", buf[i]);
		printk("\n");
	}
#endif
  return count;
}

static int ds_send_data(dma_addr_t buf_dma, int len)
{
  unsigned long count;
  int err;

  // Set up bulk send.
  capros_USB_urb urb = {
    .transfer_dma = buf_dma,
    .transfer_buffer_length = len,
  };

  err = ds_usb_io(&urb, (theDSDev.ep[EP_DATA_OUT] << 15),
          1000, &count);

  if (err < 0) {
	printk(KERN_ERR "Failed to read 1-wire data from 0x02: err=%d.\n", err);
	return err;
  }

  return err;
}

#if 0

int ds_stop_pulse(int limit)
{
	struct ds_status st;
	int count = 0, err = 0;

	do {
		err = ds_send_control(CTL_HALT_EXE_IDLE, 0);
		if (err)
			break;
		err = ds_send_control(CTL_RESUME_EXE, 0);
		if (err)
			break;
		err = ds_recv_status_nodump();
		if (err)
			break;

		if ((st.status & ST_SPUA) == 0) {
			err = ModeCommand(MOD_PULSE_EN, 0);
			if (err)
				break;
		}
	} while(++count < limit);

	return err;
}

#endif  /*  0  */

/* Returns:
If > 0: number of status bytes read.
If == 0: failed to become idle after 100 tries.
If < 0: a negative error code. */
static int waitStatus(void)
{
	int err, count = 0;

	struct ds_status * st = &cm->status;
	do {
		err = ds_recv_status_nodump();
		if (err < 0)
			return err;	// return negative error
#if 0
		dump_status(err);
#endif
	} while(!(st->status & ST_IDLE)
	        && ++count < 100);

#if 0
	printk("cnt %d", count);
#endif
	if (count >= 100) {
		dump_status(err);
		return 0;
	}
#if 0
	if (err != 16) {
	  dump_status(err);
	}
#endif
	return err;	// return positive number of bytes
}

static int ds_wait_status(void)
{
	struct ds_status * st = &cm->status;

	int err = waitStatus();

	if (((err > 16) && (st->results[0] & RES_NRS))
	    || err <= 0) {
		ds_recv_status();
		return -1;
	} else
		return 0;
}

static result_t ds_reset(void)
{
	struct ds_status * st = &cm->status;
	int err;

	err = ds_send_control(COMM_1_WIRE_RESET | COMM_F | COMM_IM, 0);
	if (err)
		return capros_Errno_ErrnoToException(-err);;

	err = waitStatus();
	if (err > 16) {
		if (st->results[0] & RES_SH)
			return RC_capros_W1Bus_BusShorted;
		if (st->results[0] & RES_APP)
			return RC_capros_W1Bus_AlarmingPresencePulse;
		if (st->results[0] & RES_NRS)
			return RC_capros_W1Bus_NoDevicePresent;
	}
	assert(err != 0);	// FIXME handle or report
	if (err < 0)
		return capros_Errno_ErrnoToException(-err);;
	return RC_OK;
}

static int ds_start_pulse(int delay)
{
	int err;
	u8 del = 1 + (u8)(delay >> 4);

#if 0
	err = ds_stop_pulse(10);
	if (err)
		return err;

	err = ModeCommand(MOD_PULSE_EN, PULSE_SPUE);
	if (err)
		return err;
#endif
	err = ds_send_control(COMM_SET_DURATION | COMM_IM, del);
	if (err)
		return err;

	err = ds_send_control(COMM_PULSE | COMM_IM | COMM_F, 0);
	if (err)
		return err;

	mdelay(delay);

	ds_wait_status();

	return err;
}

static int ds_touch_bit(u8 bit, u8 *tbit)
{
	int err, count;
	struct ds_status * st = &cm->status;
	u16 value = (COMM_BIT_IO | COMM_IM) | ((bit) ? COMM_D : 0);
	u16 cmd;

	err = ds_send_control(value, 0);
	if (err)
		return err;

	count = 0;
	do {
		err = ds_wait_status();
		if (err)
			return err;

		cmd = st->command0 | (st->command1 << 8);
	} while (cmd != value && ++count < 10);

	if (err < 0 || count >= 10) {
		printk(KERN_ERR "Failed to obtain status.\n");
		return -EINVAL;
	}

	err = ds_recv_data(DMAAddress(dataBuffer), 1);
	if (err < 0)
		return err;

	*tbit = cm->dataBuffer[0];
	return 0;
}

static int ds_write_bit(u8 bit)
{
	int err;

	err = ds_send_control(COMM_BIT_IO | COMM_IM | (bit) ? COMM_D : 0, 0);
//// add ICP bit?
	if (err)
		return err;

	ds_wait_status();

	return 0;
}

static int ds_write_byte(u8 byte)
{
	int err;

	err = ds_send_control(COMM_BYTE_IO | COMM_IM | COMM_SPU, byte);
	if (err)
		return err;

	err = ds_wait_status();
	if (err)
		return err;

	// Block I/O does a write and a read, so consume the read data.
	err = ds_recv_data(DMAAddress(dataBuffer), 1);
	if (err < 0)
		return err;

	ds_start_pulse(PULLUP_PULSE_DURATION);

	if (cm->dataBuffer[0] != byte)
		return -ENODEV;
	return 0;
}

static int ds_write_block(dma_addr_t buf_dma, int len)
{
	int err;

	err = ds_send_data(buf_dma, len);
	if (err < 0)
		return err;

	ds_wait_status();

/// FIXME: make SPU optional
	err = ds_send_control(COMM_BLOCK_IO | COMM_IM | COMM_SPU, len);
	if (err)
		return err;

	ds_wait_status();

	// Block I/O does a write and a read, so consume the read data.
	err = ds_recv_data(buf_dma, len);
	if (err < 0)
		return err;

	ds_start_pulse(PULLUP_PULSE_DURATION);

	if (err != len)
		return -ENODEV;
	return 0;
}

static int ds_read_byte(u8 *byte)
{
	int err;

	err = ds_send_control(COMM_BYTE_IO | COMM_IM , 0xff);
	if (err)
		return err;

	ds_wait_status();

	err = ds_recv_data(DMAAddress(dataBuffer), 1);
	if (err < 0)
		return err;

	*byte = cm->dataBuffer[0];
	return 0;
}

/* Read len bytes into dataBuffer. */
static int ds_read_block(dma_addr_t buf_dma, int len)
{
	int err;

	if (len > 64*1024)
		return -E2BIG;

	// To read, we must write one's.
	memset(&cm->dataBuffer, 0xFF, len);
	err = ds_send_data(DMAAddress(dataBuffer), len);
	if (err < 0)
		return err;

	err = ds_send_control(COMM_BLOCK_IO | COMM_IM | COMM_SPU, len);
	if (err)
		return err;

	ds_wait_status();

	err = ds_recv_data(buf_dma, len);

	return err;
}

void
SearchNext(Message * msg, uint8_t searchCommand)
{
  int err;
  uint64_t startROM = ((uint64_t)msg->rcv_w2 << 32) | msg->rcv_w1;

  memcpy(cm->dataBuffer, &startROM, 8);
  err = ds_send_data(DMAAddress(dataBuffer), 8);
  assert(!err);	// FIXME

  ds_wait_status();
  err = ds_send_control(COMM_SEARCH_ACCESS | COMM_IM | COMM_SM
                        | COMM_F,// | COMM_RTS,
                        searchCommand | (1 << 8) );	// find one only
  assert(!err);	// FIXME

  err = waitStatus();
  assert(err > 0);	// FIXME
  struct ds_status * st = &cm->status;
  if ((err > 16) && (st->results[0] & RES_NRS)) {
    msg->snd_code = RC_capros_W1Bus_NoDevicePresent;
    return;
  }

  err = ds_recv_data(DMAAddress(dataBuffer), 8);
  assert(err == 8);	// FIXME

  memcpy(&startROM, cm->dataBuffer, 8);
  msg->snd_w1 = startROM;
  msg->snd_w2 = startROM >> 32;
}

#if 0

static int ds_match_access(struct ds_device *dev, u64 init)
{
	int err;
	struct ds_status st;

	err = ds_send_data((unsigned char *)&init, sizeof(init));
	if (err)
		return err;

	ds_wait_status();

	err = ds_send_control(COMM_MATCH_ACCESS | COMM_IM | COMM_RST, 0x0055);
	if (err)
		return err;

	ds_wait_status();

	return 0;
}

static int ds_set_path(struct ds_device *dev, u64 init)
{
	int err;
	struct ds_status st;
	u8 buf[9];

	memcpy(buf, &init, 8);
	buf[8] = BRANCH_MAIN;

	err = ds_send_data(buf, sizeof(buf));
	if (err)
		return err;

	ds_wait_status();

	err = ds_send_control(COMM_SET_PATH | COMM_IM | COMM_RST, 0);
	if (err)
		return err;

	ds_wait_status();

	return 0;
}

#endif  /*  0  */

/* The W1Bus capability is served by this separate thread,
because it may be called by a non-persistent process, so
we might need the page fault handler to return to it. 
The page fault handler may depend on USB, which may depend
on the thread serving the USBDevice capability.
This avoids deadlock. */
void *
w1bus_thread(void * arg)
{
  result_t result;
  int err;
  Message Msg;

  result = capros_Process_makeStartKey(KR_SELF, 0, KR_TEMP0);
  assert(result == RC_OK);
  result = capros_Node_getSlotExtended(KR_CONSTIT, KC_NOTIFY, KR_RETURN);
  assert(result == RC_OK);

  // send the W1Bus cap to the notifyee.
  Msg.snd_invKey = KR_RETURN;
  Msg.snd_code = RC_OK;
  Msg.snd_w1 = 0;
  Msg.snd_w2 = 0;
  Msg.snd_w3 = 0;
  Msg.snd_key0 = KR_TEMP0;
  Msg.snd_key1 = KR_VOID;
  Msg.snd_key2 = KR_VOID;
  Msg.snd_rsmkey = KR_VOID;
  Msg.snd_len = 0;
  // Can't use an ordinary RETURN because we may be invoking a start cap.
  SEND(&Msg);

  Msg.snd_invKey = KR_VOID;

  for (;;) {
    Msg.rcv_key0 = KR_VOID;
    Msg.rcv_key1 = KR_VOID;
    Msg.rcv_key2 = KR_VOID;
    Msg.rcv_rsmkey = KR_RETURN;
    Msg.rcv_data = &cm->dataBuffer;
    Msg.rcv_limit = sizeof(cm->dataBuffer);

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
      Msg.snd_w1 = IKT_capros_W1Bus;
      break;
  
    case OC_capros_W1Bus_busReset:
      err = ds_reset();
      if (err)
        Msg.snd_code = capros_Errno_ErrnoToException(err);
      break;
  
    case OC_capros_W1Bus_write0:
    {
      uint8_t byte;
      err = ds_touch_bit(0, &byte);
      if (err) {
        Msg.snd_code = capros_Errno_ErrnoToException(err);
        break;
      }
      break;
    }
  
    case OC_capros_W1Bus_write1Read:
    {
      uint8_t byte;
      err = ds_touch_bit(1, &byte);
      if (err) {
        Msg.snd_code = capros_Errno_ErrnoToException(err);
        break;
      }
      Msg.snd_w1 = byte;
      break;
    }
  
    case OC_capros_W1Bus_writeBit:
      err = ds_write_bit(Msg.rcv_w1);
      if (err) {
        Msg.snd_code = capros_Errno_ErrnoToException(err);
      }
      break;
  
    case OC_capros_W1Bus_readByte:
    {
      uint8_t byte;
      err = ds_read_byte(&byte);
      if (err) {
        Msg.snd_code = capros_Errno_ErrnoToException(err);
        break;
      }
      Msg.snd_w1 = byte;
      break;
    }
  
    case OC_capros_W1Bus_writeByte:
      err = ds_write_byte(Msg.rcv_w1);
      if (err) {
        Msg.snd_code = capros_Errno_ErrnoToException(err);
      }
      break;
  
    case 1:	// readBlock
    {
      unsigned long len = Msg.rcv_w1;
      if (len > capros_W1Bus_maxBlockSize) {
        Msg.snd_code = RC_capros_key_RequestError;
        break;
      }
      err = ds_read_block(DMAAddress(dataBuffer), len);
      if (err) {
        Msg.snd_code = capros_Errno_ErrnoToException(err);
        break;
      }
      Msg.snd_data = &cm->dataBuffer;
      Msg.snd_len = len;
      break;
    }
  
    case 2:	// writeBlock
    {
      unsigned long len = Msg.rcv_sent;
      if (len > capros_W1Bus_maxBlockSize) {
        Msg.snd_code = RC_capros_key_RequestError;
        break;
      }
      err = ds_write_block(DMAAddress(dataBuffer), len);
      if (err) {
        Msg.snd_code = capros_Errno_ErrnoToException(err);
        break;
      }
      break;
    }
  
    case OC_capros_W1Bus_searchNext:
      SearchNext(&Msg, 0xf0);
      break;
  
    case OC_capros_W1Bus_conditionalSearchNext:
      SearchNext(&Msg, 0xec);
      break;
  
    case OC_capros_W1Bus_waitForDisconnect:
      assert(!"implemented");	//// FIXME
      break;
  
    case OC_capros_W1Bus_resetDevice:
      err = ds_send_control_cmd(CTL_RESET_DEVICE, 0);
      if (err) {
        Msg.snd_code = capros_Errno_ErrnoToException(err);
        break;
      }
      break;
  
    case OC_capros_W1Bus_setSpeed:
      if (Msg.rcv_w1 > capros_W1Bus_W1Speed_overdrive) {
        Msg.snd_code = RC_capros_key_RequestError;
        break;
      }
      ModeCommand(MOD_1WIRE_SPEED, Msg.rcv_w1);
      break;
  
    case OC_capros_W1Bus_setSPUD:
      if (Msg.rcv_w1 > 254) {
        Msg.snd_code = RC_capros_key_RequestError;
        break;
      }
      ModeCommand(MOD_STRONG_PU_DURATION, Msg.rcv_w1);
      break;
  
    case OC_capros_W1Bus_setPDSR:
      if (Msg.rcv_w1 > capros_W1Bus_PDSR_PDSR055) {
        Msg.snd_code = RC_capros_key_RequestError;
        break;
      }
      ModeCommand(MOD_PULLDOWN_SLEWRATE, Msg.rcv_w1);
      break;
  
    case OC_capros_W1Bus_setW1LT:
      if (Msg.rcv_w1 > capros_W1Bus_W1LT_W1LT15) {
        Msg.snd_code = RC_capros_key_RequestError;
        break;
      }
      ModeCommand(MOD_WRITE1_LOWTIME, Msg.rcv_w1);
      break;
  
    case OC_capros_W1Bus_setDSO:
      if (Msg.rcv_w1 > capros_W1Bus_DSO_DSO10) {
        Msg.snd_code = RC_capros_key_RequestError;
        break;
      }
      ModeCommand(MOD_DSOW0_TREC, Msg.rcv_w1);
      break;
  
    }
  }
}
