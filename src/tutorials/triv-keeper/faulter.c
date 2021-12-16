/*
 * Copyright (C) 1998, 1999, Jonathan S. Shapiro.
 * Copyright (C) 2005, 2009, Strawberry Development Group
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

#include <stddef.h>
#include <eros/target.h>
#include <eros/Invoke.h>
#include <idl/capros/Node.h>
#include <idl/capros/Sleep.h>

#include <domain/Runtime.h>
#include <domain/domdbg.h>
#include "constituents.h"

/*
 * a simple program to simulate a syscall trap
 */

#define KR_OSTREAM     KR_APP(0)
#define KR_SLEEP       KR_APP(1)

/* /i/ must not be local. Dead code elimination is not our friend for
 * this example. */
int i = 5;

int
divby(int v)
{
  return i / v;
}

int
main ()
{
  capros_Node_getSlot(KR_CONSTIT, KC_OSTREAM, KR_OSTREAM);
  capros_Node_getSlot(KR_CONSTIT, KC_SLEEP, KR_SLEEP);
#if 0
  kprintf(KR_OSTREAM, "Faulter is initialized and will sleep for 4 seconds.");

  capros_Sleep_sleep(KR_SLEEP, 4000);	/* sleep 4 secs */
#endif
  
  kprintf(KR_OSTREAM, "Faulter faults...\n");

#if 0
  /* Machine-independent hack for causing a fault: touch an address
   * that is surely outside the legal range for a small process. Since
   * this process does not have a memory keeper, the fault will get
   * sent to the process keeper instead.
   *
   */
  *((uint32_t *) 0xf0000000) = 1;
#endif

  /*
   * In case we're not dead yet (courtesy Monty Python), make sure
   * (courtesy David Braun):
   */

  divby(0);

  kprintf(KR_OSTREAM, "I survived divide by zero (this is not a good thing)...\n");

  return 0;			/* not bloody likely */
}
