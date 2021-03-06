/*
 * Copyright (C) 2007, Strawberry Development Group.
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

/* Sample small program: the obligatory ``hello world'' demo. */

#include <stddef.h>
#include <eros/target.h>
#include <domain/domdbg.h>
#include <domain/Runtime.h>
#include <idl/capros/Node.h>

#include "constituents.h"

#define KR_OSTREAM  KR_APP(0)

#ifdef EROS_TARGET_arm
// Because large spaces aren't implemented yet on arm:
const uint32_t __rt_stack_pointer = 0x02000000;
#endif

int
main(void)
{
  capros_Node_getSlot(KR_CONSTIT, KC_OSTREAM, KR_OSTREAM);

  kprintf(KR_OSTREAM, "hello, world\n");

  return 0;
}
