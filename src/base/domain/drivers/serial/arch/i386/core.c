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

#include <linuxk/linux-emul.h>
#include <linuxk/lsync.h>
#include <linux/serial_core.h>
#include <linux/serial.h>
#include <eros/Invoke.h>	// get RC_OK
#include <domain/assert.h>
#include <idl/capros/Number.h>

#include "../../8250.h"
#include "../../serialPort.h"

struct old_serial_port old_serial_port[1];

extern int __init serial8250_init(void);

int capros_serial_initialization(void)
{
  result_t result;
  unsigned long w0, w1, w2;

  // Get parameters passed in KR_CONFIG.
  result = capros_Number_get(KR_CONFIG, &w0, &w1, &w2);
  assert(result == RC_OK);

  old_serial_port[0].baud_base = w0;
  old_serial_port[0].port = w1 & 0xffff;
  old_serial_port[0].irq = w1 >> 16;
  old_serial_port[0].flags = w2;
  old_serial_port[0].io_type = UPIO_PORT;

  return serial8250_init();
}
