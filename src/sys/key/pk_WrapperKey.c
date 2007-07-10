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
#include <kerninc/Process.h>
#include <kerninc/Invocation.h>
#include <kerninc/Node.h>
#include <kerninc/Activity.h>
#include <eros/Invoke.h>
#include <eros/StdKeyType.h>
#include <eros/NodeKey.h>

#include <idl/capros/key.h>

void
WrapperKey(Invocation* inv /*@ not null @*/)
{
  /* A properly formed wrapper key has no operations, because it
   * forwards all of its behavior to some other key. Therefore, the
   * protocol is pretty minimal:
   */
  switch (inv->entry.code) {
  case OC_capros_key_getType:
    {
      COMMIT_POINT();

      inv->exit.code = RC_OK;
      inv->exit.w1 = AKT_Wrapper;
      return;
    }

  default:
    COMMIT_POINT();

    inv->exit.code = RC_capros_key_UnknownRequest;
    return;
  }
}
