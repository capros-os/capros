/*
 * Copyright (C) 2006, Strawberry Development Group.
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
   Research Projects Agency under Contract No. W31P4Q-06-C-0040. */

#include <string.h>
#include <kerninc/kernel.h>
#include <kerninc/Key.h>
#include <kerninc/Process.h>
#include <kerninc/Invocation.h>
#include <kerninc/SysTimer.h>
#include <kerninc/KernStats.h>
#include <eros/Invoke.h>
#include <eros/StdKeyType.h>

#include <idl/eros/key.h>
#include <idl/eros/arch/arm/SysTrace.h>

void
SysTraceKey(Invocation* inv /*@ not null @*/)
{
  COMMIT_POINT();
  
  switch(inv->entry.code) {
  case OC_eros_key_getType:
    inv->exit.code = RC_OK;
    inv->exit.w1 = AKT_SysTrace;
    break;

  default:
    inv->exit.code = RC_eros_key_UnknownRequest;
    break;

  case OC_eros_arch_arm_SysTrace_CheckConsistency:
    {
      // Performance test of check.
      #include <kerninc/Check.h>
      check_Consistency("SysTrace");

      inv->exit.code = RC_OK;
      break;
    }
  case OC_eros_arch_arm_SysTrace_clearKernelStats:
    {
      bzero(&KernStats, sizeof(KernStats));
      inv->exit.code = RC_OK;
      break;
    }
  }
}
