/*
 * Copyright (C) 1998, 1999, Jonathan S. Shapiro.
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
#include <kerninc/Debug.h>
#include <kerninc/Process.h>
#include <kerninc/Invocation.h>
#include <kerninc/multiboot.h>
#include <kerninc/ObjectSource.h>
#include <kerninc/CPU.h>
#include <kerninc/CpuReserve.h>


struct grub_multiboot_info * MultibootInfoPtr;

extern void ioReg_Init();
extern void UserIrqInit();

#ifdef OPTION_DDB
extern void ddb_init();
#endif

extern Activity *StartIdleActivity();

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

  printf("IPL OID = 0x%08lx%08lx, activity = 0x%08x .\n",
         (uint32_t) (iplOid >> 32),
         (uint32_t) iplOid,
         activity );

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

int
main(void)
{
  const char * p;
  int i;
  OID iplOid;
  uint32_t bootDrive;
  bool debugBoot;

  Activity *idleActivity;

  /* Parse the "command line" parameters. */
  p = (const char *)MultibootInfoPtr->cmdline;

#if 0
  printf("Cmd line %s\n", p);
#endif
  /* Skip kernel file name. */
  while (*p != ' ' && *p != 0) p++;
  assert(*p == ' ');
  p++;

  /* String begins with 0xnnnnnnnnnnnnnnnn for ipl key OID. */
  iplOid = strToUint64(&p);

  /* Next argument is 0xnnnnnnnn for boot drive. */
  assert(*p == ' ');
  p++;
  assert(*p == '0');
  p++;
  assert(*p == 'x');
  p++;
  bootDrive = 0;
  for (i = 0; i < 8; i++, p++) {
    assert(*p != 0);	/* ensure against a short string */
    bootDrive = (bootDrive << 4) + charToHex(*p);
  }

  /* Next argument if any is debug flag. */
  debugBoot = (*p != 0);

  physMem_Init();

  cpu_BootInit();

  mach_BootInit();
  /* Interrupts are now enabled. */
  
#ifdef OPTION_DDB
  ddb_init();

  if (debugBoot)
    dprintf(true, "Stopping due to kernel Debug option.\n");
#endif

  objC_InitObjectSources();
  /* Multiboot structures not needed after this point
     (but alas remain reserved). */

  UserIrqInit();

  sysT_BootInit();

  inv_BootInit();

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

  irq_DISABLE();

  act_SetRunning(idleActivity);

  StartIplActivity(iplOid);
  
  act_Reschedule();
  /* objH_ReleasePinnedObjects() not necessary */
  proc_Resume();		/* does not return. */
}
