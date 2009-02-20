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

#include <string.h>
#include <disk/GenerationHdr.h>
#include <erosimg/App.h>
#include <erosimg/Volume.h>
#include <disk/TagPot.h>
#include <disk/CkptRoot.h>

static void
showProcs(uint8_t * p, unsigned int num)
{
  struct DiskProcessDescriptor * dpdp = (struct DiskProcessDescriptor *)p;
  int i;
  for (i = 0; i < num; i++, dpdp++) {
    // dpdp is unaligned, so copy the structure:
    struct DiskProcessDescriptor dpd;
    memcpy(&dpd, dpdp, sizeof(struct DiskProcessDescriptor));

    diag_printf("OID=%#llx ac=%#x",
                get_target_oid(&dpd.oid), dpd.allocCount);
    if (dpd.actHazard) {
      diag_printf(" haz=%d\n", dpd.actHazard);
    } else {
      diag_printf("\n");
    }
  }
}

void
PrintCkptDir(Volume* pVol)
{
  int i;
  uint64_t page[EROS_PAGE_SIZE / sizeof(uint64_t)];
  uint8_t * pagep = (uint8_t *)page;
  int numDescrs;

  diag_printf("Checkpoint header:\n");
  diag_printf("  sequenceNumber        %#llx\n",
	       pVol->curDskCkpt->mostRecentGenerationNumber);
  diag_printf("  maxLogLid             0x%x\n",
	       pVol->curDskCkpt->endLog);
  diag_printf("  maxNPAllocCount       %#x\n",
	       pVol->curDskCkpt->maxNPAllocCount);

  vol_ReadLogPage(pVol, get_target_lid(&pVol->curDskCkpt->generations[0]),
                  page);

  // Display processes.
  DiskGenerationHdr * genHdr = (DiskGenerationHdr *)page;
  // First the ones in the genHdr.
  numDescrs = genHdr->processDir.nDescriptors;
  diag_printf("%d procs in genHdr:\n", numDescrs);
  showProcs(pagep + sizeof(DiskGenerationHdr), numDescrs);

  LID dirLID = get_target_lid(&genHdr->processDir.firstDirFrame);
  for (i = genHdr->processDir.nDirFrames; i-- > 0; dirLID++) {
    vol_ReadLogPage(pVol, dirLID, page);
    numDescrs = *(uint32_t *)pagep;
    diag_printf("%d procs in dir frame %#llx:\n", numDescrs, dirLID);
    showProcs(pagep + sizeof(uint32_t), numDescrs);
  }

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

  diag_printf("\nCheckpoint directory pages at log locations:\n");
  for (pg = dirStart; pg < dirEnd; ) {
    for (; pg < dirEnd; pg++)
      diag_printf("  0x%08x", pVol->curDskCkpt->dirPage[pg]);
    diag_printf("\n");
  }

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
