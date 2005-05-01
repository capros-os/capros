/*
 * Copyright (C) 1998, 1999, Jonathan S. Shapiro.
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

/* main.cxx - initialization and startup for EROS.
 * 
 * One of the design rules for EROS was that no global class should
 * require construction.  It is assumed that any classes that have
 * global instances will supply an init() routine that can be directly
 * called from the EROS main() routine.  On entry to main(),
 * interrupts are disabled.
 */

/* currently, no arguments to the EROS kernel: */

#include <kerninc/kernel.h>
/* #include "lostart.hxx" */
#include <kerninc/KernStream.h>
#include <kerninc/Machine.h>
#include <kerninc/ObjectCache.h>
#include <kerninc/util.h>
#include <kerninc/Activity.h>
#include <kerninc/IRQ.h>
#include <kerninc/Debug.h>
/*#include <kerninc/BlockDev.h>*/
/*#include <kerninc/Partition.h>*/
#include <kerninc/Process.h>
#include <kerninc/Invocation.h>
/*#include <disk/DiskKey.hxx>*/
#include <kerninc/BootInfo.h>
#include <kerninc/ObjectSource.h>
#include <kerninc/CPU.h>
#include <kerninc/CpuReserve.h>


int main(int,char**);

/* NOTE: this is NOT an extern declaration! */
BootInfo *BootInfoPtr;

extern void ioReg_Init();
extern void UserIrqInit();

#ifdef OPTION_DDB
extern void ddb_init();
#endif

extern Activity *StartIdleActivity();
extern void StartCheckActivity();

#if 0
ReadyQueue iplRQ = {
  &prioQueues[pr_High],
  (1u << pr_High),
  0,
  0
};
#endif

void
StartIplActivity()
{
  Activity *activity = 0;
  Key *k = 0;

  activity = act_AllocActivity();
  assert(activity);

  assert (keyBits_IsUnprepared(&activity->processKey));
  assert( keyBits_IsHazard(&activity->processKey) == false );

#ifdef OPTION_DDB
  if ( mach_IsDebugBoot() )
    dprintf(true, "Stopping before waking up IPL activity.\n");
#endif

  /* Forge a domain key for this activity: */
  k = &activity->processKey; /*@ not null @*/

  if(keyBits_IsType(&BootInfoPtr->iplKey, KKT_Process)) {
    keyBits_InitType(k, KKT_Process);
    k->u.unprep.oid = BootInfoPtr->iplKey.u.unprep.oid;
    k->u.unprep.count = BootInfoPtr->iplKey.u.unprep.count;
  }
  else {
    printf("No IPL key!\n");
  }

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

int
main(int is/* isboot */, char **ch)
{
  Activity *idleActivity;

  act_curActivity = 0;

  /* Initialize the stuff we don't go anywhere without: */
  mach_BootInit();

  cpu_BootInit();

  UserIrqInit();

  sysT_BootInit();

  inv_BootInit();
  
#ifdef OPTION_DDB
  ddb_init();
#endif

#if 0
  Debugger();
#endif

  /* Initialize global variables */
  keyBits_InitToVoid(&key_VoidKey);
  key_Prepare(&key_VoidKey);

  inv_InitInv(&inv);
  objH_StallQueueInit();
  ioReg_Init();

  heap_init();

  printf("Heap initialized...\n");

  /* Allocate all of the boot-time allocated structures: */
  proc_AllocUserContexts(); /* machine dependent! */

  act_AllocActivityTable();

  res_AllocReserves();

  idleActivity = StartIdleActivity();
 
  StartCheckActivity();
  
  /* NOTE: If we need to reserve extra space for the kernel heap, this
     is where to do it, though we *should* be able to steal frames
     from the object cache at some point. */

  /* Initialize the object sources, then allocate space for the
     in-core nodes, pages, and the CoreTable: */
  objC_Init();
  
  printf("Object cache initialized...\n");

#ifdef KKT_TIMEPAGE
  /* Following should be done before any possibility of a time key
   * getting prepared:
   */
  sysT_InitTimePage();
#endif

  StartIplActivity();

  act_Dequeue(idleActivity);

  act_curActivity = idleActivity;
  act_curActivity->state = act_Running;

  irq_DISABLE();
  
  proc_Resume(idleActivity->context);
}
