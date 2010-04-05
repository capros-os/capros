/*
 * Copyright (C) 2010, Strawberry Development Group.
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

/* DS2408Mult test.
*/

#include <stdint.h>
#include <string.h>
#include <eros/target.h>
#include <eros/Invoke.h>
#include <idl/capros/Sleep.h>
#include <idl/capros/W1Mult.h>
#include <idl/capros/DS2408.h>

#include <domain/Runtime.h>
#include <domain/domdbg.h>
#include <domain/assert.h>

#define KR_OSTREAM  KR_APP(0)
#define KR_SLEEP    KR_APP(1)
#define KR_DS2408M  KR_APP(2)

const uint32_t __rt_stack_pointer = 0x20000;

#define ckOK \
  if (result != RC_OK) { \
    kdprintf(KR_OSTREAM, "Line %d result is 0x%08x!\n", __LINE__, result); \
  }

int
main(void)
{
  result_t result;
  uint8_t output = 0;

  kprintf(KR_OSTREAM, "Starting.\n");

  // Give it a chance to get started:
  result = capros_Sleep_sleep(KR_SLEEP, 3000);	// sleep 3 seconds
  assert(result == RC_OK || result == RC_capros_key_Restart);

  kprintf(KR_OSTREAM, "Looping.\n");

  for (;;) {
    result = capros_Sleep_sleep(KR_SLEEP, 2000);	// sleep 2 seconds
    assert(result == RC_OK || result == RC_capros_key_Restart);

    result = capros_DS2408_setOutputs(KR_DS2408M, output++);
    ckOK
  }

  kprintf(KR_OSTREAM, "Done.\n");

  return 0;
}

