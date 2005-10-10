/*
 * Copyright (C) 2001, Jonathan S. Shapiro.
 *
 * This file is part of the EROS Operating System.
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

#include <kerninc/kernel.h>
#include <kerninc/Key.h>
#include <kerninc/Activity.h>
#include <kerninc/Invocation.h>
#include <kerninc/IRQ.h>
#include <kerninc/PhysMem.h>
#include <kerninc/ObjectSource.h>
#include <kerninc/ObjectCache.h>
#include <eros/target.h>
#include <eros/Invoke.h>
#include <eros/StdKeyType.h>
#if 0
#include <kerninc/MsgLog.h>
#include <kerninc/Activity.h>
#include <eros/StdKeyType.h>
#include <eros/DiscrimKey.h>
#include <kerninc/Persist.h>
#endif

#include <idl/eros/key.h>
#include <idl/eros/DevPrivs.h>

#define dbg_wakeup	0x1u
#define dbg_alloc	0x2u
#define dbg_sleep	0x4u
#define dbg_error	0x8u

/* Following should be an OR of some of the above */
#define dbg_flags   ( 0u )

#define DEBUG(x) if (dbg_##x & dbg_flags)

struct UserIrq {
  bool       isPending;
  bool       isAlloc;
  StallQueue sleepers;
} UserIrqEntries[NUM_HW_INTERRUPT];

void 
UserIrqInit()
{
  int i = 0;

  for (i = 0; i < NUM_HW_INTERRUPT; i++)
    sq_Init(&UserIrqEntries[i].sleepers);
}

static void
DoUsermodeInterrupt(savearea_t *ia)
{
  uint32_t irq = IRQ_FROM_EXCEPTION(ia->ExceptNo);

  struct UserIrq *uirq = &UserIrqEntries[irq];
  
  assert(irq_GetHandler(irq) == DoUsermodeInterrupt);

  DEBUG(wakeup)
    printf("Waking sleepers for IRQ %d\n", irq);

  uirq->isPending = true;
  sq_WakeAll(&uirq->sleepers, false);
}

extern void *malloc(size_t sz);

void
DevicePrivsKey(Invocation* inv /*@ not null @*/)
{
  unsigned irq = inv->entry.w1;
  struct UserIrq *uirq = &UserIrqEntries[irq];

  /* NOTE: we haven't yet range checked irq, because this isn't used
   * in all paths, so the resulting reference may well be out of
   * range. Must range check before using. */

  switch(inv->entry.code) {
  case OC_eros_DevPrivs_allocIRQ:
    {
      DEBUG(alloc)
	printf("DevPrivs: Allocating IRQ %d\n", irq);

      COMMIT_POINT();

      if (irq >= NUM_HW_INTERRUPT) {
	inv->exit.code = RC_eros_key_RequestError;
	break;
      }
      
      if (uirq->isAlloc ||
	  irq_GetHandler(irq) != irq_UnboundInterrupt) {
	inv->exit.code = RC_eros_DevPrivs_AllocFail;
	break;
      }
      
      uirq->isPending = false;

      irq_SetHandler(irq, DoUsermodeInterrupt);
      irq_Enable(irq);

#if 0
      static bool WireShared(IntAction*);
      static void Unwire(IntAction*);
#endif
    }
  
    inv->exit.code = RC_OK;
    break;
    
  case OC_eros_DevPrivs_releaseIRQ:
    {
      COMMIT_POINT();

      if (irq >= NUM_HW_INTERRUPT) {
	inv->exit.code = RC_eros_key_RequestError;
	break;
      }
      
      if (!uirq->isAlloc) {
	inv->exit.code = RC_eros_DevPrivs_AllocFail;
	break;
      }
      
      irq_UnsetHandler(irq);

      uirq->isPending = false;
  
      inv->exit.code = RC_OK;
      break;
    }
    
  case OC_eros_DevPrivs_enableIRQ:
    {
      COMMIT_POINT();

      if (irq >= NUM_HW_INTERRUPT) {
	inv->exit.code = RC_eros_key_RequestError;
	break;
      }
      
      if (!uirq->isAlloc) {
	inv->exit.code = RC_eros_DevPrivs_AllocFail;
	break;
      }
      
      irq_Enable(irq);
  
      inv->exit.code = RC_OK;
      break;
    }
    
  case OC_eros_DevPrivs_disableIRQ:
    {
      COMMIT_POINT();

      if (irq >= NUM_HW_INTERRUPT) {
	inv->exit.code = RC_eros_key_RequestError;
	break;
      }
      
      if (!uirq->isAlloc) {
	inv->exit.code = RC_eros_DevPrivs_AllocFail;
	break;
      }
      
      irq_Disable(irq);
  
      inv->exit.code = RC_OK;
      break;
    }
    
  case OC_eros_DevPrivs_waitIRQ:
    {
      irq_Enable(irq);

      irq_DISABLE();
      
      if (irq < NUM_HW_INTERRUPT && !uirq->isPending) {
	DEBUG(sleep)
	  printf("DevPrivs: Sleeping for IRQ %d\n", irq);
        act_SleepOn(act_Current(), &uirq->sleepers);


	irq_ENABLE();
	act_Yield(act_Current());
      }
      irq_ENABLE();

      COMMIT_POINT();

      if (irq >= NUM_HW_INTERRUPT) {
	DEBUG(error)
	  printf("IRQ %d is invalid, bogon\n", irq);
	inv->exit.code = RC_eros_key_RequestError;
	break;
      }

      assert(uirq->isPending);
      DEBUG(sleep)
	printf("IRQ %d was pending already\n", irq);

      irq_DISABLE();
      uirq->isPending = false;
      irq_ENABLE();
	
      /* Else return RC_OK per below */
      inv->exit.code = RC_OK;
      break;
    }
    
  case OC_eros_DevPrivs_publishMem:
    {
      PmemInfo *pmi = 0;
      kpa_t base = inv->entry.w1;
      kpa_t bound = inv->entry.w2;
      bool readOnly = inv->entry.w3;

      if ((base % EROS_PAGE_SIZE) || (bound % EROS_PAGE_SIZE)) {
	COMMIT_POINT();
	inv->exit.code = RC_eros_key_RequestError;
	break;
      }

      if (base >= bound) {
	COMMIT_POINT();
	inv->exit.code = RC_eros_key_RequestError;
	break;
      }

      COMMIT_POINT();

      pmi = physMem_AddRegion(base, bound, MI_DEVICEMEM, readOnly);

      if (pmi) {
	ObjectSource *source;

	objC_AddDevicePages(pmi);

        source = (ObjectSource *)malloc(sizeof(ObjectSource));
        /* code for initializing PhysPageSource */
        source->name = "physpage";
        source->start = OID_RESERVED_PHYSRANGE + ((pmi->base / EROS_PAGE_SIZE) * EROS_OBJECTS_PER_FRAME);
        source->end = OID_RESERVED_PHYSRANGE + ((pmi->bound / EROS_PAGE_SIZE) * EROS_OBJECTS_PER_FRAME);
        source->pmi = pmi;
        source->objS_Detach = PhysPageSource_Detach;
        source->objS_GetObject = PhysPageSource_GetObject;
        source->objS_IsRemovable = ObjectSource_IsRemovable;
        source->objS_WriteBack = PhysPageSource_WriteBack;
        source->objS_Invalidate = PhysPageSource_Invalidate;
        source->objS_FindFirstSubrange = ObjectSource_FindFirstSubrange;  
	objC_AddSource(source);

	inv->exit.code = RC_OK;
      }
      else {
	COMMIT_POINT();
	inv->exit.code = RC_eros_key_NoAccess;
      }

      break;
    }

  case OC_eros_key_getType:
    inv->exit.code = RC_OK;
    inv->exit.w1 = AKT_DevicePrivs;
    break;

  default:
    inv->exit.code = RC_eros_key_UnknownRequest;
    break;
  }

  return;
}
