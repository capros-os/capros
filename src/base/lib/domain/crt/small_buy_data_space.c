/*
 * Copyright (C) 2001 Jonathan S. Shapiro.
 * Copyright (C) 2007, 2008, 2009, Strawberry Development Group.
 *
 * This file is part of the CapROS Operating System runtime library,
 * and is derived from the EROS Operating System runtime library.
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
#include <idl/capros/Page.h>
#include <idl/capros/GPT.h>
#include <idl/capros/SpaceBank.h>
#include <idl/capros/Process.h>
#include <eros/Invoke.h>
#include <domain/ProtoSpace.h>
#include <domain/Runtime.h>

#define KR_MYSPACE KR_TEMP0
#define KR_OLDPAGE KR_TEMP1
#define KR_NEWPAGE KR_TEMP2

extern char __data_start;
extern void end();

extern uint32_t __rt_small_space;

void
__rt_buy_data_space()
{
  uint32_t base = (uint32_t) &__data_start;
  uint32_t bound = (uint32_t) &end;
  
  /* Round the base down and the bound up to the nearest page
     boundary. */
  base &= - EROS_PAGE_SIZE;
  bound += EROS_PAGE_SIZE - 1;
  bound &= - EROS_PAGE_SIZE;

  result = capros_Process_getAddrSpace(KR_SELF, KR_MYSPACE);

  while (base < bound) {
    uint32_t slot = base / EROS_PAGE_SIZE;
    
    capros_GPT_getSlot(KR_MYSPACE, slot, KR_OLDPAGE);

    capros_SpaceBank_alloc1(KR_BANK, capros_Range_otPage, KR_NEWPAGE);
    
    capros_Page_clone(KR_NEWPAGE, KR_OLDPAGE);
    
    capros_GPT_setSlot(KR_MYSPACE, slot, KR_NEWPAGE);

    base += EROS_PAGE_SIZE;
  }
}
