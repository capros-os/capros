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

#define dbg_prog 0x1

/* Following should be an OR of some of the above */
#define dbg_flags   ( 0u | dbg_prog )////

#define DEBUG(x) if (dbg_##x & dbg_flags)

unsigned long MsgRcvBuf[capros_W1Bus_maxProgramSize/4];
unsigned long MsgSndBuf[capros_W1Bus_maxResultsSize/4];

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

/* Return value:
If < 0, a negative error number.
If >=0, the number of bytes received into *status. */
static int ds_recv_status(void)
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

	if (count >= 16) {
#if 0
		ds_dump_status(st->enable, "enable flag");
		ds_dump_status(st->speed,  "1-wire speed");
		ds_dump_status(st->pullup_dur, "strong pullup duration");
		//ds_dump_status(st->ppuls_dur, "programming pulse duration");
		ds_dump_status(st->pulldown_slew, "pulldown slew rate control");
		ds_dump_status(st->write1_time, "write-1 low time");
		ds_dump_status(st->write0_time,
                               "data sample offset/write-0 recovery time");
		//ds_dump_status(st->reserved0, "reserved (test register)");
#endif
		ds_dump_status(st->status, "device status flags");
		ds_dump_status(st->command0, "communication command byte 1");
		ds_dump_status(st->command1, "communication command byte 2");
		ds_dump_status(st->command_buffer_status,
                               "communication command buffer status");
		ds_dump_status(st->data_out_buffer_status,
                               "1-wire data output buffer status");
		ds_dump_status(st->data_in_buffer_status,
                               "1-wire data input buffer status");
		//ds_dump_status(st->reserved1, "reserved");
		//ds_dump_status(st->reserved2, "reserved");
	}
}

static unsigned long ds_recv_data(dma_addr_t buf_dma, int len)
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
	count = ds_recv_status();
	assert(count >= 16);	// FIXME

	dump_status(count);

	struct ds_status * st = &cm->status;
	if (st->status & ST_EPOF) {
		printk(KERN_INFO "Resetting device after ST_EPOF.\n");
		int err = ds_send_control_cmd(CTL_RESET_DEVICE, 0);
		assert(!err);	// FIXME
	}
	assert(false);	// FIXME
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

/* Returns:
If > 0: number of status bytes read.
If == 0: failed to become idle after 100 tries.
If < 0: a negative error code. */
static int waitStatus(void)
{
	int err, count = 0;

	struct ds_status * st = &cm->status;
	do {
		err = ds_recv_status();
		if (err < 0)
			return err;	// return negative error
#if 1////
		dump_status(err);
#endif
	} while(!(st->status & ST_IDLE)
	        && ++count < 100);

#if 1////
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

uint16_t needReset;	// else COMM_RST

/* unsentData has the number of bytes of data in dataBuffer
to be written.
They will eventually be written in one of three ways:
- When followed by a strongPullup, by a COMM_BLOCK_IO.
- When followed by a readBytes, by a COMM_READ_STRAIGHT that also reads.
- Otherwise, by a COMM_READ_STRAIGHT that reads nothing.
*/
unsigned int unsentData;

// Values for Command.type:
enum {
  cmdType_none,
  cmdType_resetNormal,
  cmdType_resetAny,
  cmdType_setPath,
  cmdType_search,
  cmdType_readCRC
};

// # of bytes in dataBuffer to be sent to EP2
unsigned int ep2Size;
// # of bytes to be received from EP3
unsigned int ep3Size;

unsigned char * resultNext;
unsigned int ep3Gotten;

#define cmdBufSize capros_W1Bus_maxProgramSize // more than enough
static struct Command {
  uint8_t type;
  uint8_t request;
  uint16_t value;
  uint16_t index;
  uint8_t ep3Size;	// expected ep3 data after this command
} cmdBuf[cmdBufSize], *cmdNext;

static inline bool
cmdAtBeginning(void)
{ return cmdNext == &cmdBuf[0]; }

static inline void
NextCmd(void)
{
  cmdNext->ep3Size = ep3Size;
  cmdNext++;
}

/* The strong pullup duration code last sent to the master: */
#define SPUDCode_unknown 255
static uint8_t lastSPUDCode = SPUDCode_unknown;

/* The strong pullup duration code programmed so far: */
static uint8_t lastSPUDCodePgm;

void
ReadStraightCmd(unsigned int bytesToRead)
{
  // If there are any unsent bytes, they can be sent with this command.
  ep3Size += bytesToRead;
  cmdNext->type = cmdType_none;
  cmdNext->request = COMM_CMD;
  uint16_t val = COMM_READ_STRAIGHT | COMM_IM;
  if (needReset) {
    // The reset bit is in a different position in this command.
    val |= 0x2;
    needReset = 0;
  }
  if (unsentData) {
    val |= unsentData << 8;
    unsentData = 0;
  }
  cmdNext->value = val;
  cmdNext->index = bytesToRead;
  NextCmd();
}

void
HandleUnsentData(uint16_t spu)
{
  // The command space has already been reserved.
  if (unsentData == 1 && ! needReset) {
    cmdNext->type = cmdType_none;
    cmdNext->request = COMM_CMD;
    cmdNext->value = COMM_BYTE_IO | COMM_IM | COMM_ICP | spu;
    cmdNext->index = cm->dataBuffer[--ep2Size];	// data byte
    NextCmd();
  } else {		// can't use COMM_BYTE_IO
    /* COMM_READ_STRAIGHT is preferable to COMM_BLOCK_IO, because
    it doesn't fill EP3. */
    if (spu) {		// can't use COMM_READ_STRAIGHT
      cmdNext->type = cmdType_none;
      cmdNext->request = COMM_CMD;
      cmdNext->value = COMM_BLOCK_IO | COMM_IM | spu | needReset;
      needReset = 0;
      cmdNext->index = unsentData;
      NextCmd();
      ep3Size += unsentData;
    } else {
      ReadStraightCmd(0);
    }
  }
  unsentData = 0;
}

// spu is 0 or COMM_SPU.
static void
HandleAnyUnsentData(void)
{
  if (unsentData)
    HandleUnsentData(0);
}

unsigned int
GetEP3Data(unsigned int expected)
{
  if (expected > 0) {	// there should be data in EP3 to get
    unsigned long actualLen = ds_recv_data(DMAAddress(dataBuffer), expected);
    DEBUG(prog) printk("GetEP3 expected %d got %d", expected, actualLen);

    memcpy(resultNext, cm->dataBuffer, actualLen);
    resultNext += actualLen;
    ep3Gotten += actualLen;
    return expected - actualLen;	// return size of shortfall
  }
  return 0;
}

void
RunProgram(Message * msg, uint32_t pgmLen)
{
  int err;

  unsigned char * startPgm = (void *)&MsgRcvBuf;
  unsigned char * endPgm = startPgm + pgmLen;
  unsigned char * pgm = startPgm;	// where we are in the program

  unsigned char * resultStart = (void *)&MsgSndBuf;
  resultNext = resultStart;

  /* We may have to divide the program into segments,
  if we can't execute it all at once. */

  while (pgm < endPgm) {	// process segments
    unsigned char * segStart = pgm;	// start of the current segment
    needReset = 0;
    ep2Size = 0;
    ep3Size = 0;
    cmdNext = cmdBuf;
    unsentData = 0;
    lastSPUDCodePgm = lastSPUDCode; // code at the beginning of the segment

    while (pgm < endPgm) {	// process steps
      uint16_t option;
#define needPgm(n) if (pgm + (n) > endPgm) goto programError;
#define needEP2(n) if (ep2Size + (n) > EP2_FIFO_SIZE) goto endSegment;
#define needEP3(n) if (ep3Size + (n) > EP3_FIFO_SIZE) goto endSegment;
#define needCmd(n) if (cmdNext + (n) - cmdBuf > cmdBufSize) goto endSegment;
      unsigned char stepCode = *pgm++;
      DEBUG(prog) printk("stepCode %d", stepCode);
      switch (stepCode) {

      case capros_W1Bus_stepCode_resetSimple:
        HandleAnyUnsentData();
        if (needReset) goto sequenceError;
        /* Remember the reset. We will combine it with the next commmand. */
        needReset = COMM_RST;
        break;

      case capros_W1Bus_stepCode_resetAny:
        HandleAnyUnsentData();
        if (needReset) goto sequenceError;
        // This result byte doesn't really go into EP3, but this checks
        // the limit on the results size.
        needEP3(1)
        needCmd(1)
        ep3Size += 1;
        cmdNext->type = cmdType_resetAny;
        goto resetCommon;

      case capros_W1Bus_stepCode_resetNormal:
        HandleAnyUnsentData();
        if (needReset) goto sequenceError;
        /* COMM_RST apparently doesn't check for presence or alarm or short,
        so we must issue an explicit command. */
        needCmd(1)
        cmdNext->type = cmdType_resetNormal;
      resetCommon:
        cmdNext->request = COMM_CMD;
        cmdNext->value = COMM_1_WIRE_RESET | COMM_IM;
        cmdNext->index = 0;
        NextCmd();
        break;

      case capros_W1Bus_stepCode_setPathMain:
      case capros_W1Bus_stepCode_setPathAux:
        HandleAnyUnsentData();
        needPgm(8)
        needEP2(9)
        needEP3(1)
        needCmd(1)
        ep3Size += 1;
        memcpy(&cm->dataBuffer[ep2Size], pgm, 8);
        ep2Size += 8;
        pgm += 8;
        cm->dataBuffer[ep2Size++] = stepCode;	// Smart-On command
        cmdNext->type = cmdType_setPath;
        cmdNext->request = COMM_CMD;
        cmdNext->value = COMM_SET_PATH | COMM_IM | needReset;
        needReset = 0;
        /* We set only one coupler at a time.
        Documentation is wrong: index is the number of couplers * 9,
        not the number of couplers. */
        cmdNext->index = 9;
        NextCmd();
        break;

      case capros_W1Bus_stepCode_skipROM:
        needEP2(1)
        /* The bytes might have to be sent with COMM_BLOCK_IO,
        which also returns the data in EP3, so reserve the space. */
        needEP3(unsentData + 1)
        needCmd(2)
        // Put the command in the buffer.
        cm->dataBuffer[ep2Size++] = 0xcc;	// skip ROM command
        unsentData += 1;
        break;

      case capros_W1Bus_stepCode_matchROM:
        HandleAnyUnsentData();
        needPgm(8)
        needEP2(8)
        needCmd(1)
        memcpy(&cm->dataBuffer[ep2Size], pgm, 8);
        ep2Size += 8;
        pgm += 8;
        cmdNext->type = cmdType_none;
        cmdNext->request = COMM_CMD;
        cmdNext->value = COMM_MATCH_ACCESS | COMM_IM | needReset;
        needReset = 0;
        cmdNext->index = stepCode;	// Match ROM command
	// one coupler at a time
        NextCmd();
        break;

      case capros_W1Bus_stepCode_searchROM:
      case capros_W1Bus_stepCode_alarmSearchROM:
        HandleAnyUnsentData();
        needPgm(8)
        needEP2(8)
        needEP3(16)
        needCmd(1)
        memcpy(&cm->dataBuffer[ep2Size], pgm, 8);
        ep2Size += 8;
        pgm += 8;
        ep3Size += 16;
        cmdNext->type = cmdType_search;
        cmdNext->request = COMM_CMD;
        cmdNext->value = COMM_SEARCH_ACCESS | COMM_IM | COMM_SM
                                  | COMM_RTS | needReset;
        needReset = 0;
        cmdNext->index = stepCode	// Search ROM command
                         + (1 << 8);	// only one at a time
        NextCmd();
        break;

      case capros_W1Bus_stepCode_write1Read:
        needEP3(1)
        option = COMM_D;
        goto writeCommon;

      case capros_W1Bus_stepCode_write0:
        option = COMM_ICP;	// don't return the data
      writeCommon:
        HandleAnyUnsentData();
        if (needReset) goto sequenceError;
        /* We need one command for the bit I/O, and, if this is followed
        by a strongPullup that requires changing the duration,
        another command for the strong pullup duration. */
        needCmd(2)
        if (option == COMM_D)
          ep3Size += 1;
        cmdNext->type = cmdType_none;
        cmdNext->request = COMM_CMD;
        cmdNext->value = COMM_BIT_IO | COMM_IM | option;
        cmdNext->index = 0;
        NextCmd();
        break;

      case capros_W1Bus_stepCode_writeBytes:
      {
        needPgm(1)
        unsigned char nBytes = *pgm;
        // Note, maxWriteSize <= EP2_FIFO_SIZE.
        if (nBytes > capros_W1Bus_maxWriteSize)
          goto programError; // can never do this
        needPgm(1+nBytes)
        needEP2(nBytes)
        /* The bytes might have to be sent with COMM_BLOCK_IO,
        which also returns the data in EP3, so reserve the space. */
        needEP3(unsentData + nBytes)
        /* We need one command for the byte I/O, and, if this is followed
        by a strongPullup that requires changing the duration,
        another command for the strong pullup duration. */
        needCmd(2)
        pgm++;
        // Put the data in the buffer.
        memcpy(&cm->dataBuffer[ep2Size], pgm, nBytes);
        ep2Size += nBytes;
        pgm += nBytes;
        unsentData += nBytes;
        break;
      }

      case capros_W1Bus_stepCode_readBytes:
      {
        needPgm(1)
        unsigned char nBytes = *pgm;
        // Note, maxReadSize <= EP3_FIFO_SIZE.
        if (nBytes > capros_W1Bus_maxReadSize)
          goto programError; // can never do this
        needEP3(nBytes)
        needCmd(1)
        pgm++;
        ReadStraightCmd(nBytes);
        break;
      }

      case capros_W1Bus_stepCode_readCRC8:
        option = 0;
        goto readCRC;

      case capros_W1Bus_stepCode_readCRC16:
        option = COMM_DT;
      readCRC:
        HandleAnyUnsentData();
        if (needReset) goto sequenceError;
        needPgm(5)
        needEP2(3)
        needCmd(1)
      {
        unsigned char logPageSize = *pgm;
        if (logPageSize == 0 || logPageSize >= 7) goto programError;
        unsigned char numPages = *(pgm+1);
        unsigned long nBytes = (1UL << logPageSize) * numPages;
        if (nBytes > EP3_FIFO_SIZE) goto programError; // can never do this
        needEP3(nBytes)
        pgm += 2;
        memcpy(&cm->dataBuffer[ep2Size], pgm, 3);
        ep2Size += 3;
        ep3Size += nBytes;
        pgm += 3;
        cmdNext->type = cmdType_readCRC;
        cmdNext->request = COMM_CMD;
        cmdNext->value = COMM_READ_CRC_PROT_PAGE | COMM_IM | option;
        cmdNext->index = logPageSize | (numPages << 8);
        NextCmd();
        break;
      }

      case capros_W1Bus_stepCode_readUntil1:
        HandleAnyUnsentData();
        if (needReset) goto sequenceError;
        if (! cmdAtBeginning()) goto endSegment; // do previous stuff first
        needPgm(1)
        ////...
        break;

      case capros_W1Bus_stepCode_strongPullup5:
      {
        needPgm(1)
        uint8_t code = *pgm;
        if (code == 0 || code == SPUDCode_unknown)
          goto programError;
        if (! cmdAtBeginning()
            && ((cmdNext - 1)->value & 0xb9f6) == COMM_BIT_IO) {
          // COMM_BIT_IO can have SPU added.
          if (needReset) goto sequenceError;
          (cmdNext - 1)->value |= COMM_SPU;
        } else if (unsentData) {
          HandleUnsentData(COMM_SPU);
        }
        else goto sequenceError;
        if (code != lastSPUDCodePgm) {	// need to change duration
          struct Command * cmdPrev = cmdNext - 1;
          // Move the last command down.
          *cmdNext = *cmdPrev;
          // Insert the duration command.
          cmdNext->type = cmdType_none;
          cmdPrev->request = MODE_CMD;
          cmdPrev->value = MOD_STRONG_PU_DURATION;
          cmdPrev->index = code;
          NextCmd();
          lastSPUDCodePgm = code;
        }
        pgm++;
        break;
      }

      }
    }
    // We have preprocessed the entire program.
    goto execute;

endSegment:
    pgm--;	// undo increment to get stepCode
execute:
    HandleAnyUnsentData();
    if (needReset) goto sequenceError;

    // We have now preprocessed a segment. Execute it.
    struct Command * cmdEnd = cmdNext;
    // All EP2 data can be loaded at once.
    // That is the advantage of grouping commands into a segment.
    DEBUG(prog) printk("Sending %d bytes to EP2", ep2Size);
    if (ep2Size) {
      err = ds_send_data(DMAAddress(dataBuffer), ep2Size);
      assert(!err);	// FIXME
if (ep2Size == 17) {////
//// is the following necessary?
      err = waitStatus();
      assert(err == 16);	// FIXME handle or report
}
    }

    ep3Gotten = 0;
    bool needStatus = false;
    for (cmdNext = cmdBuf; cmdNext < cmdEnd; cmdNext++) {
      DEBUG(prog) printk("executing %#x %#x %#x type %d\n",
                         cmdNext->request, cmdNext->value,
                         cmdNext->index, cmdNext->type);

      err = ds_send_usb_control(cmdNext->request, cmdNext->value,
                                cmdNext->index);
      assert(!err);	// FIXME
      needStatus = false;	// default
      switch (cmdNext->type) {
      case cmdType_resetNormal:
      case cmdType_resetAny:
      {
        uint8_t app = 0;
        // Check the status from reset
        err = waitStatus();
        assert(err >= 16);	// FIXME handle or report
        if (err > 16) {		// got a result
          uint8_t result = cm->status.results[0];
          if (result & RES_SH)
            goto terminateShorted;
          if (result & RES_NRS)
            goto terminateNoDevice;
          if (result & RES_APP) {
            app = 4;
          }
        } else {	// err == 16, no result
        }
        if (cmdNext->type == cmdType_resetAny) {
          // Report whether app or not.
          int expected = cmdNext->ep3Size - ep3Gotten;
          assert(expected >= 0);
          err = GetEP3Data(expected);
          assert(!err);	// FIXME - too little EP3 data
          *resultNext++ = app;
          ep3Gotten++;
        } else {	// resetNormal
          if (app) goto terminateAPP;
        }
        break;
      }

      case cmdType_setPath:
      {
        err = waitStatus();
        assert(err >= 16);	// FIXME handle or report
        int expected = cmdNext->ep3Size - ep3Gotten;
        assert(expected >= 1);
        int err2 = GetEP3Data(expected);
        assert(!err2);	// FIXME - too little EP3 data
        err2 = *--resultNext;	// last character gotten
        if (err > 16) {		// got a result
          uint8_t result = cm->status.results[0];
          assert(err2 == 0);	// should have activated 0 coupler
          if (result & RES_SH)
            goto terminateShorted;
          if (result & RES_NRS)
            goto terminateNoDevice;
          if (result & RES_CMP) {
            goto terminateBusError;
          }
        }
        /* Documentation is wrong: data is the number of activated couplers * 9,
        not the number of activated couplers. */
        if (err2 != 9) {	// should have activated 1 coupler = 9 bytes
          printk("path err2=%d", err2);
          assert(false);
        }
        break;
      }

      case cmdType_search:
        err = waitStatus();
        assert(err >= 16);	// FIXME handle or report
        if (err > 16) {		// got a result
          uint8_t result = cm->status.results[0];
          DEBUG(prog) printk("search result=%#x", result);
          if (result & RES_EOS)
            assert(false);  // can't happen because we search for 1 at a time
          if (result & RES_NRS)	// this also seems to happen with bus errors
            goto terminateNoDevice;
        }
        DEBUG(prog) printk("search ep3Gotten %d cmdNext->ep3Size %d",
                           ep3Gotten, cmdNext->ep3Size);
        int expected = cmdNext->ep3Size - ep3Gotten;
        assert(expected >= 16);
        err = GetEP3Data(expected);
        DEBUG(prog) printk("search shortfall %d", err);
        if (err == 8) {	// we did not get the discrepancy information
          memset(resultNext, 0, 8);	// supply zeros
          resultNext += 8;
          ep3Gotten += 8;
        } else {
          assert(!err);	// FIXME - too little EP3 data
        }
        break;

      case cmdType_readCRC:
        err = waitStatus();
        assert(err >= 16);	// FIXME handle or report
        if (err > 16) {		// got a result
          uint8_t result = cm->status.results[0];
          if (result & RES_CRC)
            goto terminateCRCError;
        }
        break;

      case cmdType_none:
      default:
        needStatus = true;
        break;
      }
    }
    // Finished issuing the commands for this segment.
    if (needStatus) {
      DEBUG(prog) printk("End of segment");
      err = waitStatus();
      assert(err == 16);	// FIXME handle or report
    }
    // Get any final data for this segment.
    int expected = (cmdNext - 1)->ep3Size - ep3Gotten;
    err = GetEP3Data(expected);
    assert(!err);

    DEBUG(prog) {	// show status at end of segment
      err = ds_recv_status();
      assert(err >= 16);
      dump_status(err);
      struct ds_status * st = &cm->status;
      if (st->data_out_buffer_status) {
        printk("Clearing excess data");
        err = ds_send_usb_control(CONTROL_CMD, CTL_HALT_EXE_IDLE, 0);
        assert(!err);	// FIXME
        err = ds_send_usb_control(CONTROL_CMD, CTL_FLUSH_XMT_BUFFER, 0);
        assert(!err);	// FIXME
        err = ds_send_usb_control(CONTROL_CMD, CTL_RESUME_EXE, 0);
        assert(!err);	// FIXME
      }
    }
    continue;	// process any other segments

sequenceError: ;
    unsigned long ret = capros_W1Bus_StatusCode_SequenceError;
    goto preprocessError;

programError:
    ret = capros_W1Bus_StatusCode_ProgramError;
preprocessError:
    // None of the current segment was executed.
    msg->snd_w1 = ret;
    // -1 below is because pgm was incremented to get the stepCode.
    msg->snd_w2 = segStart - (pgm - 1);
    return;
  }
  // Processed all segments.
  unsigned long ret = capros_W1Bus_StatusCode_OK;
  goto returnLength;

terminateShorted:
  ret = capros_W1Bus_StatusCode_BusShorted;
  goto returnLength;

terminateNoDevice:
  ret = capros_W1Bus_StatusCode_NoDevicePresent;
  goto returnLength;

terminateAPP:
  ret = capros_W1Bus_StatusCode_AlarmingPresencePulse;
  goto returnLength;

terminateBusError:
  ret = capros_W1Bus_StatusCode_BusError;
  goto returnLength;

terminateCRCError:
  ret = capros_W1Bus_StatusCode_CRCError;
  goto returnLength;

returnLength:
  msg->snd_w1 = ret;
  msg->snd_w2 = -1;	// FIXME what to report here?
  msg->snd_data = resultStart;
  msg->snd_len = resultNext - resultStart;
  return;
}

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

    switch (Msg.rcv_code) {
    default:
      Msg.snd_code = RC_capros_key_UnknownRequest;
      break;

    case OC_capros_key_getType:
      Msg.snd_w1 = IKT_capros_W1Bus;
      break;
  
    case 3:	// runProgram
      if (Msg.rcv_sent > Msg.rcv_limit) {	// he sent too much
        Msg.snd_code = RC_capros_W1Bus_ProgramTooLong;
        break;
      }
      RunProgram(&Msg, Msg.rcv_sent);
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
      lastSPUDCode = 512 / 16;	// power-up default
      break;
  
    case OC_capros_W1Bus_setSpeed:
      if (Msg.rcv_w1 > capros_W1Bus_W1Speed_overdrive) {
        Msg.snd_code = RC_capros_key_RequestError;
        break;
      }
      ModeCommand(MOD_1WIRE_SPEED, Msg.rcv_w1);
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
