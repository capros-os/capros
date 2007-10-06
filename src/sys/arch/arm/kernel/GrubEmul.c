/*
 * Copyright (C) 2006, Strawberry Development Group.
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
   Research Projects Agency under Contract No. W31P4Q-06-C-0040. */

#include <string.h>
#include <kerninc/kernel.h>
#include <kerninc/KernStream.h>
#include <kerninc/multiboot.h>

/* Set things up the way GRUB would, which is what the kernel boot
   expects. */

struct grub_multiboot_info MultibootInfo;
struct grub_mod_list ModList;
struct grub_mmap MemMap[3];

/* Kludge: rather than parse the configuration file, we assume the parts
are of fixed size. */
#define kernelCmdLen 46	/* not including terminating nul */
#define moduleCmdLen 33
struct GrubEmulStuff {
  char modSize[8];
  // Following is the sysgen-generated GRUB configuration file.
  char configFile0[72];
  char kernelCmd[kernelCmdLen];
		// "/CapROS-kernel-1 0xdddddddddddddddd 0xdddddddd"
  char configFile1[9];		// "\n\tmodule "
  char moduleCmd[moduleCmdLen];	// "/CapROS-PL-3-1 0xdddddddddddddddd"
  char configFile2[4];		// "\n" and 3 bytes of padding
  uint32_t modStart[];
};

char KernelCmdline[kernelCmdLen+1];
char ModuleCmdline[moduleCmdLen+1];

void
GrubEmul(struct GrubEmulStuff * ges)
{
  register struct grub_multiboot_info * mi = &MultibootInfo;
  mi->flags = (GRUB_MB_INFO_BOOTDEV+GRUB_MB_INFO_CMDLINE
              +GRUB_MB_INFO_MODS+GRUB_MB_INFO_MEM_MAP);
  mi->boot_device = 0;	/* bogus */
  /* Copy command lines to RAM so we can nul-terminate them. */
  memcpy(KernelCmdline, ges->kernelCmd, kernelCmdLen);
  KernelCmdline[kernelCmdLen] = '\0';
  memcpy(ModuleCmdline, ges->moduleCmd, moduleCmdLen);
  ModuleCmdline[moduleCmdLen] = '\0';
  mi->cmdline = (grub_uint32_t)KernelCmdline;
#if 0
  printf("GrubEmulStuff 0x%x ", ges);
#endif

  /* get size of module from hex string */
  char * c = ges->modSize;
  int i;
  uint32_t modSize = 0;
  for (i = 0; i < 8; i++) {
    int ch = *c++;
    if (ch <= '9') {
      ch = ch - '0';
    } else {
      ch = ch - 'a' + 10;
    }
    modSize = (modSize << 4) + ch;
  }

  /* The module information is processed after mach_BootInit, so it
  assumes the memory map is set up.
  Thus it will reference addresses using KPAtoP(). 
  That doesn't work for addresses in flash memory (not implemented). 
  As a kludge, we subtract PhysMapVA to undo KPAtoP. */
  ModList.mod_start = ((kpa_t)ges->modStart) - PhysMapVA;
  ModList.mod_end = ModList.mod_start + modSize;
  ModList.cmdline = (grub_uint32_t)ModuleCmdline - PhysMapVA;
#if 0
  printf(" %x 0x%x %s\n", modSize, ModList.cmdline,
         (char *)KPAtoP(ModList.cmdline));
#endif
  mi->mods_count = 1;
  mi->mods_addr = (grub_uint32_t)&ModList - PhysMapVA;

  /* On EP9315, SDRAM is 64MB beginning at 0. */
  MemMap[0].size = sizeof(struct grub_mmap)-4;
  MemMap[0].base_addr_low = 0;
  MemMap[0].base_addr_high = 0;
  MemMap[0].length_low = 64*1024*1024;
  MemMap[0].length_high = 0;
  MemMap[0].type = 1;	/* available RAM */

  /* On EP9315, AHB device registers. */
  MemMap[1].size = sizeof(struct grub_mmap)-4;
  MemMap[1].base_addr_low = 0x80000000;
  MemMap[1].base_addr_high = 0;
  MemMap[1].length_low = 0x800d0000 - 0x80000000;
  MemMap[1].length_high = 0;
  MemMap[1].type = 4567;	/* private convention: device registers */

  /* On EDB9315, APB device registers. */
  MemMap[2].size = sizeof(struct grub_mmap)-4;
  MemMap[2].base_addr_low = 0x80810000;
  MemMap[2].base_addr_high = 0;
  MemMap[2].length_low = 0x80950000 - 0x80810000;
  MemMap[2].length_high = 0;
  MemMap[2].type = 4567;	/* private convention: device registers */

  mi->mmap_addr = (grub_uint32_t)&MemMap;
  mi->mmap_length = sizeof(MemMap) -4;	/* that's just the way it is */

  MultibootInfoPtr = &MultibootInfo;
}
