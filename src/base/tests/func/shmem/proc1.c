/*
 * Copyright (C) 2007, Strawberry Development Group
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

#include <eros/target.h>
#include <domain/domdbg.h>
#include <idl/eros/arch/arm/SysTrace.h>

#define KR_VOID 0
#define KR_OSTREAM 10
#define KR_SYSTRACE 11

#define ITERATIONS 100

#define ADDR1 0x40000
#define ADDR2 0x60000

/* It is intended that this should be a small space domain */
const uint32_t __rt_stack_pages = 0;
const uint32_t __rt_stack_pointer = 0x21000;
const uint32_t __rt_unkept = 1;	/* do not mess with keeper */

uint32_t
IRead(uint32_t addr) {
  return *((uint32_t volatile *)addr);
}

void
IReadCk(uint32_t addr, uint32_t value) {
  uint32_t x;
  x = IRead(addr);
  if (x != value)
    kprintf(KR_OSTREAM, "At 0x%08x read 0x%08x expected 0x%08x\n",
            addr, x, value);
}

void
IWrite(uint32_t addr, uint32_t value) {
  *((uint32_t volatile *)addr) = value;
}

int
main()
{
  int i;
  unsigned offset = 0;

  kprintf(KR_OSTREAM, "Beginning %d iterations\n", ITERATIONS);
  for (i = 0; i < ITERATIONS; i++) {
    IRead(ADDR1+offset);
    IWrite(ADDR1+offset, i);
    IReadCk(ADDR1+offset, i);
    IWrite(ADDR2+offset, -i);
    IReadCk(ADDR1+offset, -i);
    IReadCk(ADDR2+offset, -i);

    offset += 4;
  }

  eros_arch_arm_SysTrace_CheckConsistency(KR_SYSTRACE);

  kprintf(KR_OSTREAM, "Done\n");

  return 0;
}
