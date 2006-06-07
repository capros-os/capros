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
struct grub_mmap MemMap;

/* In flash memory, we have the space allocated for kernel text, followed by
   the kernel data section, followed by our stuff. */
#define ModSizeAddr (FlashMemVA + KTextPackedSize + (&_edata - &__data_start))
/* The sysgen-generated GRUB configuration file is copied after the 
8-byte module size. */
#define ConfigFile (ModSizeAddr + 8)

#define kernelCmdLen 46	/* not including terminating nul */
#define moduleCmdLen 33
char KernelCmdline[kernelCmdLen+1];
char ModuleCmdline[moduleCmdLen+1];

void
GrubEmul(void)
{
  register struct grub_multiboot_info * mi = &MultibootInfo;
  mi->flags = (GRUB_MB_INFO_BOOTDEV+GRUB_MB_INFO_CMDLINE
              +GRUB_MB_INFO_MODS+GRUB_MB_INFO_MEM_MAP);
  mi->boot_device = 0;	/* bogus */
  /* KLUDGE: we take advantage of fixed offsets in the config file. */
  /* Copy command lines to RAM so we can nul-terminate them. */
  memcpy(KernelCmdline, (char *)(ConfigFile + 0x48), kernelCmdLen);
  KernelCmdline[kernelCmdLen] = '\0';
  memcpy(ModuleCmdline, (char *)(ConfigFile + 0x7f), moduleCmdLen);
  ModuleCmdline[moduleCmdLen] = '\0';
  mi->cmdline = (grub_uint32_t)KernelCmdline;
#if 0
  printf("ModSize in flash 0x%x ", ModSizeAddr);
#endif

  /* get size of module from hex string */
  char * c = (char *)ModSizeAddr;
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
  ModList.mod_start = (ConfigFile + 164) - PhysMapVA;
  ModList.mod_end = ModList.mod_start + modSize;
  ModList.cmdline = (grub_uint32_t)ModuleCmdline - PhysMapVA;
#if 0
  printf(" %x 0x%x %s\n", modSize, ModList.cmdline,
         (char *)KPAtoP(ModList.cmdline));
#endif
  mi->mods_count = 1;
  mi->mods_addr = (grub_uint32_t)&ModList - PhysMapVA;

  /* On EDB9315, SDRAM is 64MB beginning at 0. */
  MemMap.size = sizeof(MemMap);
  MemMap.base_addr_low = 0;
  MemMap.base_addr_high = 0;
  MemMap.length_low = 64*1024*1024;
  MemMap.length_high = 0;
  MemMap.type = 1;	/* available RAM */
  mi->mmap_addr = (grub_uint32_t)&MemMap;
  mi->mmap_length = sizeof(MemMap) -4;	/* that's just the way it is */

  MultibootInfoPtr = &MultibootInfo;
}
