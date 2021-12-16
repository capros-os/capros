/*
 * Copyright (C) 1998, 1999, Jonathan S. Shapiro.
 * Copyright (C) 2005, 2007, Strawberry Development Group
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
#include <idl/capros/Sleep.h>
#include <idl/capros/SysTrace.h>
#include <domain/domdbg.h>
#include <ipl/capros/Constructor.h>
#include <memory.h>

#define KR_VOID     0
#define KR_SLEEP    9
#define KR_OSTREAM  10
#define KR_SYSTRACE 11
#define KR_HELLOCRE 12
#define KR_BANK     13
#define KR_SCHED    14
#define KR_SINK     15
#define KR_HELLO    16

#define ITERATIONS 100

/* It is intended that this should be a large space domain */
const uint32_t __rt_stack_pages = 0;
const uint32_t __rt_stack_pointer = 0x41000;

#define BUF_SZ 1

char buf[BUF_SZ];

uint32_t
create_hello(uint32_t krHelloCre, uint32_t krBank, uint32_t krSched,
	     uint32_t krHello)
{
  return capros_Constructor_request(krHelloCre,
                                    krBank, krSched, KR_VOID, krHello);
}

int
main()
{
  int i;
  uint32_t result;

  eros_SysTrace_clearKernelStats(KR_SYSTRACE);
  eros_SysTrace_startCounter(KR_SYSTRACE, eros_SysTrace_mode_cycles);

  for (i = 0; i < ITERATIONS; i++) {
    result = create_hello(KR_HELLOCRE, KR_BANK, KR_SCHED, KR_HELLO);
    
    if (result != RC_OK)
      kdprintf(KR_OSTREAM, "Hello creation failed\n");
  }
  
  eros_SysTrace_stopCounter(KR_SYSTRACE);

  
  kprintf(KR_OSTREAM, "forkexec -- %d iterations\n", ITERATIONS);

  return 0;
}

