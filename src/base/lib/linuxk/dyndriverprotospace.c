/*
 * Copyright (C) 2008, Strawberry Development Group.
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
for a driver built by the constructor. */

#include <linuxk/lsync.h>
#include <InterpreterTable.h>
#include <domain/ProtoSpace.h>

struct InterpreterStep ConstructionTable[] = {
  MsgAlloc3(KR_BANK, Page, GPT, GPT, KR_TEMP1, KR_TEMP2, KR_TEMP3,
            passErrorThrough, 2),
  MsgSetL2v(KR_TEMP3, 22),
  MsgSetL2v(KR_TEMP2, 17),
  MsgMakeGuarded(KR_TEMP1, (1UL << LK_LGSTACK_AREA) - EROS_PAGE_SIZE, KR_TEMP1),
  MsgGPTSetSlot(KR_TEMP2, 0, KR_TEMP1),
  MsgGPTSetSlot(KR_TEMP3, LK_STACK_BASE / 0x400000, KR_TEMP2),
  MsgNodeGetSlotExtended(KR_CONSTIT, KC_TEXT, KR_TEMP0),
  MsgGPTSetSlot(KR_TEMP3, 0, KR_TEMP0),
  MsgNodeGetSlotExtended(KR_CONSTIT, KC_DATAVCSK, KR_TEMP0),
  MsgNewVCSK(KR_TEMP0, KR_BANK, KR_SCHED, KR_TEMP0, passErrorThrough, 1),
  MsgGPTSetSlot(KR_TEMP3, LK_DATA_BASE / 0x400000, KR_TEMP0),
  MsgNodeGetSlotExtended(KR_CONSTIT, KC_STARTADDR, KR_TEMP0),
  MsgGetNumber(KR_TEMP0),
  MsgNewSpace(KR_TEMP3)
};
struct InterpreterStep DestructionTable[] = {
/* [0] */ MsgDestroy(KR_TEMP0),	// destroy VCSK
/* [1] */ MsgFree3(KR_BANK, KR_TEMP1, KR_TEMP2, KR_TEMP3),
/* [2] */ MsgDestroyProcess(KR_CREATOR, KR_BANK, KR_RETURN)
};
