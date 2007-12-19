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

#include <sys/fcntl.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#if 0
#include <linux/elf.h>		/* FIX: this is scrod... */
#else
#include <elf.h>

typedef Elf32_Ehdr elfhdr;
typedef Elf32_Phdr elf_phdr;

#ifndef ELFMAG
#define	ELFMAG		"\177ELF"
#endif
#ifndef SELFMAG
#define	SELFMAG		4
#endif
#endif

/* Old versions of elf.h didn't define these: */
#ifndef PF_X
#define PF_R		0x4
#define PF_W		0x2
#define PF_X		0x1
#endif

#include <erosimg/App.h>
#include <erosimg/Parse.h>
#include <erosimg/Intern.h>
#include <erosimg/ExecImage.h>


#if 0
static int cmp_phdr(const void *ph1, const void *ph2)
{
  const struct elf_phdr *eph1 = (elf_phdr*) ph1;
  const struct elf_phdr *eph2 = (elf_phdr*) ph2;

  /* unsigned, so need to compare explicitly: */
  if (eph1->p_vaddr < eph2->p_vaddr)
    return -1;
  if (eph1->p_vaddr > eph2->p_vaddr)
    return 1;

  return 0;
}
#endif

bool
xi_InitElf(ExecImage *pImage, uint32_t permMask, uint32_t permValue)
{
  int i;
  elfhdr *exehdr = (elfhdr*) pImage->image;

  if (memcmp(exehdr->e_ident, ELFMAG, SELFMAG) != 0)
    return false;

  pImage->entryPoint = exehdr->e_entry;

  pImage->imageTypeName = "ELF";

  /* Read the program headers: */
  pImage->nRegions = 0;

  /* Make one pass to figure out how many to copy: */

#define selectPH  \
    (phdr->p_type == PT_LOAD \
     && (phdr->p_flags & permMask) == permValue)
  
  for (i = 0; i < exehdr->e_phnum; i++) {
    elf_phdr *phdr =
      (elf_phdr *) &pImage->image[exehdr->e_phoff + exehdr->e_phentsize * i];

    char perm[4] = "\0\0\0";
    char *pbuf = perm;

    if (phdr->p_flags & PF_R)
      *pbuf++ = 'R';
    if (phdr->p_flags & PF_W)
      *pbuf++ = 'W';
    if (phdr->p_flags & PF_X)
      *pbuf++ = 'X';
    
#ifdef DEBUG
    char *ptypes[] = {
      "PT_NULL",
      "PT_LOAD",
      "PT_DYNAMIC",
      "PT_INTERP",
      "PT_NOTE",
      "PT_SHLIB",
      "PT_PHDR",
    };
    
    if (App.IsInteractive())
      diag_printf("phdr[%s] %s va=0x%08x memsz=0x%08x\n"
		   "         filesz=0x%08x offset 0x%x\n",
		   ptypes[phdr->p_type],
		   perm,
		   phdr->p_vaddr,
		   phdr->p_memsz,
		   phdr->p_filesz,
		   phdr->p_offset);
#endif
    
    if (selectPH)
      pImage->nRegions++;
  }

  /* Make another to actually copy them: */
  pImage->regions = 
    (ExecRegion*) malloc(sizeof(ExecRegion) * pImage->nRegions);
  pImage->nRegions = 0;

  for (i = 0; i < exehdr->e_phnum; i++) {
    elf_phdr *phdr =
      (elf_phdr *) &pImage->image[exehdr->e_phoff + exehdr->e_phentsize * i];

    if (selectPH) {
      pImage->regions[pImage->nRegions].perm = phdr->p_flags & 0xf;
      pImage->regions[pImage->nRegions].vaddr = phdr->p_vaddr;
      pImage->regions[pImage->nRegions].memsz = phdr->p_memsz;
      pImage->regions[pImage->nRegions].filesz = phdr->p_filesz;
      pImage->regions[pImage->nRegions].offset = phdr->p_offset;

#if 0
      diag_printf("Region va=0x%x sz=0x%x type=%d\n",
                  pImage->regions[pImage->nRegions].vaddr,
                  pImage->regions[pImage->nRegions].memsz,
                  phdr->p_type);
#endif

      pImage->nRegions++;
    }
  }

  return true;
}
