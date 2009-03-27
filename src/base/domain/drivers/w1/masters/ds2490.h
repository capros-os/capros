/*
 * Copyright (C) 2008, 2009, Strawberry Development Group.
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
/* This material is based upon work supported by the US Defense Advanced
Research Projects Agency under Contract No. W31P4Q-07-C-0070.
Approved for public release, distribution unlimited. */

#include <linux/usb.h>
#include <idl/capros/W1Bus.h>
#include "../../usb/lib/usbdev.h"

// #define TIMING

unsigned long capros_Errno_ExceptionToErrno(unsigned long excep);
unsigned long capros_Errno_ErrnoToException(unsigned long errno);

#define KC_VOLSIZE KC_APP2(0)
#define KC_SYSTRACE KC_APP2(1)

// KR_APP2(0) is KR_USBINTF from lib/usbdev.h
#define KR_SysTrace KR_APP2(1)

/* COMMAND TYPE CODES */
#define CONTROL_CMD			0x00
#define COMM_CMD			0x01
#define MODE_CMD			0x02

/* CONTROL COMMAND CODES */
#define CTL_RESET_DEVICE		0x0000
#define CTL_START_EXE			0x0001
#define CTL_RESUME_EXE			0x0002
#define CTL_HALT_EXE_IDLE		0x0003
#define CTL_HALT_EXE_DONE		0x0004
#define CTL_FLUSH_COMM_CMDS		0x0007
#define CTL_FLUSH_RCV_BUFFER		0x0008
#define CTL_FLUSH_XMT_BUFFER		0x0009
#define CTL_GET_COMM_CMDS		0x000A

/* MODE COMMAND CODES */
#define MOD_PULSE_EN			0x0000
#define MOD_SPEED_CHANGE_EN		0x0001
#define MOD_1WIRE_SPEED			0x0002
#define MOD_STRONG_PU_DURATION		0x0003
#define MOD_PULLDOWN_SLEWRATE		0x0004
#define MOD_PROG_PULSE_DURATION		0x0005
#define MOD_WRITE1_LOWTIME		0x0006
#define MOD_DSOW0_TREC			0x0007

/* COMMUNICATION COMMAND CODES */
#define COMM_ERROR_ESCAPE		0x0601
#define COMM_SET_DURATION		0x0012
#define COMM_BIT_IO			0x0020
#define COMM_PULSE			0x0030
#define COMM_1_WIRE_RESET		0x0042
#define COMM_BYTE_IO			0x0052
#define COMM_MATCH_ACCESS		0x0064
#define COMM_BLOCK_IO			0x0074
#define COMM_READ_STRAIGHT		0x0080
#define COMM_DO_RELEASE			0x6092
#define COMM_SET_PATH			0x00A2
#define COMM_WRITE_SRAM_PAGE		0x00B2
#define COMM_WRITE_EPROM		0x00C4
#define COMM_READ_CRC_PROT_PAGE		0x00D4
#define COMM_READ_REDIRECT_PAGE_CRC	0x21E4
#define COMM_SEARCH_ACCESS		0x00F4

/* Communication command bits */
#define COMM_TYPE			0x0008
#define COMM_SE				0x0008
#define COMM_D				0x0008
#define COMM_Z				0x0008
#define COMM_CH				0x0008
#define COMM_SM				0x0008
#define COMM_R				0x0008
#define COMM_IM				0x0001

#define COMM_PS				0x4000
#define COMM_PST			0x4000
#define COMM_CIB			0x4000
#define COMM_RTS			0x4000
#define COMM_DT				0x2000
#define COMM_SPU			0x1000
#define COMM_F				0x0800
#define COMM_NTP			0x0400
#define COMM_ICP			0x0200
#define COMM_RST			0x0100

#define PULSE_PROG			0x01
#define PULSE_SPUE			0x02

#define BRANCH_MAIN			0xCC
#define BRANCH_AUX			0x33

/* Status flags */
#define ST_SPUA				0x01  /* Strong Pull-up is active */
#define ST_PRGA				0x02  /* 12V programming pulse is being generated */
#define ST_12VP				0x04  /* external 12V programming voltage is present */
#define ST_PMOD				0x08  /* DS2490 powered from USB and external sources */
#define ST_HALT				0x10  /* DS2490 is currently halted */
#define ST_IDLE				0x20  /* DS2490 is currently idle */
#define ST_EPOF				0x80

/* Result register flags */
#define RES_NRS	0x01
#define RES_SH  0x02
#define RES_APP 0x04
#define RES_VPP 0x08
#define RES_CMP 0x10
#define RES_CRC 0x20
#define RES_RDP 0x40
#define RES_EOS 0x80

#define SPEED_NORMAL			0x00
#define SPEED_FLEXIBLE			0x01
#define SPEED_OVERDRIVE			0x02

#define NUM_EP				4
#define EP_CONTROL			0
#define EP_STATUS			1
#define EP_DATA_OUT			2
#define EP_DATA_IN			3

#define CMD_FIFO_SIZE 16
#define EP2_FIFO_SIZE 128
#define EP3_FIFO_SIZE 128

extern unsigned int w1bus_threadNum;

struct ds_device
{
	struct usb_device	*udev;

	int			ep[NUM_EP];
};
extern struct ds_device theDSDev;

struct ds_status {
	u8	enable;
	u8	speed;
	u8	pullup_dur;
	u8	ppuls_dur;
	u8	pulldown_slew;
	u8	write1_time;
	u8	write0_time;
	u8	reserved0;
	u8	status;
	u8	command0;
	u8	command1;
	u8	command_buffer_status;
	u8	data_out_buffer_status;
	u8	data_in_buffer_status;
	u8	reserved1;
	u8	reserved2;
	uint8_t results[16];
};

/* All the structures that need to be in dma-coherent memory are
allocated in this structure.
Some could be overlaid, but we have to allocate at least a page anyway. */
struct coherentMemory {
  struct usb_ctrlrequest ctrlreq;
  struct ds_status status;
  unsigned char dataBuffer[capros_W1Bus_maxWriteSize];
} * cm;
dma_addr_t coherentMemory_dma;
#define DMAAddress(field) coherentMemory_dma + offsetof(struct coherentMemory, field)

void * w1bus_thread(void * arg);
