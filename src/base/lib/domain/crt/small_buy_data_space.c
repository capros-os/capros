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
#include <idl/eros/Page.h>
#include <eros/KeyConst.h>
#include <eros/Invoke.h>
#include <domain/ProtoSpace.h>
#include <domain/SpaceBankKey.h>
#include <domain/ProcessCreatorKey.h>
#include <domain/ConstructorKey.h>
#include <domain/Runtime.h>

#define KR_MYSPACE KR_APP(0)
#define KR_OLDPAGE KR_APP(1)
#define KR_NEWPAGE KR_APP(2)

extern void etext();
extern void end();

extern uint32_t __rt_small_space;

void
__rt_buy_data_space()
{
  uint32_t result;
  uint32_t base = (uint32_t) &etext;
  uint32_t bound = (uint32_t) &end;
  
  /* Round the base down and the bound up to the nearest page
     boundary. */
  base -= base % EROS_PAGE_SIZE;
  bound += EROS_PAGE_SIZE - 1;
  bound -= bound % EROS_PAGE_SIZE;

  result = process_copy(KR_SELF, ProcAddrSpace, KR_MYSPACE);

  while (base < bound) {
    uint32_t slot = base / EROS_PAGE_SIZE;
    
    result = node_copy(KR_MYSPACE, slot, KR_OLDPAGE);

    result = spcbank_buy_data_pages(KR_BANK, 1, KR_NEWPAGE, KR_VOID, KR_VOID);
    
    result = eros_Page_clone(KR_NEWPAGE, KR_OLDPAGE);
    
    result = node_swap(KR_MYSPACE, slot, KR_NEWPAGE, KR_VOID);

    base += EROS_PAGE_SIZE;
  }
}
