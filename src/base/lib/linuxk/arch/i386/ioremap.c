/*
 * Copyright (C) 2007, 2008, Strawberry Development Group.
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

#include <eros/Invoke.h>
#include <domain/assert.h>

#include <idl/capros/Void.h>
#include <idl/capros/Node.h>
#include <idl/capros/GPT.h>
#include <idl/capros/SpaceBank.h>
#include <idl/capros/Process.h>

#include <linuxk/linux-emul.h>
#include <linuxk/lsync.h>
#include <asm/io.h>
#include <linux/mutex.h>
#include <domain/CMTEMaps.h>

void __iomem *
__ioremap(unsigned long offset, unsigned long size, unsigned long flags)
{
  kdprintf(KR_OSTREAM, "__ioremap called, not implemented!'\n");
  return NULL;
}

void iounmap(volatile void __iomem *addr)
{
  kdprintf(KR_OSTREAM, "iounmap called, not implemented!'\n");
}

/* serial/8250.c procedure wait_for_xmitr can loop over 1 second
with irq disabled! */
void touch_nmi_watchdog(void)
{
  kdprintf(KR_OSTREAM, "touch_nmi_watchdog called, not implemented!'\n");
}

unsigned long probe_irq_on(void)
{
  kdprintf(KR_OSTREAM, "probe_irq_on called, not implemented!'\n");
  return 0;
}

int probe_irq_off(unsigned long val)
{
  kdprintf(KR_OSTREAM, "probe_irq_off called, not implemented!'\n");
  return 0;
}
