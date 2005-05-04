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

/* Implementation of the EROS request mechanism. */

#if 0
#include <kerninc/kernel.hxx>
#include <kerninc/MsgLog.hxx>
#include <kerninc/Check.hxx>
#include <kerninc/Thread.hxx>
#include <kerninc/Task.hxx>
#include <kerninc/ObjectCache.hxx>
#include <arch-kerninc/KernTune.hxx>
#include <kerninc/IoRequest.hxx>
#include <kerninc/BlockDev.hxx>

#include <arch-kerninc/PTE.hxx>

#endif

#include "BlockDev.hxx"
#include "IoRequest.hxx"
#include "KernTune.h"
#include "ObjectHeader.h"

/* #define REQUEST_DEBUG */

/* Want an even number so we can manage the DIO pool as a 2-set cache: */
#define IO_NDIO          (KTUNE_NBLKIO & ~0x1u)

/* KTUNE_NIOREQ is the number of per-device I/O request structures.
   An object I/O requires both a DIO and an IOReq structure.
   
   There must be AT LEAST as many I/O device request structures as
   there are core divisions.  The '+8' is to reduce contention */
#if ((IO_NDIO * KTUNE_MAXDUPLEX) < KTUNE_NCOREDIV)
#define IO_NIOREQ (KTUNE_NCOREDIV+8)
#else
#define IO_NIOREQ  (IO_NDIO * KTUNE_MAXDUPLEX)
#endif


//static ThreadPile reqWait;

/* Note that the DuplexedIoPool is managed as a 2-set cache. */
//static DuplexedIO DuplexedIoPool[IO_NDIO];
struct Request RequestPool[IO_NIOREQ];
static uint32_t nFreeReq = IO_NIOREQ;

const char *cmdNames[NUM_IOCMD] = {
  "Read",
  "Write",
  /*  "WriteVerify" */
  "Plug",
};

#define dio_ndx_check(oid) assert(((oid) % EROS_OBJECTS_PER_FRAME) == 0)
#define dio_ndx(oid) (((oid) / EROS_OBJECTS_PER_FRAME) % (IO_NDIO/2))
#define alt_dio_ndx(oid) (dio_ndx(oid) + (IO_NDIO/2))
  
#ifdef OPTION_DDB
extern void db_printf(const char *fmt, ...);

void
request_ddb_dump( struct Request *req )
{
    uint32_t i;

    for (i = 0; i < IO_NIOREQ; i++) {
	Request *ior = &RequestPool[i];
	if (ior->sleepQ == 0)
	    continue;

	db_printf("IOR %4d [0x%08x]: active? %c cmd=%s start %d nsec%d dio 0x%08x\n",
		  i, ior, ior->active ? 'y' : 'n',
		  ior->CmdName(), ior->req_start, ior->req_nsec,
		  ior->dio);
    }
}

void
duplexedIO_ddb_dump()
{
  for (uint32_t i = 0; i < IO_NDIO; i++) {
    DuplexedIO *dio = &DuplexedIoPool[i];
    if (dio->status == DuplexedIO_Free)
      continue;

    db_printf("DIO %4d [0x%08x]: status %d nReq %d pObHdr=0x%08x\n",
	      i, dio, dio->status, dio->nRequest, dio->pObHdr);
  }
}

void
duplexedIO_ddb_dump_hist()
{
  for (uint32_t i = 0; i < IO_NDIO; i++) {
    DuplexedIO *dio = &DuplexedIoPool[i];

    db_printf("DIO %4d [0x%08x]: use 0x%08x%08x\n",
	      i, dio,
	      (uint32_t) (dio->useCount>>32),
	      (uint32_t) (dio->useCount));
  }
}
#endif
#ifdef RMG
struct DuplexedIO*
suplexedIO_FindPendingIO(OID oid, ObType::Type ty)
{
  dio_ndx_check(oid);
  
  uint32_t ndx = dio_ndx(oid);
  uint32_t alt_ndx = alt_dio_ndx(oid);

  DuplexedIO& dio = DuplexedIoPool[ndx];
  DuplexedIO& alt_dio = DuplexedIoPool[alt_ndx];

  if (dio.status != Free && dio.oid == oid && dio.obType == ty)
    return &dio;
  else if (alt_dio.status != Free && alt_dio.oid == oid &&
	   alt_dio.obType == ty) 
    return &alt_dio;
  
  return 0;
}
#endif

#ifdef RMG
/* DuplexedIO::Grab(): Grab a duplexed IO structure for use in I/O on
 * the specified OID.  This MUST fail if there is already an active
 * DIO with the same OID.
 */
struct DuplexedIO*
duplexedIO_Grab(OID oid, IoCmd::Type ioCmd)
{
  dio_ndx_check(oid);
  
  uint32_t ndx = dio_ndx(oid);
  uint32_t alt_ndx = alt_dio_ndx(oid);
  
  DuplexedIO& dio; //= DuplexedIoPool[ndx];
  DuplexedIO& alt_dio; // = DuplexedIoPool[alt_ndx];

  for (;;) {
    if (dio.status == Free) {
      if (alt_dio.status == Free || alt_dio.oid != oid) {
	dio.status = Pending;
	dio.isObjectRead = false;
	dio.completionCallBack = 0;
	dio.oid = oid;
	dio.pObHdr = 0;
	dio.cmd = ioCmd;
	/* Setting this initially to 1 keeps the DIO from evaporating
	 * while we are still setting it up.
	 */
	dio.nRequest = 1;

	dio.useCount++;
	
	return &dio;
      }
    }
    else if (alt_dio.status == Free) {
      /* Already know dio not free */
      if (dio.oid != oid) {
	alt_dio.status = Pending;
	alt_dio.isObjectRead = false;
	alt_dio.completionCallBack = 0;
	alt_dio.oid = oid;
	alt_dio.pObHdr = 0;
	alt_dio.cmd = ioCmd;
	/* Setting this initially to 1 keeps the DIO from evaporating
	 * while we are still setting it up.
	 */
	alt_dio.nRequest = 1;

	alt_dio.useCount++;

	return &alt_dio;
      }
    }

#ifdef REQUEST_DEBUG
    kprintf( KR_OSTREAM,  "Blocking for DIO=0x%08x structure\n", &dio);
#endif

    //    if ( Thread::Current()->IsUser() ) {
      if (nFreeReq >= KTUNE_MAXDUPLEX*4)
	  {
	      //reqWait.WakeAll();
	  }
      //    }

#ifdef REQUEST_DEBUG
    kprintf( KR_OSTREAM, "@%02d", ndx);
#endif
    Thread::Current()->SleepOn(dio.stallQ);
    Thread::Current()->Yield();
  }

#ifdef REQUEST_DEBUG
  kprintf( KR_OSTREAM, "DIO structure 0x%08x acquired\n", &dio);
#endif
  return 0;
}
#endif



void
duplexedIO_WakeDioSiblings(struct DuplexedIO *dio)
{
#ifdef RMG
    uint32_t ndx = 0; //(dio - DuplexedIoPool) % (IO_NDIO/2);

  while (ndx < IO_NDIO) {
    ThreadPile& stallQ = DuplexedIoPool[ndx].stallQ;
    if (stallQ.IsEmpty() == false) {
      stallQ.WakeAll();
#ifdef REQUEST_DEBUG
      kprintf( KR_OSTREAM, "Waking DIO=0x%08x sleepers\n", dio);
#endif
    }

    ndx += (IO_NDIO/2);
  }
#endif
}



void
duplexedIO_Release( struct DuplexedIO *dio)
{
    dio->nRequest--;
    if (dio->nRequest == 0) {
	if (dio->status != Completed)
	    duplexIO_CompleteIO();
	dio->status = Free;

	/* If this was a read request, it may have completed eagerly, in
	 * which case other threads may have gone to sleep because the DIO
	 * structure wasn't free yet.  In that event, the DIO stall queue
	 * will not be empty at this point, and we should wake everyone on
	 * it:
	 */
	WakeDioSiblings( dio );
#ifdef REQUEST_DEBUG
	kprintf( KR_OSTREAM, "DIO for oid ");
	kprint( KR_OSTREAM, "%d", oid);
	kprintf( KR_OSTREAM, " releases\n");
#endif
    }
    else
	BlockDev::ActivateTask();
  
#ifdef REQUEST_DEBUG
    kprintf( KR_OSTREAM, "DIO structure 0x%08x released\n", dio );
#endif
}


#ifdef RMG
void
duplexedIO_AddRequest(struct DuplexedIO *dio, Request* pReq)
{
  pReq->dio = dio;
  pReq->sleepQ = &stallQ;
  nRequest++;
#ifdef REQUEST_DEBUG
  kprintf( KR_OSTREAM,  "DIO structure 0x%08x count increment\n", dio);
#endif
}
#endif

#if 0
void *
DuplexedIO::operator new(size_t /* sz */)
{
  for (uint32_t i = 0; i < KernTune::NumDIO; i++) {
    if (DuplexedIoPool[i].status == Free) {
      DuplexedIoPool[i].status = Init;
      return &DuplexedIoPool[i];
    }
  }

  // assert(false);
  return 0;
}

void
DuplexedIO::operator delete(void * vp)
{
  DuplexedIO *pd = (DuplexedIO*) vp;

  pd->status = Free;
}
#endif

void *
Request::operator new(size_t /* sz */)
{
    //  assert(nFreeReq);
  for (uint32_t i = 0; i < IO_NDIO; i++) {
      //    if (RequestPool[i].sleepQ == 0) {
      //      nFreeReq--;
      //      return &RequestPool[i];
      //    }
  }

  //MsgLog::fatal("Ran out of I/O requests!\n");
  return 0;
}

void
Request::operator delete(void *vp)
{
    //  Request *pReq = (Request *) vp;

  //  pReq->sleepQ = 0;
  nFreeReq++;

#if 0
  // assert (KernTune::MaxDuplex*4 < IO_NDIO);
#endif
  if (nFreeReq >= KTUNE_MAXDUPLEX*4)
      {
	  //  reqWait.WakeAll();
      }
}

void
request_Require(uint32_t nReqs)
{
  if (nFreeReq < nReqs) {
      //    Thread::Current()->SleepOn(reqWait);
      //    Thread::Current()->Yield();
  }
  
  return;
}

request_init( struct Request *req, uint8_t _unit, uint8_t theCmd, uint32_t startSec, uint32_t nSec)
{
    req->unit = _unit;
    //  req->sleepQ = &rawStallQ;
    req->req_nsec = nSec;
    req->req_start = startSec;
    req->dio = 0;
    req->cmd = theCmd;
    req->next = 0;
    req->nError = 0;
    req->active= false;
    req->inProgress = false;
    req->completionCallBack = 0;
}

//Request::~Request()
//{
    //  assert (dio == 0);
//}

bool
request_IsCompleted( struct Request *req)
{
  if (req->dio && req->dio->status == DuplexedIO_Completed)
    return true;

  if (req->req_nsec == 0)
    return true;
  
  return false;
}

bool
request_Commit(struct Request *req, BlockDev *ctrlr, void *activeDev)
{
#ifdef REQUEST_DEBUG
  kprintf( KR_OSTREAM, "Request::Commit this=0x%08x, ctrlr=0x%08x\n", req, ctrlr);
#endif
  if (req->dio == 0) {
      req->active = true;
  }
  else {
      req->active = duplexedIO_commitRequest(dio, ctrlr, activeDev);
      req->req_ioaddr = dio->ioaddr;
  }

  return req->active;
}

void
request_Finish( struct Request *req )
{
#ifdef REQUEST_DEBUG
  MsgLog::dprintf(true, "Completing request...\n");
#endif
  
  if (completionCallBack)
    completionCallBack( req);
     
  if (req->dio) {
      duplexedIO_FinishRequest(req->dio, (req->cmd == Read_Cmd));
      duplexedIO_Release( req->dio );
      req->dio = 0x0;			/* for paranoia! */
  }
  else {
#ifdef REQUEST_DEBUG
      kprintf( KR_OSTREAM, "  Request was raw.\n");
#endif
    //    rawStallQ.WakeAll();
  }

  req->active = false;
}

/* There is a policy question in deciding what to do when the max
 * error count is exceeded!
 */
void
request_Terminate()
{
    //  MsgLog::fatal("Unimplemented Request::Terminate() called\n");
}

void
duplexedIO_AllocateDeferredFrame( struct DuplexedIO *dio )
{
#ifdef REQUEST_DEBUG
  kprintf( KR_OSTREAM, "Allocating deferred frame oid ");
  MsgLog::print(oid);
  kprintf( KR_OSTREAM, " ty %d...", obType);
#endif
  pObHdr = ObjectCache::IoCommitGrabPageFrame();

  pObHdr->SetFlags(OFLG_IO);
  pObHdr->ob.ioCount = 1;

  ioaddr = ObjectCache_ObHdrToPage(pObHdr);

#ifdef REQUEST_DEBUG
  kprintf( KR_OSTREAM, " done\n");
#endif
}

bool
DuplexedIO::CommitRequest(BlockDev *ctrlr, void* activeDev)
{
    if (cmd == IoCmd::Read) // && !pObHdr)
      AllocateDeferredFrame();
  
  if (cmd != IoCmd::Plug) {
      //    assert(pObHdr);
    
      //    if (pObHdr->ob.ioCount != 1 && status != Completed)
      //	{
	    //	    MsgLog::fatal("pObHdr 0x%08x has bad ioCount %d\n",
	    //		    pObHdr, pObHdr->ob.ioCount);
      //	}
    //    assert(pObHdr->ob.ioCount == 1 || status == Completed);
  }

  if (status == Completed)
    return true;

  if (status == Pending) {
    status = Active;
    activeDevice = activeDev;
#ifdef REQUEST_DEBUG
    kprintf( KR_OSTREAM, " Status went pending->active\n");
#endif
    return true;
  }

  //  assert (status == Active);
#ifdef REQUEST_DEBUG
  kprintf( KR_OSTREAM, " Status was active\n");
#endif

  if (activeDev == activeDevice)
    return true;

  ctrlr->next = dplxWait;
  ctrlr->inDuplexWait = true;
  dplxWait = ctrlr;

#if 0
  MsgLog::dprintf(false, "Ctrlr 0x%08x stalled by duplexing\n", ctrlr);
#endif
  return false;
}

void
duplexedIO_DuplexWakeup( struct DuplexedIO *dio )
{
  while (dplxWait) {
    BlockDev *bd = dplxWait;
    dplxWait = bd->next;
    bd->inDuplexWait = false;
    bd->next = BlockDev::readyChain;
    BlockDev::readyChain = bd;
#if 0
    MsgLog::dprintf(false, "DuplexWakeup: Ctrlr 0x%08x placed on ready chain\n", bd);
#endif
  }

  BlockDev::ActivateTask();
}

void
duplexedIO_CompleteIO( struct DuplexedIO *dio )
{
    //  assert(cmd == IoCmd::Plug || pObHdr);

    {    //  if (pObHdr) {
#ifdef REQUEST_DEBUG
    kprintf( KR_OSTREAM, "  Waking up request sleepers - q=0x%08x.\n", &stallQ);
#endif
    pObHdr->ob.ioCount--;

    if (pObHdr->ob.ioCount != 0)
	kprintf( KR_OSTREAM, "pObHdr 0x%08x has bad ioCount %d\n",
		 pObHdr, pObHdr->ob.ioCount);
  
        // assert(pObHdr->ob.ioCount == 0);
  
#ifdef OPTION_OB_MOD_CHECK
    /* Recomputing does no harm even if the object is dirty, and is
     * necessary if the object is clean:
     */
    pObHdr->ob.check = pObHdr->CalcCheck();
#endif
  
        if (pObHdr->GetFlags(OFLG_REDIRTY))
    	{
	    	    // assert(pObHdr->GetFlags(OFLG_DIRTY));
    	}
        else {
	      // assert ( PTE::ObIsNotWritable(pObHdr) );
          pObHdr->ClearFlags(OFLG_DIRTY);
#ifdef DBG_CLEAN
          kprintf( KR_OSTREAM, "Object 0x%08x ty %d oid=0x%08x%08x cleaned\n",
    		     pObHdr,
    		     pObHdr->obType,
    		     (uint32_t) (pObHdr->oid >> 32),
    		     (uint32_t) pObHdr->oid);
#endif
        }

        pObHdr->ClearFlags(OFLG_IO|OFLG_REDIRTY);

#if 0
    /* Preserve the checkpoint bit as long as possible.  If this was a
     * new object allocation, it will have been cleared in the object
     * cache logic.
     */
    pObHdr->flags.ckpt = 0;	/* NA after pageout or on clean object */
#endif

    if (isObjectRead) {
	      // assert (pObHdr->obType == ObType::PtDriverPage);
      /* This was an incoming object.  Set up the bits and intern it: */

	      pObHdr->SetFlags(OFLG_CURRENT|OFLG_DISKCAPS);
#ifdef OFLG_PIN
	      pObHdr->ClearFlags(OFLG_PIN);
#endif
	      pObHdr->products = 0;

	      pObHdr->obType = obType;
	      pObHdr->ob.oid = oid;
	      pObHdr->ob.allocCount = allocCount;
	      pObHdr->kr.ResetRing();
	      pObHdr->Intern();

#ifdef REQUEST_DEBUG
      kprintf( KR_OSTREAM, "Virgin object hdr=0x%08x oid ", pObHdr);
      MsgLog::print(oid);
      kprintf( KR_OSTREAM, " type %d read\n", obType);
#endif
    }
  }

#if defined(REQUEST_DEBUG)
  MsgLog::dprintf(true, "  Awaken sleepers on dio=0x%08x.\n", dio);
#endif

  /* Wake up the IO sleepers: */
#ifdef REQUEST_DEBUG
  stallQ.WakeAll(0, true);
#else
    stallQ.WakeAll(0, true);
#endif
  /* Wake up the hazard sleepers: */
    if (pObHdr)
      pObHdr->ObjectSleepQueue().WakeAll();

  if (completionCallBack)
    completionCallBack( dio );

  status = Completed;
  activeDevice = 0;

#if 0
  if (isPlug == false && pObHdr->flags.dirty)
    MsgLog::dprintf(true, "Object ty %d oid 0x%08x%08x was redirtied\n",
		    pObHdr->obType,
		    (uint32_t) (pObHdr->oid >> 32),
		    (uint32_t) (pObHdr->oid));
#endif

#ifdef DBG_WILD_PTR
  if (dbg_wild_ptr)
    Check::Consistency("Top GetObjectPot()");
#endif
}

void
duplexedIO_FinishRequest( struct DuplexedIO *dio, bool completed)
{
      // assert(status == Active || status == Completed);
	 
  if (completed && dio->status != Completed) {
#ifdef REQUEST_DEBUG
    kprintf( KR_OSTREAM, "Request completes early\n");
#endif
    duplexIO_CompleteIO( dio );
  }
  else if (dio->status != Completed) {
#ifdef REQUEST_DEBUG
    kprintf( KR_OSTREAM, "Revert to pending\n");
#endif
    dio->status = Pending;
    dio->activeDevice = 0;
  }
  
  duplexedIO_DuplexWakeup();
}


