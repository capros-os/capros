/*
 * Copyright (C) 2006, 2008, Strawberry Development Group.
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
Research Projects Agency under Contract Nos. W31P4Q-06-C-0040 and
W31P4Q-07-C-0070.  Approved for public release, distribution unlimited. */

#include <string.h>
#include <kerninc/kernel.h>
#include <kerninc/KernStream.h>
#include <kerninc/multiboot.h>

/* Set things up the way GRUB would, which is what the kernel boot
   expects. */

struct grub_multiboot_info MultibootInfo;
struct grub_mod_list ModList;
#define numMmaps 4
struct grub_mmap MemMap[numMmaps];

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

static void
MemMap_init(struct grub_mmap * mmp, grub_uint32_t type,
            grub_uint32_t addr, grub_uint32_t len)
{
  mmp->size = sizeof(struct grub_mmap)-4;
  mmp->base_addr_low = addr;
  mmp->base_addr_high = 0;
  mmp->length_low = len;
  mmp->length_high = 0;
  mmp->type = type;
}

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

  struct grub_mmap * mmp = &MemMap[0];
  /* On EP9315, SDRAM is 32MB beginning at 0 ... */
  MemMap_init(mmp++, 1 /* available RAM */, 0, 0x02000000);

  /* and 32MB beginning at 0x04000000 */
  MemMap_init(mmp++, 1 /* available RAM */, 0x04000000, 0x02000000);

  /* On EP9315, AHB device registers. */
  MemMap_init(mmp++, 4567 /* private convention: device registers */,
              0x80000000, 0x800d0000 - 0x80000000);

  /* On EDB9315, APB device registers. */
  MemMap_init(mmp++, 4567 /* private convention: device registers */,
              0x80810000, 0x80950000 - 0x80810000);
  assert(mmp <= &MemMap[numMmaps]);

  mi->mmap_addr = (grub_uint32_t)&MemMap;
  mi->mmap_length = sizeof(struct grub_mmap) * (mmp - &MemMap[0])
                     -4;	/* that's just the way it is */

  MultibootInfoPtr = &MultibootInfo;
}
