/*
 * Copyright (C) 2009, Strawberry Development Group.
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

/* This table is used by an interpreter to construct the address space
for a logfile object built by the constructor. */

#include "logfile.h"
#include <InterpreterTable.h>
#include <idl/capros/Page.h>

struct InterpreterStep ConstructionTable[] = {
  MsgAlloc3(KR_BANK, Page, Page, GPT, KR_TEMP1, KR_TEMP2, KR_TEMP3,
    passErrorThrough, 6),
  MsgSetL2v(KR_TEMP3, 12),
  MsgNodeGetSlotExtended(KR_CONSTIT, KC_DATASEG, KR_TEMP0),
  MsgClonePage(KR_TEMP2, KR_TEMP0),
  MsgGPTSetSlot(KR_TEMP3, 0, KR_TEMP2),	// .data page
  MsgGPTSetSlot(KR_TEMP3, 1, KR_TEMP1),	// .bss page
  MsgAlloc2(KR_BANK, GPT, GPT, KR_TEMP2, KR_TEMP1,
    passErrorThrough, 3),
  MsgSetL2v(KR_TEMP2, 17),
  MsgSetL2v(KR_TEMP1, 22),
  MsgNodeGetSlotExtended(KR_CONSTIT, KC_TEXT, KR_TEMP0),
  MsgGPTSetSlot(KR_TEMP2, 0, KR_TEMP0),
  MsgGPTSetSlot(KR_TEMP2, 1, KR_TEMP3),
  MsgGPTSetSlot(KR_TEMP1, 0, KR_TEMP2),

  MsgNodeGetSlotExtended(KR_CONSTIT, KC_STARTADDR, KR_TEMP3),
  MsgGetNumber(KR_TEMP3),
  MsgNewSpace(KR_TEMP1)
};

/* For destruction, call:
   InterpreterDestroy(telospaceCap, KR_TEMP1, finalResult);
   which puts the address space root into KR_TEMP1.
 */
struct InterpreterStep DestructionTable[] = {
/* [0] */ MsgGPTGetSlot(KR_TEMP1, 0, KR_TEMP2),
/* [1] */ MsgGPTGetSlot(KR_TEMP2, 1, KR_TEMP3),
/* [2] */ MsgFree2(KR_BANK, KR_TEMP2, KR_TEMP1),
/* [3] */ MsgGPTGetSlot(KR_TEMP3, 0, KR_TEMP2),
/* [4] */ MsgGPTGetSlot(KR_TEMP3, 1, KR_TEMP1),
/* [5] */ MsgFree3(KR_BANK, KR_TEMP3, KR_TEMP2, KR_TEMP1),
/* [6] */ MsgDestroyProcess(KR_CREATOR, KR_BANK, KR_RETURN)
};
