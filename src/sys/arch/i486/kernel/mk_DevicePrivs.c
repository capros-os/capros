/*
 * Copyright (C) 2001, Jonathan S. Shapiro.
 * Copyright (C) 2005, 2006, 2007, 2008, Strawberry Development Group.
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

#include <kerninc/kernel.h>
#include <kerninc/Key.h>
#include <kerninc/Activity.h>
#include <kerninc/Invocation.h>
#include <kerninc/PhysMem.h>
#include <kerninc/ObjectSource.h>
#include <kerninc/ObjectCache.h>
#include <kerninc/IRQ.h>
#include <eros/Invoke.h>
#include <eros/StdKeyType.h>

#include <idl/capros/key.h>
#include <eros/machine/DevPrivs.h>
#include <key/DevicePrivs.h>
#include "IRQ386.h"

#define dbg_alloc	0x2u
#define dbg_sleep	0x4u
#define dbg_error	0x8u

/* Following should be an OR of some of the above */
#define dbg_flags   ( 0u )

#define DEBUG(x) if (dbg_##x & dbg_flags)

/* May Yield. */
void
DevicePrivsKey(Invocation* inv /*@ not null @*/)
{
  inv_GetReturnee(inv);

  unsigned irq = inv->entry.w1;
  struct UserIrq *uirq = &UserIrqEntries[irq];

  /* NOTE: we haven't yet range checked irq, because this isn't used
   * in all paths, so the resulting reference may well be out of
   * range. Must range check before using. */

  switch(inv->entry.code) {
  case OC_capros_DevPrivs_allocIRQ:
    {
      DEBUG(alloc)
	printf("DevPrivs: Allocating IRQ %d\n", irq);

      COMMIT_POINT();

      if (irq >= NUM_HW_INTERRUPT) {
	inv->exit.code = RC_capros_key_RequestError;
	break;
      }
      
      if (uirq->isAlloc ||
	  irq_GetHandler(irq) != irq_UnboundInterrupt) {
	inv->exit.code = RC_capros_DevPrivs_AllocFail;
	break;
      }
      
      uirq->isAlloc = true;
      uirq->isPending = false;

      irq_SetHandler(irq, DoUsermodeInterrupt);
    }
  
    inv->exit.code = RC_OK;
    break;
    
  case OC_capros_DevPrivs_releaseIRQ:
    {
      COMMIT_POINT();

      if (irq >= NUM_HW_INTERRUPT) {
	inv->exit.code = RC_capros_key_RequestError;
	break;
      }
      
      if (!uirq->isAlloc) {
	inv->exit.code = RC_capros_DevPrivs_AllocFail;
	break;
      }
      
      irq_UnsetHandler(irq);
      uirq->isAlloc = false;
  
      inv->exit.code = RC_OK;
      break;
    }
    
  case OC_capros_DevPrivs_enableIRQ:
    {
      COMMIT_POINT();

      if (irq >= NUM_HW_INTERRUPT) {
	inv->exit.code = RC_capros_key_RequestError;
	break;
      }
      
      if (!uirq->isAlloc) {
	inv->exit.code = RC_capros_DevPrivs_AllocFail;
	break;
      }
      
      irq_Enable(irq);
  
      inv->exit.code = RC_OK;
      break;
    }
    
  case OC_capros_DevPrivs_disableIRQ:
    {
      COMMIT_POINT();

      if (irq >= NUM_HW_INTERRUPT) {
	inv->exit.code = RC_capros_key_RequestError;
	break;
      }
      
      if (!uirq->isAlloc) {
	inv->exit.code = RC_capros_DevPrivs_AllocFail;
	break;
      }
      
      irq_Disable(irq);
  
      inv->exit.code = RC_OK;
      break;
    }
    
  case OC_capros_DevPrivs_waitIRQ:
    {
      if (irq >= NUM_HW_INTERRUPT) {
        COMMIT_POINT();
	inv->exit.code = RC_capros_key_RequestError;
	break;
      }
      
      if (!uirq->isAlloc) {
        COMMIT_POINT();
	inv->exit.code = RC_capros_DevPrivs_AllocFail;
	break;
      }

      irqFlags_t flags = local_irq_save();
      
      if (!uirq->isPending) {
        // Ensure the interrupt is enabled.
        irq_Enable(irq);
  
	DEBUG(sleep)
	  printf("DevPrivs: Sleeping for IRQ %d\n", irq);
        act_SleepOn(&uirq->sleepers);


	local_irq_restore(flags);
	act_Yield();
      }
      local_irq_restore(flags);

      COMMIT_POINT();

      assert(uirq->isPending);
      DEBUG(sleep)
	printf("IRQ %d was pending already\n", irq);

      flags = local_irq_save();
      uirq->isPending = false;
      local_irq_restore(flags);
	
      inv->exit.code = RC_OK;
      break;
    }

  case OC_capros_DevPrivs_allocateDMAPages:
    physMem_AllocateDMAPages(inv);
    break;

  case OC_capros_DevPrivs_deallocateDMAPages:
    physMem_DeallocateDMAPages(inv);
    break;

  case OC_capros_DevPrivs_allocateIORQ:
    devPrivs_allocateIORQ(inv);
    break;

  case OC_capros_DevPrivs_declarePFHProcess:
    devPrivs_DeclarePFHProcess(inv);
    break;
    
  case OC_capros_DevPrivs_publishMem:
    {
      PmemInfo *pmi = 0;
      kpa_t base = inv->entry.w1;
      kpa_t bound = inv->entry.w2;
      bool readOnly = inv->entry.w3;

      COMMIT_POINT();

      if ((base % EROS_PAGE_SIZE) || (bound % EROS_PAGE_SIZE)) {
	inv->exit.code = RC_capros_key_RequestError;
	break;
      }

      if (base >= bound) {
	inv->exit.code = RC_capros_key_RequestError;
	break;
      }

      pmi = physMem_AddRegion(base, bound, MI_DEVICEMEM, readOnly);

      if (pmi) {
	objC_AddDevicePages(pmi);
        PhysPageSource_Init(pmi);

	inv->exit.code = RC_OK;
      }
      else {
	inv->exit.code = RC_capros_key_NoAccess;
      }

      break;
    }

  case OC_capros_key_getType:
    COMMIT_POINT();
    inv->exit.code = RC_OK;
    inv->exit.w1 = IKT_capros_DevPrivs;
    break;

  default:
    COMMIT_POINT();
    inv->exit.code = RC_capros_key_UnknownRequest;
    break;
  }

  ReturnMessage(inv);
}
