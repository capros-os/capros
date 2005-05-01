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

#include <eros/target.h>
#include <eros/i486/io.h>
#include <disk/LowVolume.h>
#include <kerninc/BootInfo.h>
#include "boot.h"
#include "boot-asm.h"
#include "debug.h"

#define DESIRED_MODE 101

extern KeyBits IplKey;

int
main()
{
  uint32_t consoleMode = 0;
  BootInfo *bi;
  int vidmode;

  /* This used to be after the geometry probe, but there should be no
   * impediment to doing it immediately. */
  gateA20();

  bi = ProbeMemory();

  CaptureBootDisk(bi);

  vidmode = GetDisplayMode();
  printf("Video mode is 0x%x\n", vidmode);

  if(!InitVESA())
    consoleMode = ChooseVESA();

  Interact(bi);
  
  bi->volFlags = VolFlags;	/* copy stuff to BootInfo */
  bi->iplSysId = IplSysId;

  memcpy(&bi->iplKey, &IplKey, sizeof(IplKey));
  
  if (VolFlags & VF_RAMDISK) {
    LoadRamDisk(bi);
  }
  else if (VolFlags & VF_COMPRESSED) {
    printf("Compressed volumes must be marked for ramdisk load.\n");
    halt();
  }

  if (CND_DEBUG(step)) {
    printf("Press any key to continue\n");
    waitkbd();
  }

  PreloadDivisions(bi);
  
  if (CND_DEBUG(step)) {
    printf("Press any key to continue\n");
    waitkbd();
  }

#if 0
  if (VolFlags & VolHdr::VF_DEBUG) {
    printf("Press any key to load kernel\n");
    waitkbd();
  }
#endif

  LoadKernel(bi);
  /* AFTER LoadKernel can no longer do disk I/O */

  if (CND_DEBUG(step)) {
    printf("Press any key to continue\n");
    waitkbd();
  }

  /* spin down floppy drive to minimize wear.  0x3f2 is the floppy
   * controller's device output register.  0x08 says spin everything
   * down.  It would be better to do this with a BIOS call, but I
   * don't have my BIOS reference handy at the moment.
   */
  outb(0x08, 0x3f2);

#if 0
  if (VolFlags & VolHdr::VF_DEBUG) {
    printf("Press any key to start kernel\n");
    waitkbd();
  }
#endif
  
  ShowMemory(bi);
  ShowDivisions(bi);

#if 0
  printf("\nPress any key to start kernel\n");
  waitkbd();
#endif

  printf("\nStarting kernel with bi=0x%08x\n", BOOT2PA(bi, BootInfo *)); 

  /* Will set VESA mode, if available; otherwise, do nothing. */
  SetConsoleMode(bi, consoleMode);

#if 0				/* set 1 to force text console */
  SetDisplayMode(vidmode);
  bi->useGraphicsFB = false;
#endif

  /* Fix up the pointers in the BootInfo structure to be absolute
   * physical addresses. Past this point you can no longer call
   * BootAlloc() */
  bi->memInfo = BOOT2PA(bi->memInfo, MemInfo *);
  bi->divInfo = BOOT2PA(bi->divInfo, DivisionInfo *);
  bi->consInfo = BOOT2PA(bi->consInfo, ConsoleInfo *);

  StartKernel(KERNPBASE, BOOT2PA(bi, BootInfo *));
}

void
Interact(BootInfo * bi)
{
  DEBUG(unimpl) printf("Interact() not implemented\n");
}
