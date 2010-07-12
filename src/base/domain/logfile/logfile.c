/*
 * Copyright (C) 2009, 2010, Strawberry Development Group.
 *
 * This file is part of the CapROS Operating System.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */
/* This material is based upon work supported by the US Defense Advanced
Research Projects Agency under Contract No. W31P4Q-07-C-0070.
Approved for public release, distribution unlimited. */

#include <string.h>
#include <eros/Invoke.h>
#include <eros/machine/cap-instr.h>
#include <idl/capros/Logfile.h>
#include <idl/capros/Node.h>
#include <idl/capros/Page.h>
#include <idl/capros/GPT.h>
#include <idl/capros/Range.h>
#include <idl/capros/SpaceBank.h>
#include <idl/capros/Process.h>
#include <domain/InterpreterDestroy.h>
#include "logfile.h"
#include <domain/assert.h>

#define KR_OSTREAM KR_APP(0)
#define KR_WAITER  KR_APP(1)
#define KR_MEMROOT KR_APP(2)
/* NOTE!! The 3 or 4 key registers following KR_MEMROOT are a stack of
 * scratch registers for the recursive procedures. */
#define MemrootL2v 22

#define dbg_memory 0x1
#define dbg_server 0x2

/* Following should be an OR of some of the above */
#define dbg_flags   ( 0u )

#define DEBUG(x) if (dbg_##x & dbg_flags)

#define minEquals(v, x) if ((v) > (x)) (v) = (x)
#define maxEquals(v, x) if ((v) < (x)) (v) = (x)

const uint32_t __rt_stack_pointer = 0x00020800;


/* Log records are stored in a circular buffer.
The circular buffer is divided into blocks of fixed size BLOCKSIZE.
Records are contiguous and can span more than one block. */
#define BLOCKSIZE 4096	// must be a power of 2

/* To support getNextRecord and getPreviousRecord operations,
there are two ways to find a record given its RecordID.

First, we cache a single id/location pair. This provides a
quick way to find a record when there is a single client
doing getNextRecord/getPreviousRecord.

If that cache does not have the record we want, then we look in the index.
The index has the id and location of the first record in each block.
From there we can find the other records in the block by scanning linearly.
The idea is for the block size to be approximately the page size.
Then the linear scan references only one or perhaps two pages.
The index parallels the log so it is also circular.

More precisely:

The log begins and ends on a block boundary and also on a page boundary.
Each block in the memory for the log corresponds one-to-one with
a record in the memory for the index.
LogToIndex(logLoc) (defined below) is the address of the index record
corresponding to the block containing logLoc.

If the log is empty, CBOut can be any location in the log, and CBIn == CBOut.
The index is empty (numIndexRecords == 0)
and indexHi == indexLo == LogToIndex(CBOut).

If the log is nonempty:

CBOut is the beginning of the first (oldest) record in the log.
The first record in the index (indexLo) is LogToIndex(CBOut).

CBIn is the end of the last record in the log (the last byte + 1).
LogToIndex(the beginning of the last record in the log)
is the last record in the index, and indexHi == that record + 1 record.
(indexHi may equal IndexEnd but never equals IndexStart.)

There are two cases for a nonempty log:
1. CBOut < CBIn: the area from CBOut to CBIn has the data.
                 CBLast is CBEnd.
2. CBOut > CBIn:
     The data are from CBOut to CBLast, followed by CBStart to CBIn.
     The area from CBLast to CBEnd is too small to hold a record.

For each index record from the first to the last, inclusive,
taking into account wrapping: the index record contains the id and address
of the first record boundary found by starting from the beginning of
the corresponding block, going forward, and taking into account wrapping.
(Note for example that if the log wraps, and the space from CBLast to CBEnd
has one or more completely empty blocks, index records corresponding to those
blocks will exist and will refer to the record at CBStart.)

The space from CBOut back to the beginning of its block is always empty.

(Note that if the log is nearly full and wraps, indexLo may == indexHi.)

If the log does not wrap (CBIn >= CBOut),
only the following addresses are allocated in the log:
  from RoundDownToPage(CBOut) to RoundUpToPage(CBIn).
If the log wraps (CBIn < CBOut),
only the following addresses are allocated in the log:
  from RoundDownToPage(CBOut) to RoundUpToPage(CBLast)
  and from CBStart to RoundUpToPage(CBIn).
(Note that if the log is empty and CBIn is not on a page boundary,
the page containing CBIn is allocated.)

If the log does not wrap (CBIn >= CBOut),
only the following addresses are allocated in the index:
  from RoundDownToPage(indexLo) to RoundUpToPage(indexHi).
If the log wraps (CBIn < CBOut),
only the following addresses are allocated in the index:
  from RoundDownToPage(indexLo) to RoundUpToPage(IndexEnd)
  and from IndexStart to RoundUpToPage(indexHi).
 */

struct IndexRecord {
  capros_Logfile_RecordID id;
  uint8_t * addr;
};

struct IndexRecord cachedLocation = {
  .id = capros_Logfile_nullRecordID,
  .addr = NULL	// for safety
};

// BPSIZE is the larger of BLOCKSIZE and EROS_PAGE_SIZE.
// Since both are powers of 2, BPSIZE is a multiple of each.
#define BPSIZE (BLOCKSIZE > EROS_PAGE_SIZE ? BLOCKSIZE : EROS_PAGE_SIZE)

// Virtual memory available for the index and log:
// SpaceStart and SpaceEnd must both be multiples of BPSIZE.
#define SpaceStart ((uint8_t *)0x00040000)
#define SpaceEnd   ((uint8_t *)0x02000000)	// for ARM FCSE
// Number of blocks in the log:
#define NumBlocks (((SpaceEnd - SpaceStart) \
                    / (BLOCKSIZE + sizeof(struct IndexRecord))) \
                   & (- (BPSIZE / BLOCKSIZE)))
// Virtual memory allocated to the index:
#define IndexStart ((struct IndexRecord *)SpaceStart)
#define IndexEnd   ((struct IndexRecord *)(SpaceStart + NumBlocks * sizeof(struct IndexRecord)))
// Virtual memory allocated to the log:
#define CBStart    (CBEnd - NumBlocks * BLOCKSIZE)
#define CBEnd      SpaceEnd

/* Pointers into the circular log buffer:
*/
uint8_t * CBIn  = CBStart;
uint8_t * CBOut = CBStart;
uint8_t * CBLast = CBEnd;

// Pointers to the index:
struct IndexRecord * indexLo = IndexStart;
struct IndexRecord * indexHi = IndexStart;
unsigned long numIndexRecords = 0;

static inline capros_Logfile_recordHeader *
RecToHdr(uint8_t * rec)
{
  return (capros_Logfile_recordHeader *)rec;
}

/***********************  Memory manipulation  *************************/

const unsigned int l2nSlots = capros_GPT_l2nSlots;	// a shorter name

static inline uint32_t
RoundDownToPage(uint32_t p)
{
  return p & (-EROS_PAGE_SIZE);
}

static inline uint32_t
RoundUpToPage(uint32_t p)
{
  return (p + EROS_PAGE_SIZE - 1) & (-EROS_PAGE_SIZE);
}

/* Recursive procedure to allocate memory (if not already allocated)
   in the range of addresses from start to end
   relative to the specified gpt which is of the specified l2v. */
result_t
EnsureRangeAllocatedRec(cap_t gpt, uint32_t start, uint32_t end,
  unsigned int l2v)
{
  result_t result;

  DEBUG(memory) kprintf(KR_OSTREAM,
                        "EnsureRangeAllocatedRec start=%#x end=%#x l2v=%d\n",
                        start, end, l2v);
  assert(l2v >= EROS_PAGE_LGSIZE);
  assert((l2v - EROS_PAGE_LGSIZE) % l2nSlots == 0);
  assert(start < end);
  assert(end <= (1 << (l2v + l2nSlots)));

  int f = start >> l2v;
  int l = (end - 1) >> l2v;
  int s;
  for (s = f; s <= l; s++) {
    result = capros_GPT_getSlot(gpt, s, gpt+1);
    assert(result == RC_OK);
    uint32_t akt;
    result = capros_key_getType(gpt+1, &akt);
    if (l2v > EROS_PAGE_LGSIZE) {	// looking for a GPT
      if (result == RC_capros_key_Void) {
        result = capros_SpaceBank_alloc1(KR_BANK, capros_Range_otGPT, gpt+1);
        if (result != RC_OK)
          return result;
        result = capros_GPT_setL2v(gpt+1, l2v - l2nSlots);
        assert(result == RC_OK);
        result = capros_GPT_setSlot(gpt, s, gpt+1);
        assert(result == RC_OK);
      } else {
        assert(result == RC_OK);
        assert(akt == IKT_capros_GPT);
      }
      // ss is the first address in the range covered by this slot:
      uint32_t ss = s << l2v;
      // se is the last address in the range covered by this slot:
      uint32_t se = (s+1) << l2v;
      minEquals(se, end);
      result = EnsureRangeAllocatedRec(gpt+1, start - ss, se - ss,
                                       l2v - l2nSlots);
      if (result != RC_OK)
        return result;
      start = se;	// for next iteration
    } else {				// looking for a page
      if (result == RC_capros_key_Void) {
        result = capros_SpaceBank_alloc1(KR_BANK, capros_Range_otPage, gpt+1);
        if (result != RC_OK)
          return result;
        result = capros_GPT_setSlot(gpt, s, gpt+1);
        assert(result == RC_OK);
      } else {
        assert(result == RC_OK);
        assert(akt == IKT_capros_Page);
      }
      return RC_OK;
    }
  }
  return RC_OK;
}

static inline result_t
EnsureRangeAllocated(uint32_t start, uint32_t end)
{
  result_t result;
  if (start == end)
    return RC_OK;
  result = EnsureRangeAllocatedRec(KR_MEMROOT, start, end, MemrootL2v);
  if (result != RC_OK) {
    assert(result == RC_capros_SpaceBank_LimitReached);
  }
  return result;
}

/* Recursive procedure to deallocate memory (if not already deallocated)
   in the range of addresses from start to end
   relative to the specified gpt which is of the specified l2v. */
void
EnsureRangeDeallocatedRec(cap_t gpt, uint32_t start, uint32_t end,
  unsigned int l2v)
{
  result_t result;

  DEBUG(memory) kprintf(KR_OSTREAM,
                        "EnsureRangeDeallocatedRec start=%#x end=%#x l2v=%d\n",
                        start, end, l2v);
  assert((start & (EROS_PAGE_SIZE-1)) == 0);
  assert((end   & (EROS_PAGE_SIZE-1)) == 0);
  assert(l2v >= EROS_PAGE_LGSIZE);
  assert((l2v - EROS_PAGE_LGSIZE) % l2nSlots == 0);
  assert(start < end);
  assert(end <= (1 << (l2v + l2nSlots)));

  int f = start >> l2v;
  int l = (end - 1) >> l2v;
  int s;
  for (s = f; s <= l; s++) {
    result = capros_GPT_getSlot(gpt, s, gpt+1);
    assert(result == RC_OK);
    uint32_t akt;
    result = capros_key_getType(gpt+1, &akt);
    if (result == RC_capros_key_Void)
      return;	// nothing there; we are done
    if (l2v > EROS_PAGE_LGSIZE) {	// looking for a GPT
      assert(result == RC_OK);
      assert(akt == IKT_capros_GPT);
      // ss is the first address in the range covered by this slot:
      uint32_t ss = s << l2v;
      // se is the last address in the range covered by this slot:
      uint32_t se = (s+1) << l2v;
      minEquals(se, end);
      EnsureRangeDeallocatedRec(gpt+1, start - ss, se - ss,
                             l2v - l2nSlots);
      if (start - ss == 0
          && se - ss == (1 << l2v)) {
        // Deallocated everything under this slot.
        result = capros_SpaceBank_free1(KR_BANK, gpt+1);
        assert(result == RC_OK);
      }
      start = se;	// for next iteration
    } else {				// looking for a page
      assert(result == RC_OK);
      assert(akt == IKT_capros_Page);
      result = capros_SpaceBank_free1(KR_BANK, gpt+1);
      assert(result == RC_OK);
      return;
    }
  }
  return;
}

void
EnsureRangeDeallocated(uint32_t start, uint32_t end)
{
  if (start >= end)
    return;
  EnsureRangeDeallocatedRec(KR_MEMROOT, start, end, MemrootL2v);
}

/***********************  Record deletion  **********************/

capros_Logfile_RecordID deletionPolicyAge = UINT64_MAX;
uint32_t deletionPolicySize = UINT32_MAX;

capros_Logfile_RecordID lastIDAdded = capros_Logfile_nullRecordID;
unsigned long totalLogSpace = 0;

#define bufUnit sizeof(capros_Logfile_recordHeader)
capros_Logfile_recordHeader
    messageBuffer[(capros_Logfile_MaxRecordLength + bufUnit - 1) / bufUnit ];

static inline bool
RecordsExist(void)
{
  return totalLogSpace > 0;
}

static inline struct IndexRecord *
LogToIndex(uint8_t * logLoc)
{
  return IndexStart + (logLoc - CBStart) / BLOCKSIZE;
}

void
DeallocateLogRecord(uint8_t * oldOut, uint8_t * newOut)
{
  EnsureRangeDeallocated(RoundDownToPage((uint32_t)oldOut),
                         RoundDownToPage((uint32_t)newOut));
}

// The log must not be empty.
void
DeleteOldestRecord(void)
{
#if 0
  kprintf(KR_OSTREAM,
          "DeleteOldest CBO=%#x indexLo=%#x ->addr=%#x numIndex=%d CBI=%#x\n",
          CBOut, indexLo, indexLo->addr, numIndexRecords, CBIn);
#endif
  assert(CBOut == indexLo->addr);

  if (cachedLocation.addr == CBOut) {
    // We are deleting the cached location. Invalidate the cache.
    cachedLocation.id = capros_Logfile_nullRecordID;
    cachedLocation.addr = NULL;	// for safety
  }

  unsigned long recordLength = RecToHdr(CBOut)->length;
  uint8_t * endRecord = CBOut + recordLength;
  uint8_t * newOut = endRecord;
  struct IndexRecord * newIndexLo = LogToIndex(newOut);
  if (newOut == CBIn) {		// we are deleting the last record
    assert(recordLength == totalLogSpace);
    assert(indexHi = indexLo + 1);
    /* In this case, we don't move indexLo up to newIndexLo,
    because that could require allocating a new page at newIndexLo
    (different from the existing one at indexLo), which might fail.
    Instead we move indexHi down to indexLo. */
    EnsureRangeDeallocated(RoundUpToPage((uint32_t)indexLo),
                           RoundUpToPage((uint32_t)indexHi));
    indexHi = indexLo;
    DeallocateLogRecord(CBOut, newOut);
  } else {
    assert(recordLength < totalLogSpace);

    uint32_t lowDealloc = RoundDownToPage((uint32_t)indexLo);
    uint32_t hiKeep = RoundUpToPage((uint32_t)indexHi);
    maxEquals(lowDealloc, hiKeep);	// don't dealloc indexHi
    if (CBOut >= CBIn && endRecord == CBLast) {
      assert(indexHi > IndexStart);	// there is a record at the start
      // The log will no longer wrap.
      newOut = CBStart;
      newIndexLo = IndexStart;
      CBLast = CBEnd;
      EnsureRangeDeallocated(lowDealloc, RoundUpToPage((uint32_t)IndexEnd));
      // Deallocate the log record:
      EnsureRangeDeallocated(RoundDownToPage((uint32_t)CBOut),
                             RoundUpToPage((uint32_t)CBLast));
    } else {
      if (newIndexLo == indexLo) {
        // Not deallocating an index record.
        // Change the record to refer to the new oldest record.
        indexLo->addr = newOut;
        indexLo->id = RecToHdr(newOut)->id;
      } else {
        // Deallocate index record(s).
        EnsureRangeDeallocated(lowDealloc,
                               RoundDownToPage((uint32_t)newIndexLo));
      }
      DeallocateLogRecord(CBOut, newOut);
    }
    indexLo = newIndexLo;
  }

  totalLogSpace -= recordLength;
  CBOut = newOut;

  numIndexRecords = indexHi - indexLo;
  if ((signed long)numIndexRecords < 0)
    numIndexRecords += NumBlocks;
}

void
DeleteByID(capros_Logfile_RecordID id)
{
  while (RecordsExist() && indexLo->id < id) {
#if 0
    kprintf(KR_OSTREAM, "Deleting by id %llu %llu\n",
            indexLo->id, id);
#endif
    DeleteOldestRecord();
  }
}

void
CheckDeletionByID(void)
{
  if (lastIDAdded >= deletionPolicyAge) {
    DeleteByID(lastIDAdded - deletionPolicyAge);
  }
}

void
CheckDeletionBySpace(unsigned long newSpace)
{
  while (RecordsExist()
         && totalLogSpace + newSpace > deletionPolicySize) {
#if 0
    kprintf(KR_OSTREAM, "Deleting for space %u %u %u\n",
            totalLogSpace, newSpace, deletionPolicySize);
#endif
    DeleteOldestRecord();
  }
}

/***********************  Record search  **********************/

static struct IndexRecord *
GetIndexRecord(int i)
{
  struct IndexRecord * ir = indexLo + i;
  if (ir >= IndexEnd)
    ir -= NumBlocks;
  return ir;
}

/* Returns u such that
  GetIndexRecord(u)->id <= id < GetIndexRecord(u+1)
  where GetIndexRecord(-1) is like minus infinity
  and GetIndexRecord(numIndexRecords) is like plus infinity. */
int
SearchID(capros_Logfile_RecordID id)
{
  // Do a binary search for the id in the index.
  int l = 0;
  int u = numIndexRecords - 1;
  while (1) {
    if (u < l)
      return u;
    int i = (u + l) / 2;
    if (id < GetIndexRecord(i)->id)
      u = i - 1;
    else
      l = i + 1;
  }
}

// Search for the record with the smallest RecordID greater than id.
// On entry, rec->id <= id.
// Returns NULL if no next record.
uint8_t *
SequentialSearchForNext(capros_Logfile_RecordID id, uint8_t * rec)
{
  capros_Logfile_recordHeader * hdr, * nextHdr;
  uint8_t * nextRec;
  for (hdr = RecToHdr(rec);
       ;
       hdr = nextHdr, rec = nextRec) {
    nextRec = rec + hdr->length;
    if (nextRec == CBIn)	// reached the end
      return NULL;
    if (nextRec == CBLast)
      nextRec = CBStart;	// wrap
    nextHdr = RecToHdr(nextRec);
    if (nextHdr->id > id) {
      cachedLocation.id = nextHdr->id;
      cachedLocation.addr = (uint8_t *)nextHdr;
      return (uint8_t *)nextHdr;
    }
  }
}

// Returns NULL if no next record.
uint8_t *
GetNext(capros_Logfile_RecordID id)
{
  if (! RecordsExist())
    return NULL;
  if (id == capros_Logfile_nullRecordID) {
    // Get oldest record.
    return CBOut;
  } else {
    // First check the cache:
    if (cachedLocation.id == id)
      return SequentialSearchForNext(id, cachedLocation.addr);

    int u = SearchID(id);
    if (u < 0)
      return CBOut;	// id is before the first index record
    return SequentialSearchForNext(id, GetIndexRecord(u)->addr);
  }
}

// Get the length of the previous log record, which must exist.
static inline unsigned long
GetPrevLogRecordLength(uint8_t * this)
{
  uint32_t * word = (uint32_t *)this;
  return *(word - 1);	// get length in trailer
}

// Locate the previous log record, which must exist.
static inline uint8_t *
GetPrevLogRecord(uint8_t * this)
{
  return this - GetPrevLogRecordLength(this);
}

// Search for the record with the largest RecordID less than id.
// On entry, rec->id >= id or rec is CBIn.
// Returns NULL if no such record.
uint8_t *
SequentialSearchForPrev(capros_Logfile_RecordID id, uint8_t * rec)
{
  uint8_t * nextRec;
  for ( ; ; rec = nextRec) {
    if (rec == CBOut)	// reached the beginning
      return NULL;
    if (rec == CBStart)
      rec = CBLast;	// wrap
    nextRec = GetPrevLogRecord(rec);
    capros_Logfile_recordHeader * hdr = RecToHdr(nextRec);
    if (hdr->id < id) {
      cachedLocation.id = hdr->id;
      cachedLocation.addr = (uint8_t *)hdr;
      return (uint8_t *)hdr;
    }
  }
}

// Returns NULL if no previous record.
uint8_t *
GetPrev(capros_Logfile_RecordID id)
{
  if (! RecordsExist())
    return NULL;
  if (id == capros_Logfile_nullRecordID) {
    // Get newest record.
    return GetPrevLogRecord(CBIn);
  } else {
    // First check the cache:
    if (cachedLocation.id == id)
      return SequentialSearchForPrev(id, cachedLocation.addr);

    int u = SearchID(id);
    if (u < 0)
      return NULL;	// id is before the first index record
    u++;	// we will search back from a higher record
    uint8_t * searchStart = 
      (u == numIndexRecords ? CBIn : GetIndexRecord(u)->addr);
    return SequentialSearchForPrev(id, searchStart);
  }
}

/***********************  Stuff for the waiter  **********************/

bool notifWaiter = false;
capros_Logfile_RecordID waiterID;

static void
CheckWaiter(void)
{
  assert(RecordsExist());
  if (notifWaiter && lastIDAdded > waiterID) {
    uint8_t * rec;
    rec = GetNext(waiterID);
    assert(rec);

    Message Msg = {
      .snd_invKey = KR_WAITER,
      .snd_key0 = KR_VOID,
      .snd_key1 = KR_VOID,
      .snd_key2 = KR_VOID,
      .snd_rsmkey = KR_VOID,
      .snd_data = rec,
      .snd_len = RecToHdr(rec)->length,
      .snd_code = RC_OK,
      .snd_w1 = 0,
      .snd_w2 = 0,
      .snd_w3 = 0
    };
    SEND(&Msg);
    notifWaiter = false;
  }
}

/***********************  Record addition  **********************/

void
AppendRecord(Message * msg)
{
  result_t result;

  unsigned long recordLength = msg->rcv_sent;
#if 0
  kprintf(KR_OSTREAM, "AppendRecord len=%d %d %d\n", recordLength,
          messageBuffer[0].length,
          *(unsigned long *)((uint8_t *)messageBuffer + recordLength
                               - sizeof(unsigned long)));
#endif
  if (recordLength > capros_Logfile_MaxRecordLength
      || recordLength < sizeof(capros_Logfile_recordHeader)
                        + sizeof(unsigned long)
      || recordLength % sizeof(capros_Logfile_RecordID)
      || recordLength != messageBuffer[0].length ) {
    msg->snd_code = RC_capros_key_RequestError;
    return;
  }

  // Ensure the trailer is correct:
  *(unsigned long *)
   ((uint8_t *)messageBuffer + recordLength - sizeof(unsigned long))
     = recordLength;

  capros_Logfile_RecordID id = messageBuffer[0].id;
  if (id <= lastIDAdded) {
    msg->snd_code = RC_capros_Logfile_OutOfSequence;
    return;
  }

  uint8_t * tentativeLoc = CBIn;	// where the new record will start
  if (CBOut > CBIn) {	// the log wraps around
    /* The + BPSIZE below ensures that CBIn and CBOut don't
    end up in the same block or page, which would be awkward for the index
    and for allocation. */
    if (recordLength + BPSIZE >= CBOut - CBIn) {
      msg->snd_code = RC_capros_Logfile_Full;
      return;
    }
    // else it will go at tentativeLoc
  } else {	// the log does not wrap around
    if (recordLength > CBEnd - CBIn) {	// does not fit at end
      /* Re the + BPSIZE below, see comment above. */
      if (recordLength + BPSIZE >= CBOut - CBStart) { // nor at beginning
        msg->snd_code = RC_capros_Logfile_Full;
        return;
      } else {		// wrap around
        CBLast = CBIn;
        tentativeLoc = CBStart;
      }
    }
    // else it will go at tentativeLoc
  }
  // The record fits and will go at tentativeLoc.

  // Check deletion policy now so we don't temporarily go over the limit.
  CheckDeletionBySpace(recordLength);

  // Ensure the log is allocated.
  uint32_t lowAlloc = RoundUpToPage((uint32_t)tentativeLoc);
  uint32_t highAlloc = RoundUpToPage((uint32_t)tentativeLoc + recordLength);
  result = EnsureRangeAllocated(lowAlloc, highAlloc);
  if (result != RC_OK) {
    msg->snd_code = result;
    return;
  }

  // Ensure the index records are allocated.
  struct IndexRecord * newIndexHi = LogToIndex(tentativeLoc) + 1;
  uint32_t oldRounded = RoundUpToPage((uint32_t)indexHi);
  uint32_t newRounded = RoundUpToPage((uint32_t)newIndexHi);
  if (indexHi > newIndexHi) {	// we begin to wrap
    result = EnsureRangeAllocated(oldRounded,
                                  RoundUpToPage((uint32_t)IndexEnd));
    if (result != RC_OK) {
      msg->snd_code = result;
      // Don't bother to deallocate space allocated for log above
      // or for previous index records.
      return;
    }
    result = EnsureRangeAllocated((uint32_t)IndexStart, newRounded);
    if (result != RC_OK) {
      msg->snd_code = result;
      // Don't bother to deallocate space allocated for log above
      // or for previous index records.
      return;
    }
  } else {
    result = EnsureRangeAllocated(oldRounded, newRounded);
    if (result != RC_OK) {
      msg->snd_code = result;
      // Don't bother to deallocate space allocated for log above
      // or for previous index records.
      return;
    }
  }

  // Whew! Everything is OK to add the record. Do it.
  memcpy(tentativeLoc, messageBuffer, recordLength);
  CBIn = tentativeLoc + recordLength;
  cachedLocation.id = id;
  cachedLocation.addr = tentativeLoc;

  // Now update the index:
  struct IndexRecord * ir;
  for (ir = indexHi; ir != newIndexHi; ++ir) {
    if (ir == IndexEnd)
      ir = IndexStart;	// wrap
    ir->id = id;	// index record refers to the new log record
    ir->addr = tentativeLoc;
    numIndexRecords++;
  }
  indexHi = ir;
  totalLogSpace += recordLength;
  lastIDAdded = id;
  CheckDeletionByID();
  CheckWaiter();
}

/***********************  Capability server  **********************/

// Destroy self:
void
Sepuku(uint32_t finalResult)
{
  // Free all the memory:
  EnsureRangeDeallocated((uint32_t)SpaceStart, (uint32_t)SpaceEnd);

  capros_Node_getSlotExtended(KR_CONSTIT, KC_INTERPRETERSPACE, KR_TEMP0);
  InterpreterDestroy(KR_TEMP0, KR_TEMP1, finalResult);
}

static inline bool
IsReadOnly(Message * msg)
{
  return msg->rcv_keyInfo & capros_Logfile_readOnly;
}

int
main(void)
{
  Message Msg = {
    .snd_invKey = KR_RETURN,
    .snd_key0 = KR_VOID,
    .snd_key1 = KR_VOID,
    .snd_key2 = KR_VOID,
    .snd_rsmkey = KR_VOID,
    .snd_len = 0,
    .snd_code = RC_OK,
    .snd_w1 = 0,
    .snd_w2 = 0,
    .snd_w3 = 0,

    .rcv_key0 = KR_VOID,
    .rcv_key1 = KR_VOID,
    .rcv_key2 = KR_VOID,
    .rcv_rsmkey = KR_RETURN,
    .rcv_data = &messageBuffer,
    .rcv_limit = capros_Logfile_MaxRecordLength
  };
  Message * msg = &Msg;

  capros_Node_getSlotExtended(KR_CONSTIT, KC_OSTREAM, KR_OSTREAM);

  capros_Process_getAddrSpace(KR_SELF, KR_MEMROOT);

  // Return start cap to this process:
  capros_Process_makeStartKey(KR_SELF, 0, KR_TEMP0);
  Msg.snd_key0 = KR_TEMP0;
  for (;;) {
    capros_Logfile_RecordID id;
    uint32_t maxLenToReceive;
    uint8_t * rec;

    RETURN(&Msg);

    DEBUG(server) kprintf(KR_OSTREAM, "logfile was called, OC=%#x\n",
                          Msg.rcv_code);

    // Defaults for reply:
    Msg.snd_invKey = KR_RETURN;
    Msg.snd_code = RC_OK;
    Msg.snd_w1 = 0;
    Msg.snd_w2 = 0;
    Msg.snd_w3 = 0;
    Msg.snd_key0 = KR_VOID;
    Msg.snd_len = 0;

    switch (Msg.rcv_code) {
    default:
      Msg.snd_code = RC_capros_key_UnknownRequest;
      break;

    case OC_capros_key_getType:
      Msg.snd_w1 = IKT_capros_Logfile;
      break;

    case OC_capros_key_destroy:
      if (IsReadOnly(&Msg)) {
        Msg.snd_code = RC_capros_key_NoAccess;
        break;
      }
      Sepuku(RC_OK);
      break;

    case OC_capros_Logfile_reduce:
      if (msg->rcv_w1 & ~(capros_Logfile_readOnly | capros_Logfile_noWait)) {
        Msg.snd_code = RC_capros_key_RequestError;
        break;
      }
      capros_Process_makeStartKey(KR_SELF,
               msg->rcv_keyInfo | msg->rcv_w1, KR_TEMP0);
      Msg.snd_key0 = KR_TEMP0;
      break;

    case OC_capros_Logfile_getReadOnlyCap:
      capros_Process_makeStartKey(KR_SELF, capros_Logfile_readOnly, KR_TEMP0);
      Msg.snd_key0 = KR_TEMP0;
      break;

    case 1:	// OC_capros_Logfile_appendRecord
      if (IsReadOnly(&Msg)) {
        Msg.snd_code = RC_capros_key_NoAccess;
        break;
      }
      AppendRecord(&Msg);
      break;

    case OC_capros_Logfile_deleteByID:
      if (IsReadOnly(&Msg)) {
        Msg.snd_code = RC_capros_key_NoAccess;
        break;
      }
      DeleteByID(Msg.rcv_w1 | ((uint64_t)Msg.rcv_w2 << 32));
      break;

    case 2:	// OC_capros_Logfile_getNextRecord
      id = Msg.rcv_w1 | ((uint64_t)Msg.rcv_w2 << 32);
      rec = GetNext(id);
    returnOneRecord:
      if (! rec)
        Msg.snd_code = RC_capros_Logfile_NoRecord;
      else {
        Msg.snd_data = rec;
        Msg.snd_len = RecToHdr(rec)->length;
      }
      break;

    case 6:	// OC_capros_Logfile_waitNextRecord
      if (msg->rcv_keyInfo & capros_Logfile_noWait) {
        Msg.snd_code = RC_capros_key_NoAccess;
        break;
      }
      if (notifWaiter) {
        Msg.snd_code = RC_capros_Logfile_Already;
        break;
      }
      id = Msg.rcv_w1 | ((uint64_t)Msg.rcv_w2 << 32);
      rec = GetNext(id);
      if (rec)
        goto returnOneRecord;
      // Must wait.
      notifWaiter = true;
      waiterID = id;
      COPY_KEYREG(KR_RETURN, KR_WAITER);	// save return cap
      Msg.snd_invKey = KR_VOID;
      break;

    case 3:	// OC_capros_Logfile_getPreviousRecord
      id = Msg.rcv_w1 | ((uint64_t)Msg.rcv_w2 << 32);
      rec = GetPrev(id);
      goto returnOneRecord;

    case 4:	// OC_capros_Logfile_getNextRecords
      id = Msg.rcv_w1 | ((uint64_t)Msg.rcv_w2 << 32);
      maxLenToReceive = Msg.rcv_w3;
      minEquals(maxLenToReceive, capros_key_messageLimit);
      rec = GetNext(id);
      if (! rec)
        Msg.snd_code = RC_capros_Logfile_NoRecord;
      else {
        uint8_t * curRec = rec;
        while (1) {
          uint32_t length = RecToHdr(curRec)->length;
          if (curRec + length - rec > maxLenToReceive)
            break;	// next record doesn't fit
          curRec += length;
          if (curRec == CBIn || curRec == CBLast)
            break;	// no more records, or would wrap
        }
        Msg.snd_data = rec;
        Msg.snd_len = curRec - rec;
      }
      break;

    case 5:	// OC_capros_Logfile_getPreviousRecords
      id = Msg.rcv_w1 | ((uint64_t)Msg.rcv_w2 << 32);
      maxLenToReceive = Msg.rcv_w3;
      minEquals(maxLenToReceive, capros_key_messageLimit);
      rec = GetPrev(id);
      if (! rec)
        Msg.snd_code = RC_capros_Logfile_NoRecord;
      else {
        rec += RecToHdr(rec)->length;	// end of the data we will return
        uint8_t * curRec = rec;
        while (1) {
          uint32_t length = GetPrevLogRecordLength(curRec);
          if (rec + length - curRec > maxLenToReceive)
            break;	// next record doesn't fit
          curRec -= length;
          if (curRec == CBOut || curRec == CBStart)
            break;	// no more records, or would wrap
        }
        Msg.snd_data = curRec;
        Msg.snd_len = rec - curRec;
      }
      break;

    case OC_capros_Logfile_setDeletionPolicyByID:
      if (IsReadOnly(&Msg)) {
        Msg.snd_code = RC_capros_key_NoAccess;
        break;
      }
      deletionPolicyAge = Msg.rcv_w1 | ((uint64_t)Msg.rcv_w2 << 32);
      CheckDeletionByID();
      break;

    case OC_capros_Logfile_setDeletionPolicyBySpace:
      if (IsReadOnly(&Msg)) {
        Msg.snd_code = RC_capros_key_NoAccess;
        break;
      }
      deletionPolicySize = Msg.rcv_w1;
      CheckDeletionBySpace(0);
      break;
    }
  }
}
