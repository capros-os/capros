/*
 * Copyright (C) 2007, Strawberry Development Group.
 *
 * This file is part of the CapROS Operating System runtime library.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, 59 Temple Place - Suite 330 Boston, MA 02111-1307, USA.
 */
/* This material is based upon work supported by the US Defense Advanced
Research Projects Agency under Contract No. W31P4Q-07-C-0070.
Approved for public release, distribution unlimited. */

/* Emulation for Linux procedures clk_get etc.
*/

#include <string.h>
#include <domain/Runtime.h>
#include <linuxk/linux-emul.h>
#include <linuxk/lsync.h>
#include <linux/clk.h>
#include <linux/err.h>
#include <idl/capros/Node.h>
#include <idl/capros/DevClock.h>

struct clk *
clk_get(struct device * dev, const char * id)
{
  int i;
  result_t retval;

  /* The key in KR_LINUX_EMUL slot LE_CLOCKS defines the clock resources
  available. If it is void, there are no resources. 
  If it has a key to a node, the node contains pairs of keys.
  The first key of a pair is a number key containing
  the name of a clock. The second key is a key to the clock object. */

  retval = capros_Node_getSlot(KR_LINUX_EMUL, LE_CLOCKS, KR_TEMP0);
  if (retval)
    return ERR_PTR(-ENOENT);

  for (i = 0; i < EROS_NODE_SIZE; i += 2) {
    retval = capros_Node_getSlot(KR_TEMP0, i, KR_TEMP1);
    if (retval)	// if LE_CLOCKS has a void key, there are no resources:
      return ERR_PTR(-ENOENT);

    capros_Number_value num;
    retval = capros_Number_getValue(KR_TEMP1, &num);
    if (retval)
      return ERR_PTR(-ENOENT);

    if (! strncmp((char *)&num, id, sizeof(num))) {	// match
      return (struct clk *)(i+1);	// return index of key to use
    }
  }
 
  return ERR_PTR(-ENOENT);
}

// Get a capability to the clock object into KR_TEMP0.
static inline int get_cap(struct clk * clk)
{
  result_t retval;

  retval = capros_Node_getSlot(KR_LINUX_EMUL, LE_CLOCKS, KR_TEMP0);
  if (retval)
    return -ENOENT;

  retval = capros_Node_getSlot(KR_TEMP0, (int)clk, KR_TEMP0);
  if (retval)
    return -ENOENT;

  return 0;
}

int 
clk_enable(struct clk * clk)
{
  result_t retval;
  int err;

  err = get_cap(clk);
  if (err) return err;

  retval = capros_DevClock_enable(KR_TEMP0);
  if (retval) return -ENOENT;
  return 0;
}

void 
clk_disable(struct clk * clk)
{
  if (! get_cap(clk)) {
    capros_DevClock_disable(KR_TEMP0);
  }
}

unsigned long 
clk_get_rate(struct clk * clk)
{
  unsigned long rate = 0;

  if (! get_cap(clk)) {
    capros_DevClock_getRate(KR_TEMP0, &rate);
  }
  return rate;
}

void 
clk_put(struct clk * clk)
{
  if (! get_cap(clk)) {
    capros_key_destroy(KR_TEMP0);
  }
}

