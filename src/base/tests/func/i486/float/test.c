/*
 * Copyright (C) 2010, Strawberry Development Group.
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

#include <eros/target.h>
#include <eros/Invoke.h>

#include <domain/Runtime.h>

#include <domain/domdbg.h>

#define KR_OSTREAM  KR_APP(0)

const uint32_t __rt_stack_pointer = 0x20000;
const uint32_t __rt_unkept = 1;

void getFloat(const void * buf, int num, double f);

int
main(void)
{
  kprintf(KR_OSTREAM, "Test starting.\n");

  getFloat(NULL, 0, 2.7);

  kdprintf(KR_OSTREAM, "Step 2.\n");

  double f = 1.2;

  f += 1;

  kprintf(KR_OSTREAM, "Done %d.\n", (int)f);

  return 0;
}
