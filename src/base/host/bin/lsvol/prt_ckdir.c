/*
 * Copyright (C) 1998, 1999, Jonathan S. Shapiro.
 * Copyright (C) 2008, Strawberry Development Group.
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
#include <disk/TagPot.h>
#include <disk/CkptRoot.h>

void
PrintCkptDir(Volume* pVol)
{
  diag_printf("Checkpoint header:\n");
  diag_printf("  sequenceNumber        %#llx\n",
	       pVol->curDskCkpt->mostRecentGenerationNumber);
  diag_printf("  maxLogLid             0x%x\n",
	       pVol->curDskCkpt->endLog);
  diag_printf("  maxNPAllocCount       %#x\n",
	       pVol->curDskCkpt->maxNPAllocCount);
		 
#if 0	// this is not working now ...
  uint32_t thrdStart;
  uint32_t thrdEnd;
  uint32_t dirStart;
  uint32_t dirEnd;
  uint32_t pg;
  unsigned i;

  thrdStart = rsrvEnd;
  thrdEnd = thrdStart + pVol->curDskCkpt->hdr.nThreadPage;
  dirStart = thrdEnd;
  dirEnd = dirStart + pVol->curDskCkpt->hdr.nDirPage;

  diag_printf("\nCheckpoint thread pages at log locations:\n");
  for (pg = thrdStart; pg < thrdEnd; ) {
    for (; pg < thrdEnd; pg++)
      diag_printf("  0x%08x", pVol->curDskCkpt->dirPage[pg]);
    diag_printf("\n");
  }

  diag_printf("\nCheckpoint directory pages at log locations:\n");
  for (pg = dirStart; pg < dirEnd; ) {
    for (; pg < dirEnd; pg++)
      diag_printf("  0x%08x", pVol->curDskCkpt->dirPage[pg]);
    diag_printf("\n");
  }

  diag_printf("\nThreads:\n");
  for (i = 0; i < vol_NumThread(pVol); i++) {
    ThreadDirent tde = vol_GetThread(pVol, i);
    diag_printf("OID=0x%08x%08x  ac=0x%08x  ",
		 (uint32_t) (tde.oid>>32), (uint32_t) tde.oid, tde.allocCount);
    if (i%2 == 1)
      diag_printf("\n");
  }
  if (vol_NumThread(pVol) % 2 != 0)
    diag_printf("\n");

  diag_printf("\nP/N OID                     AllocCount LogLoc\n");
  for (i = 0; i < vol_NumDirent(pVol); i++) {
    CkptDirent de = vol_GetDirent(pVol, i);

    char pageNode = (de.type == FRM_TYPE_NODE) ? 'N' : 'P';
    
    if (! CONTENT_LID(de.lid))
      diag_printf("%c   OID=0x%08x%08x  %-8d   <zero>\n",
		   pageNode,
		   (uint32_t) (de.oid >> 32),
		   (uint32_t) de.oid,
		   de.count);
    else 
      diag_printf("%c   OID=0x%08x%08x  %-8d   0x%08x\n",
		   pageNode,
		   (uint32_t) (de.oid >> 32),
		   (uint32_t) de.oid,
		   de.count,
		   de.lid);
  }
#endif
}
