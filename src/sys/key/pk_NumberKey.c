/*
 * Copyright (C) 1998, 1999, 2001, Jonathan S. Shapiro.
 *
 * This file is part of the EROS Operating System.
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

#include <kerninc/kernel.h>
#include <kerninc/Key.h>
#include <kerninc/Activity.h>
#include <kerninc/Invocation.h>
#include <eros/Invoke.h>
#include <eros/StdKeyType.h>

#include <idl/capros/Number.h>

void
NumberKey(Invocation* inv /*@ not null @*/)
{
  COMMIT_POINT();

  inv->exit.code = RC_OK;

  switch(inv->entry.code) {
  case OC_capros_Number_get:
    {
      inv->exit.w1 = inv->key->u.nk.value[0];
      inv->exit.w2 = inv->key->u.nk.value[1];
      inv->exit.w3 = inv->key->u.nk.value[2];
      return;
    }
  case OC_capros_key_getType:
    inv->exit.code = RC_OK;
    inv->exit.w1 = AKT_Number;
    return;
  default:
    inv->exit.code = RC_capros_key_UnknownRequest;
    return;
  }
}
