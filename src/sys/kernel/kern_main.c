/*
 * Copyright (C) 1998, 1999, Jonathan S. Shapiro.
 * Copyright (C) 2005, 2006, 2007, 2008, Strawberry Development Group.
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

/* kern_main.c - initialization and startup for EROS.
 * 
 * One of the design rules for EROS was that no global class should
 * require construction.  It is assumed that any classes that have
 * global instances will supply an init() routine that can be directly
 * called from the EROS main() routine.  On entry to main(),
 * interrupts are disabled.
 */

#include <kerninc/kernel.h>
#include <kerninc/KernStream.h>
#include <kerninc/Machine.h>
#include <kerninc/ObjectCache.h>
#include <kerninc/util.h>
#include <kerninc/Activity.h>
#include <kerninc/IRQ.h>
#include <kerninc/IORQ.h>
#include <kerninc/Debug.h>
#include <kerninc/Process.h>
#include <kerninc/Invocation.h>
#include <kerninc/multiboot.h>
#include <kerninc/ObjectSource.h>
#include <kerninc/CPU.h>
#include <kerninc/CpuReserve.h>
#include <disk/NPODescr.h>
#include <kerninc/Key-inline.h>

struct grub_multiboot_info * MultibootInfoPtr;
struct NPObjectsDescriptor * NPObDescr;

extern void UserIrqInit();

#ifdef OPTION_DDB
extern void ddb_init();
#endif

#if 0
ReadyQueue iplRQ = {
  &prioQueues[pr_High],
  (1u << pr_High),
  0,
  0
};
#endif

static void
StartIplActivity(OID iplOid)
{
  Activity *activity = 0;
  Key *k = 0;

  activity = act_AllocActivity();
  assert(activity);

  assert (keyBits_IsUnprepared(&activity->processKey));
  assert( keyBits_IsHazard(&activity->processKey) == false );

  printf("IPL OID = %#llx, activity = %#.8x .\n",
         iplOid, activity );

  /* Forge a domain key for this activity: */
  k = &activity->processKey; /*@ not null @*/

  keyBits_InitType(k, KKT_Process);
  k->u.unprep.oid = iplOid;
  k->u.unprep.count = 0;	/* Is this right? */

  /* The process prepare logic will appropriately adjust this priority
     if it is wrong -- this guess only has to be good enough to get
     the activity scheduled. */

  activity->readyQ = dispatchQueues[pr_High];
 
  /*activity->priority = pr_High;*/

  /* is this wrong?*/
#if 1
  kstream_BeginUserActivities();
#endif

  act_Wakeup(activity);
}

// Does not return.
int
main(void)
{
  OID iplOid = NPObDescr->IPLOID;

  physMem_Init();
  /* The Multiboot structure is not needed after this point
     (but alas remains reserved). */

  cpu_BootInit();

  heap_init();

  mach_BootInit();
  /* Interrupts are now enabled. */
  
#ifdef OPTION_DDB
  ddb_init();

#if 0
  dprintf(true, "Stopping due to kernel Debug option.\n");
#endif
#endif

  UserIrqInit();

  sysT_BootInit();

  inv_BootInit();

  /* Initialize global variables */
  keyBits_InitToVoid(&key_VoidKey);
  key_Prepare(&key_VoidKey);	// will never Yield

  inv_InitInv(&inv);
  objH_StallQueueInit();
  IORQ_Init();

  printf("Heap initialized...\n");

  /* Allocate all of the boot-time allocated structures: */
  proc_AllocUserContexts(); /* machine dependent! */

  act_AllocActivityTable();

  res_AllocReserves();

  extern Activity * StartIdleActivity(void);
  Activity * idleActivity = StartIdleActivity();
  extern void CreateMigratorActivity(void);
  CreateMigratorActivity();
 
  /* NOTE: If we need to reserve extra space for the kernel heap, this
     is where to do it, though we *should* be able to steal frames
     from the object cache at some point. */

  /* Allocate space for the
     in-core nodes, pages, and the CoreTable: */
  objC_Init();

  printf("Object cache initialized...\n");

  objC_InitObjectSources();

  extern void preload_Init(void);
  preload_Init();

#ifdef KKT_TIMEPAGE
  /* Following should be done before any possibility of a time key
   * getting prepared:
   */
  sysT_InitTimePage();
#endif

  irq_DISABLE();

  act_SetRunning(idleActivity);

  StartIplActivity(iplOid);
  
  ExitTheKernel();		/* does not return. */
}
