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


#include <erosimg/App.h>
#include <erosimg/Volume.h>
#include <eros/Reserve.h>

void
PrintRsrvDir(Volume* pVol)
{
  unsigned i;

  diag_printf("Reserve directory:\n");
  diag_printf("\nP/N OID                     AllocCount LogLoc\n");
  for (i = 0; i < MAX_CPU_RESERVE; i++) {
    const CpuReserveInfo *pCri= vol_GetReserve(pVol, i);

    if (pCri->normPrio == -2 && pCri->rsrvPrio == -2)
      continue;
    
    diag_printf("%3d  per=0x%08x%08x dur=0x%08x%08x rsrv=%d\n"
		 "     quanta=0x%08x%08x start = 0x%08x%08x norm %d\n",
		 pCri->index,
		 (uint32_t) (pCri->period >> 32),
		 (uint32_t) pCri->period,
		 (uint32_t) (pCri->duration >> 32),
		 (uint32_t) pCri->duration,
		 pCri->rsrvPrio,
		 (uint32_t) (pCri->quanta >> 32),
		 (uint32_t) pCri->quanta,
		 (uint32_t) (pCri->start >> 32),
		 (uint32_t) pCri->start,
		 pCri->normPrio);
  }
}
