/*
 * Copyright (C) 2009, Strawberry Development Group.
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

#include <eros/target.h>
#include <eros/Invoke.h>
#include <idl/capros/Number.h>
#include <idl/capros/SpaceBank.h>
#include <idl/capros/Logfile.h>

#include <idl/capros/Constructor.h>
#include <domain/Runtime.h>

#include <domain/domdbg.h>

#define KR_LOGFILEC KR_APP(0)
#define KR_OSTREAM  KR_APP(1)
#define KR_LOGFILE  KR_APP(2)

const uint32_t __rt_stack_pointer = 0x20000;
const uint32_t __rt_unkept = 1;

#define ckOK \
  if (result != RC_OK) \
    kdprintf(KR_OSTREAM, "Line %d result is 0x%08x!\n", __LINE__, result);

#define ckNoRecord \
  if (result != RC_capros_Logfile_NoRecord) \
    kdprintf(KR_OSTREAM, "Line %d result is 0x%08x!\n", __LINE__, result);

struct logrec {
  capros_Logfile_recordHeader hdr;
  uint64_t data1[100];
  uint32_t data2;
  uint32_t trailer;
} logrecord, rcvrecord;

int
main(void)
{
  result_t result;
  uint32_t isDiscrete;
  uint32_t currentID = 0;
  uint32_t lengthGotten;
  int i, j;

  logrecord.hdr.length = sizeof(logrecord);
  logrecord.trailer = sizeof(logrecord);

#define ckLengthGotten \
  if (lengthGotten != sizeof(rcvrecord)) \
    kdprintf(KR_OSTREAM, "Line %d length is %d!\n", __LINE__, lengthGotten);
#define ckID(goodid) \
  if (rcvrecord.hdr.id != goodid) \
    kdprintf(KR_OSTREAM, "Line %d id is %d!\n", __LINE__, rcvrecord.hdr.id);

  result = capros_Constructor_isDiscreet(KR_LOGFILEC, &isDiscrete);
  ckOK
  if (isDiscrete) {
    kprintf(KR_OSTREAM,
	    "Constructor alleges discretion.\n");
  } else {
    kdprintf(KR_OSTREAM,
	     "Constructor is not discreet.\n");
  }

  kprintf(KR_OSTREAM, "Creating logfile.\n");

  /* we've now got all the pieces -- let the test begin! */

  result = capros_Constructor_request(KR_LOGFILEC,
			       KR_BANK, KR_SCHED, KR_VOID,
			       KR_LOGFILE);
  ckOK

  result = capros_Logfile_deleteOldestRecord(KR_LOGFILE);
  ckNoRecord

  logrecord.hdr.id = ++currentID;
  kprintf(KR_OSTREAM, "About to appendRecord.\n");
  result = capros_Logfile_appendRecord(KR_LOGFILE,
                       sizeof(logrecord), (uint8_t *)&logrecord);
  ckOK

  kprintf(KR_OSTREAM, "About to getNextRecord.\n");
  result = capros_Logfile_getNextRecord(KR_LOGFILE,
             capros_Logfile_nullRecordID, sizeof(rcvrecord),
             (uint8_t *)&rcvrecord, &lengthGotten);
  ckOK
  ckLengthGotten
  ckID(currentID);

  kprintf(KR_OSTREAM, "About to deleteOldestRecord.\n");
  result = capros_Logfile_deleteOldestRecord(KR_LOGFILE);
  ckOK

#define numSizes 2
  unsigned long sizes[numSizes] = { 2048, 16*4096 };
  for (i = 0; i < numSizes; i++) {
    int records = sizes[i] / sizeof(logrecord);

    result = capros_Logfile_setDeletionPolicyByID(KR_LOGFILE, records);
    ckOK

    for (j = 0; j < records * 2; j++) {
      logrecord.hdr.id = ++currentID;
      result = capros_Logfile_appendRecord(KR_LOGFILE,
                           sizeof(logrecord), (uint8_t *)&logrecord);
      ckOK
    }

    capros_Logfile_RecordID testID = currentID - records/2;
    kprintf(KR_OSTREAM, "About to getNextRecord.\n");
    result = capros_Logfile_getNextRecord(KR_LOGFILE,
               testID, sizeof(rcvrecord),
               (uint8_t *)&rcvrecord, &lengthGotten);
    ckOK
    ckLengthGotten
    ckID(testID+1);
  }

  kprintf(KR_OSTREAM, "Destroying logfile.\n");

  result = capros_key_destroy(KR_LOGFILE);
  ckOK

  kprintf(KR_OSTREAM, "Done.\n");

  return 0;
}

