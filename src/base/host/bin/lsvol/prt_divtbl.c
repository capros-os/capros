/*
 * Copyright (C) 1998, 1999, Jonathan S. Shapiro.
 * Copyright (C) 2008, 2009, Strawberry Development Group.
 *
 * This file is part of the CapROS Operating System,
 * and is derived from the EROS Operating System.
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

void
PrintDivTable(Volume *pVol)
{
  int i;

  diag_printf("%-4s %-8s %-8s %-8s %-10s\n",
	       "Div", "Start", "End", "Size", "Type/Info");
    
  for (i = 0; i < vol_MaxDiv(pVol); i++) {
    const Division *d = vol_GetDivision(pVol, i);
      
    switch(d->type) {
    case dt_Boot:
    case dt_DivTbl:
    case dt_FailStart:
      diag_printf("%-4d %-8d %-8d %-8d %-10s\n",
		   i, d->start, d->end, d->end - d->start, 
		   div_TypeName(d->type));
      break;
    case dt_Object:
    case dt_Log:
    case dt_Kernel:
      diag_printf("%-4d %-8d %-8d %-8d %-10s\n%32s%s=[",
		   i, d->start, d->end, d->end - d->start, 
		   div_TypeName(d->type), "",
		   (d->type == dt_Log) ? "LID" : "OID");
      diag_printOid(get_target_oid(&d->startOid));
      diag_printf(", ");
      diag_printOid(get_target_oid(&d->endOid));
      diag_printf(")\n");
      if (d->flags) {
	diag_printf("  [ ");
	diag_printf("]\n");
      }
      break;
    }
  }
}
