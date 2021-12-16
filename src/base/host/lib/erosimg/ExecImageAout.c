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
#include <a.out.h>

#include <erosimg/App.h>
#include <erosimg/Parse.h>
#include <erosimg/Intern.h>
#include <erosimg/ExecImage.h>


bool xi_InitAout(ExecImage *pImage)
{
  struct exec *exehdr = (struct exec*) pImage->image;

  switch (N_MAGIC(*exehdr)) {
  case OMAGIC:			/* linked with -N */
    pImage->imageTypeName = "OMAGIC";
    break;
  case NMAGIC:			/* linked with -n */
    pImage->imageTypeName = "NMAGIC";
    break;
  case ZMAGIC:			/* linked normally */
    pImage->imageTypeName = "ZMAGIC";
    break;
#ifdef QMAGIC
  case QMAGIC:			/* linux qmagic - demand paged with */
				/* exec header in text region, page
				 * 0 unmapped. 
				 */
    pImage->imageTypeName = "QMAGIC";
    diag_fatal(0, "Do not use LINUX qmagic format.\n");
#endif
  default:
    return false;
  }

  pImage->entryPoint = exehdr->a_entry;

  pImage->nRegions = 0;

  if (N_MAGIC(*exehdr) == OMAGIC) {
    /* Text+Data+BSS in one region, RWX: */
    pImage->nRegions = 1;
    pImage->regions = 
      (ExecRegion *) malloc(sizeof(ExecRegion) * pImage->nRegions);
    pImage->regions[0].perm = ER_R | ER_W | ER_X;
    pImage->regions[0].vaddr = exehdr->a_entry;
    pImage->regions[0].memsz = exehdr->a_text + exehdr->a_data + exehdr->a_bss;
    pImage->regions[0].filesz = exehdr->a_text + exehdr->a_data;
    pImage->regions[0].offset = N_TXTOFF(*exehdr);
  }
  else {
    /* All other formats use distinct permissions, bss abutted to
     * data.
     */
    if (exehdr->a_text)
      pImage->nRegions++;
    if (exehdr->a_data || exehdr->a_bss)
      pImage->nRegions++;
    
    pImage->regions = 
      (ExecRegion *) malloc(sizeof(ExecRegion) * pImage->nRegions);

    pImage->nRegions = 0;
    if (exehdr->a_text) {
      pImage->regions[pImage->nRegions].perm = ER_R | ER_X;
      pImage->regions[pImage->nRegions].vaddr = exehdr->a_entry;
      pImage->regions[pImage->nRegions].memsz = exehdr->a_text;
      pImage->regions[pImage->nRegions].filesz = exehdr->a_text;
      pImage->regions[pImage->nRegions].offset = N_TXTOFF(*exehdr);
      pImage->nRegions++;
    }
    if (exehdr->a_data || exehdr->a_bss) {
      pImage->regions[pImage->nRegions].perm = ER_R | ER_W;
      pImage->regions[pImage->nRegions].vaddr = exehdr->a_entry;
      pImage->regions[pImage->nRegions].memsz = exehdr->a_data + exehdr->a_bss;
      pImage->regions[pImage->nRegions].filesz = exehdr->a_data;
      pImage->regions[pImage->nRegions].offset = N_TXTOFF(*exehdr) + exehdr->a_text;
      pImage->nRegions++;
    }
  }

  return true;
}
