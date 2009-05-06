#ifndef _SERIALPORT_H_
#define _SERIALPORT_H_
/*
 * Copyright (C) 2007, Strawberry Development Group.
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
/* This material is based upon work supported by the US Defense Advanced
Research Projects Agency under Contract No. W31P4Q-07-C-0070.
Approved for public release, distribution unlimited. */

/* Task to wait until the serial port transmission is finished.
*/

#include <linuxk/linux-emul.h>
#include <linuxk/lsync.h>
#include <linux/wait.h>
#include <linux/serial_core.h>

// Key registers for the main thread:
// KR_PortNum initially has a number cap with the port number.
#define KR_PortNum KR_APP2(0)
#define KR_CONFIG     KR_APP2(1)	// architecture-dependent config param
#define KR_XMITWAITER KR_APP2(2)

bool isTransmitterEmpty(struct uart_port * port);
void * transmitterEmptyTask(void * arg);
void WaitForTransmitterEmpty(wait_queue_t * wait);
int CreateTransmitterEmptyTask(void);
void DestroyTransmitterEmptyTask(void);
void TransmitterEmptySepuku(void);

extern wait_queue_head_t sent_wait_queue_head;

extern struct uart_state theUartState;

#endif // _SERIALPORT_H_
