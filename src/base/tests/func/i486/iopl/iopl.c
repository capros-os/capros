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

#include <eros/target.h>
#include <eros/Invoke.h>
#include <eros/arch/i486/io.h>
#include <domain/domdbg.h>
#include <idl/capros/arch/i386/DevPrivsX86.h>

#define KR_OSTREAM  9
#define KR_DEVPRIVS 10

/* It is intended that this should be a small space process. */
const uint32_t __rt_stack_pages = 0;
const uint32_t __rt_stack_pointer = 0x21000;
const uint32_t __rt_unkept = 1; /* do not mess with keeper */

#define ckOK \
  if (result != RC_OK) { \
    kdprintf(KR_OSTREAM, "Line %d result is 0x%08x!\n", __LINE__, result); \
  }

int
main(void)
{
  result_t result;

  kprintf(KR_OSTREAM, "IOPL: About to call getCPUInfo\n");

  capros_arch_i386_DevPrivsX86_CPUInfo cpui;
  result = capros_arch_i386_DevPrivsX86_getCPUInfo(KR_DEVPRIVS, &cpui);
  ckOK
  kprintf(KR_OSTREAM, "family %u, vendorCode %u\n",
          cpui.family, cpui.vendorCode);

  kprintf(KR_OSTREAM, "IOPL: About to issue IO instruction\n");

  /* Writing anything to port 0x80 is a no-op, so this should be
   * safe. */
  outb(0x5, 0x80);

  kprintf(KR_OSTREAM, "IOPL: Success (no GP fault)\n");

  return 0;
}
