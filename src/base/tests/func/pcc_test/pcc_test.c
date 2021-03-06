/*
 * Copyright (C) 1998, 1999, 2001, Jonathan S. Shapiro.
 * Copyright (C) 2007, Strawberry Development Group.
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


#include <eros/target.h>
#include <eros/Invoke.h>
#include <idl/capros/Node.h>
#include <idl/capros/PCC.h>
#include <idl/capros/ProcTool.h>
#include <domain/domdbg.h>

#define KR_VOID 0
#define KR_CONSTIT 1
#define KR_OURDOM 2
#define KR_SPCBANK 4
#define KR_SCHED 5
#define KR_DOMTOOL 6
#define KR_KEYBITS 7
#define KR_DCC_BRAND 8
#define KR_OSTREAM 9
#define KR_DCC 10
#define KR_DOM 13

#define KC_OSTREAM    0
#define KC_DOMTOOL    1
#define KC_KEYBITS    2
#define KC_DCC_BRAND  3
#define KC_DCC        4
  
const uint32_t __rt_stack_pages = 1;
const uint32_t __rt_stack_pointer = 0x20000;

int
main()
{
  capros_Node_getSlot(KR_CONSTIT, KC_OSTREAM, KR_OSTREAM);
  capros_Node_getSlot(KR_CONSTIT, KC_DOMTOOL, KR_DOMTOOL);
  capros_Node_getSlot(KR_CONSTIT, KC_KEYBITS, KR_KEYBITS);
  capros_Node_getSlot(KR_CONSTIT, KC_DCC_BRAND, KR_DCC_BRAND);
  capros_Node_getSlot(KR_CONSTIT, KC_DCC, KR_DCC);

  kdprintf(KR_OSTREAM, "About to invoke pcc\n");

  {
    uint32_t result;
    result = capros_PCC_createProcessCreator(KR_DCC, KR_SPCBANK, KR_SCHED,
               KR_DOM);
    kdprintf(KR_OSTREAM, "Result is 0x%08x\n", result);
  }

  /* print the key: */
  {
    ShowKey(KR_OSTREAM, KR_KEYBITS, KR_DOM);
  }

  /* Convert the start key to a domain key: */
  {
    uint32_t isGood;
    uint32_t result;
    
    result = capros_ProcTool_canOpener(KR_DOMTOOL, KR_DOM, KR_DCC_BRAND, KR_DOM,
			  &isGood, 0);
    if (result != RC_OK || isGood == 0)
      kdprintf(KR_OSTREAM, "Canopener failed. Result is 0x%08x, %d\n",
	       result, isGood);
  }

  {
    ShowKey(KR_OSTREAM, KR_KEYBITS, KR_DOM);
  }

  return 0;
}
