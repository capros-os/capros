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
#include <kerninc/Process.h>
#include <kerninc/Invocation.h>
#include <kerninc/Activity.h>
#include <kerninc/KernStream.h>
#include <eros/Invoke.h>
#include <eros/StdKeyType.h>
#include <eros/ConsoleKey.h>

#include <idl/capros/key.h>


/* May Yield. */
void
ConsoleKey(Invocation* inv /*@ not null @*/)
{
  inv_GetReturnee(inv);

  switch(inv->entry.code) {
  case OC_Console_Put:
    {
      COMMIT_POINT();

      if (inv->entry.len > 1024) {
	inv->exit.code = RC_capros_key_RequestError;
	break;
      }

      if (inv->entry.len) {
        if (*(inv->entry.data) == 0)	// bug catcher
          dprintf(true, "OC_Console_Put with null in string, %#x.\n",
                  inv->entry.data);
	kstream_PutBuf(inv->entry.data, inv->entry.len);

#if 0
	if (inv->entry.data[inv->entry.len-1] != '\n')
	  printf("\n");
#endif
      }

      inv->exit.code = RC_OK;
      break;
    }
  case OC_Console_KDB:
    {
      inv->exit.code = RC_OK;
      Debugger();
      COMMIT_POINT();

      break;
    }
  case OC_capros_key_getType:
    {
      inv->exit.code = RC_OK;
      inv->exit.w1 = AKT_Console;
      COMMIT_POINT();

      break;
    }
  default:
    inv->exit.code = RC_capros_key_UnknownRequest;
    COMMIT_POINT();

    break;
  }

  ReturnMessage(inv);
}
