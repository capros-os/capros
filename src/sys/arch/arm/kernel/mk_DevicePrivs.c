/*
 * Copyright (C) 2001, Jonathan S. Shapiro.
 * Copyright (C) 2005, 2006, Strawberry Development Group.
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
#include <eros/Invoke.h>
#include <eros/StdKeyType.h>
#include "Interrupt.h"

#include <idl/eros/key.h>
#include <idl/eros/DevPrivs.h>

#define dbg_alloc	0x2u
#define dbg_sleep	0x4u
#define dbg_error	0x8u

/* Following should be an OR of some of the above */
#define dbg_flags   ( 0u )

#define DEBUG(x) if (dbg_##x & dbg_flags)

extern void *malloc(size_t sz);

void
DevicePrivsKey(Invocation* inv /*@ not null @*/)
{
  unsigned source = inv->entry.w1;
  VICIntSource * vis = &VICIntSources[source];

  /* NOTE: we haven't yet range checked source, because this isn't used
   * in all paths, so the resulting reference may well be out of
   * range. Must range check before using. */

  switch(inv->entry.code) {
  case OC_eros_DevPrivs_allocIRQ:
    {
      DEBUG(alloc)
	printf("DevPrivs: Allocating IRQ %d\n", source);

      COMMIT_POINT();

      if (source >= NUM_INTERRUPT_SOURCES) {
	inv->exit.code = RC_eros_key_RequestError;
	break;
      }
      
      if (vis_IsAlloc(vis)) {
	inv->exit.code = RC_eros_DevPrivs_AllocFail;
	break;
      }
      
      int32_t prio = inv->entry.w2;
      if (prio < 0	// FIQ not allowed
          || prio > 16 ) {
	inv->exit.code = RC_eros_key_RequestError;
	break;
      }
      
      vis->isPending = false;
      InterruptSourceSetup(source, prio, DoUsermodeInterrupt);

      InterruptSourceEnable(source);	// Do we want to do this now?
    }
  
    inv->exit.code = RC_OK;
    break;
    
  case OC_eros_DevPrivs_releaseIRQ:
    {
      COMMIT_POINT();

      if (source >= NUM_INTERRUPT_SOURCES) {
	inv->exit.code = RC_eros_key_RequestError;
	break;
      }
      
      if (!vis_IsAlloc(vis)) {
	inv->exit.code = RC_eros_DevPrivs_AllocFail;
	break;
      }
      
      InterruptSourceUnset(source);
  
      inv->exit.code = RC_OK;
      break;
    }
    
  case OC_eros_DevPrivs_enableIRQ:
    {
      COMMIT_POINT();

      if (source >= NUM_INTERRUPT_SOURCES) {
	inv->exit.code = RC_eros_key_RequestError;
	break;
      }
      
      if (!vis_IsAlloc(vis)) {
	inv->exit.code = RC_eros_DevPrivs_AllocFail;
	break;
      }
      
      InterruptSourceEnable(source);
  
      inv->exit.code = RC_OK;
      break;
    }
    
  case OC_eros_DevPrivs_disableIRQ:
    {
      COMMIT_POINT();

      if (source >= NUM_INTERRUPT_SOURCES) {
	inv->exit.code = RC_eros_key_RequestError;
	break;
      }
      
      if (!vis_IsAlloc(vis)) {
	inv->exit.code = RC_eros_DevPrivs_AllocFail;
	break;
      }
      
      InterruptSourceDisable(source);
  
      inv->exit.code = RC_OK;
      break;
    }
    
  case OC_eros_DevPrivs_waitIRQ:
    {
      if (source >= NUM_INTERRUPT_SOURCES) {
        COMMIT_POINT();
	inv->exit.code = RC_eros_key_RequestError;
	break;
      }
      
      if (!vis_IsAlloc(vis)) {
        COMMIT_POINT();
	inv->exit.code = RC_eros_DevPrivs_AllocFail;
	break;
      }

      InterruptSourceEnable(source);	// this should not be necessary,
		// or, why have a separate enable?

      irq_DISABLE();
      
      if (!vis->isPending) {
	DEBUG(sleep)
	  printf("DevPrivs: Sleeping for int source %d\n", source);
        act_SleepOn(act_Current(), &vis->sleeper);


	irq_ENABLE();
	act_Yield(act_Current());
      }
      irq_ENABLE();

      COMMIT_POINT();

      assert(vis->isPending);
      DEBUG(sleep)
	printf("Int source %d was pending already\n", source);

      irq_DISABLE();
      vis->isPending = false;
      irq_ENABLE();
	
      inv->exit.code = RC_OK;
      break;
    }
    
  case OC_eros_DevPrivs_publishMem:
    {
      PmemInfo *pmi = 0;
      kpa_t base = inv->entry.w1;
      kpa_t bound = inv->entry.w2;
      bool readOnly = inv->entry.w3;

      COMMIT_POINT();

      if ((base % EROS_PAGE_SIZE) || (bound % EROS_PAGE_SIZE)) {
	inv->exit.code = RC_eros_key_RequestError;
	break;
      }

      if (base >= bound) {
	inv->exit.code = RC_eros_key_RequestError;
	break;
      }

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
	inv->exit.code = RC_eros_key_NoAccess;
      }

      break;
    }

  case OC_eros_key_getType:
    COMMIT_POINT();
    inv->exit.code = RC_OK;
    inv->exit.w1 = AKT_DevicePrivs;
    break;

  default:
    COMMIT_POINT();
    inv->exit.code = RC_eros_key_UnknownRequest;
    break;
  }

  return;
}
