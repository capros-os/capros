/*
 * Copyright (C) 2001 Jonathan S. Shapiro.
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

#include <eros/target.h>
#include <eros/ProcessKey.h>
#include <eros/ProcessState.h>
#include <eros/NodeKey.h>
#include <eros/KeyConst.h>
#include <eros/Invoke.h>
#include <domain/Runtime.h>
#include <domain/ProtoSpace.h>
#include <domain/SpaceBankKey.h>
#include <domain/ProcessCreatorKey.h>
#include <domain/ConstructorKey.h>

#include <idl/capros/key.h>

void __rt_setup_keeper() __attribute__((weak, alias("__rt_do_setup_keeper")));

#define KR_KEEPER KR_APP(0)

void
__rt_do_setup_keeper()
{
  extern uint32_t __rt_unkept;
  uint32_t result;
  uint32_t keyType;

  if (__rt_unkept)
    return;

  process_copy(KR_SELF, ProcKeeper, KR_KEEPER);

  result = capros_key_getType(KR_KEEPER, &keyType);

  if (result == RC_capros_key_Void)
    keyType = AKT_Void;

  if (keyType == AKT_ConstructorRequestor) {
    result = constructor_request(KR_KEEPER, KR_BANK, KR_SCHED,
				 KR_VOID, KR_KEEPER);
    process_swap(KR_SELF, ProcKeeper, KR_KEEPER, KR_VOID);
    return;
  }

  if (keyType == AKT_Void) {
  }
}
