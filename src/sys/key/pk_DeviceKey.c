/*
 * Copyright (C) 1998, 1999, Jonathan S. Shapiro.
 * Copyright (C) 2007, 2008, Strawberry Development Group.
 *
 * This file is part of the CapROS Operating System,
 * and is derived from the EROS Operating System.
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

#include <kerninc/kernel.h>
#include <kerninc/Key.h>
#include <kerninc/Activity.h>
#include <kerninc/Invocation.h>
#include <eros/Device.h>
#include <eros/Invoke.h>
#include <eros/StdKeyType.h>
/*#include <kerninc/BlockDev.h>*/

#include <idl/capros/key.h>

/* FIX -- device key needs a kosher index field, and should use
 * subtype for the class.
 */


void
DeviceKey(Invocation* inv /*@ not null @*/)
{
  inv_GetReturnee(inv);

  /* Note that DEV_GET_CLASS retrieves the CLASS from the device. */
  
  uint32_t devclass = DEV_GET_CLASS(inv->key->u.dk.devClass);

  switch(inv->entry.code) {
  case OC_capros_key_getType:
    COMMIT_POINT();
      
    inv->exit.code = AKT_Device;
    inv->exit.w1 = devclass;
    inv->exit.w2 = DEV_GET_SUBCLASS(inv->key->u.dk.devClass);
    break;

  default:
    COMMIT_POINT();

    inv->exit.code = RC_capros_key_UnknownRequest;
    break;
  }

  ReturnMessage(inv);
}
