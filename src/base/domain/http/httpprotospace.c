/*
 * Copyright (C) 2009, Strawberry Development Group.
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

/* This table is used by an interpreter to construct the address space
for an instance of HTTP built by a constructor. */

#include <InterpreterTable.h>
#include <idl/capros/HTTP.h>

struct InterpreterStep ConstructionTable[] = {
  MsgAlloc1(KR_BANK, GPT, KR_TEMP1, passErrorThrough, 2),
  MsgSetL2v(KR_TEMP1, 22),
  MsgNodeGetSlotExtended(KR_CONSTIT, capros_HTTP_KC_ProgramVCS, KR_TEMP0),
  MsgNewVCSK(KR_TEMP0, KR_BANK, KR_SCHED, KR_TEMP0, passErrorThrough, 1),
  MsgGPTSetSlot(KR_TEMP1, 0, KR_TEMP0),
  MsgNodeGetSlotExtended(KR_CONSTIT, capros_HTTP_KC_ProgramPC, KR_TEMP0),
  MsgGetNumber(KR_TEMP0),
  MsgNewSpace(KR_TEMP1)
};
struct InterpreterStep DestructionTable[] = {
/* [0] */ MsgDestroy(KR_TEMP0),	// destroy VCSK
/* [1] */ MsgFree1(KR_BANK, KR_TEMP1),
/* [2] */ MsgDestroyProcess(KR_CREATOR, KR_BANK, KR_RETURN)
};
