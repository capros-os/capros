/*
 * Copyright (C) 1998, 1999, Jonathan S. Shapiro.
 * Copyright (C) 2008, Strawberry Development Group.
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

#include <erosimg/App.h>
#include <erosimg/Volume.h>

extern void PrintDiskKey(KeyBits);

void
PrintVolHdr(Volume* pVol)
{
  const VolHdr *vh = vol_GetVolHdr(pVol);

  diag_printf("Volume header:\n");
#if 0
  diag_printf("  sectors        %d\n", vh->sectors);
  diag_printf("  cylinders      %d\n", vh->cylinders);
  diag_printf("  heads          %d\n", vh->heads);
  diag_printf("  Total Sectors  %d\n", vh->totSectors);
#endif
  diag_printf("  %-20s %d\n", "Hdr Version", vh->HdrVersion);
  diag_printf("  %-20s %d\n", "Page Size", vh->PageSize);
  diag_printf("  %-20s %d\n", "Pri Div Tbl", vh->DivTable);
  diag_printf("  %-20s %d\n", "Alt Div Tbl", vh->AltDivTable);

  diag_printf("  %-20s 0x%02x", "Flags", vh->BootFlags);

  if (vh->BootFlags) {
    bool haveshown = false;

    diag_printf(" [");
    if (vh->BootFlags & VF_BOOT) {
      diag_printf("%sBOOT", haveshown ? "," : "");
      haveshown = true;
    }
    diag_printf("]\n");
  }
  else
    diag_printf("\n");

  diag_printf("  %-20s %d\n", "Vol Sectors", vh->VolSectors);
  diag_printf("  %-20s '%c%c%c%c'\n", "Signature",
	       vh->signature[0],
	       vh->signature[1],
	       vh->signature[2],
	       vh->signature[3]
	       );
  diag_printf("  %-20s 0x%08x%08x\n", "IPL sysid:",
	       (uint32_t) (vh->iplSysId >> 32), (uint32_t) vh->iplSysId);

  diag_printf("  %-20s ", "IPL key:");
  PrintDiskKey(vh->iplKey);
  diag_printf("\n\n");
}
