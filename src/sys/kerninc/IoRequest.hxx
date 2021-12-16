#ifndef __REQUEST_HXX__
#define __REQUEST_HXX__
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

#include <kerninc/Thread.hxx>

struct DuplexedIO;
struct BlockDev;
/* struct ObjectHeader; */

struct IoCmd {
  enum Type {
    Read,
    Write,
#if 0
    WriteVerify,	/* for now */
#endif
    /* Any command above these is a unit-specific command.  Some
     * drivers use the request structure to handle such things.
     */
    Plug,
  } ;

  enum { NUM_IOCMD = Plug + 1 };
  
protected:
  static const char *cmdNames[NUM_IOCMD];

public:
  static INLINE const char *CmdName(uint8_t cmd)
  { return cmdNames[cmd]; }
} ;

/* Note that this version does EAGER allocation of core frames!! */
struct Request {
  ThreadPile    rawStallQ;		/* used only for raw IO */
  ThreadPile    *sleepQ;
  
public:

  bool		active;		/* request is active (i.e. committed) */
  bool		inProgress;	/* an interrupt is pending on this request. */
  
  uint8_t	unit;		/* unit on which request should occur */
  uint8_t       cmd;

  /* These describe the residual -- the part of the request remaining
   * to be done.  They are initially the entire request.  While a
   * request is in progress, these describe the state prior to
   * initiation, and nSec describes the number of sectors the current
   * phase is attempting to transfer.
   */
  kva_t		req_ioaddr;	/* base address */
  uint32_t	req_start;	/* starting sector number */
  uint32_t	req_nsec;	/* total number of sectors */
  uint32_t	nsec;
  
  uint32_t      nError;		/* number of errors on this request */
  
  void (*completionCallBack)(Request *);
  
  DuplexedIO    *dio;		/* coordination structure */


  Request	*next;
  
public:
  Request()		/* initial array declarations only */
  {
    sleepQ = 0;
    next = 0;
  }
  
  Request(uint8_t unit, uint8_t cmd, uint32_t startSec, uint32_t nSecs);
  ~Request();

  INLINE const char *CmdName()
  {
    return IoCmd::CmdName(cmd);
  }
  
  static void Init();
  bool IsCompleted();

  /* For serialization of sibling requests: */
  bool Commit(BlockDev *, void *activeDev);
  void Finish();

#if 0
  void Cancel()
  {
    req_nsec = 0;
  }
#endif

  void *operator new(size_t);
  void operator delete(void *);

  void Terminate();
  
  static void Require(uint32_t count);
#ifdef OPTION_DDB
  static void ddb_dump();
#endif
};

/* Coordination structure for duplexed I/O requests: */

struct DuplexedIO {
protected:
  void CompleteIO();
  void AllocateDeferredFrame();
  
  static void WakeDioSiblings(DuplexedIO*);
public:
  ThreadPile  stallQ;		/* Waiting for avail struct or I/O
		 */
				/* completion
				 */
  
  enum Status {
    Free,			/* unallocated DIO structure */
    Pending,			/* not yet started */
    Active,			/* at least one unit is working on
			 */
				/* this IoRequest
				 */
    Completed,			/* IoRequest completed */
  } status;

  /* Since requests can be split, it is necessary to recall which
   * DEVICE has committed this request so that it can commit it again
   * for the 2nd..nth chunks.  We need not go finer grain than the
   * device, because a device processes only one request at a time.
   * 
   * It is entirely up to the controller what device pointer to hand
   * us -- single channel controllers might just hand us the
   * controller pointer.  We simply equality test this in the Commit()
   * routine.
   */
  void *activeDevice;
  
  uint32_t nRequest;		/* number of associated requests */

  bool          isObjectRead;
  
  uint8_t          cmd;		/* duplicate of per-request cmd */
  ObjectHeader *pObHdr;
  ObType obType;		/* object type we are fetching */
  OID oid;			/* object oid */
  ObCount allocCount;		/* object allocation count */
  kva_t ioaddr;
  void (*completionCallBack)(DuplexedIO *);

  DuplexedIO()
  {
    nRequest = 0;
    status = Free;
  }
  
  static DuplexedIO *FindPendingIO(OID oid, ObType obType);

  INLINE const char *CmdName()
  {
    return IoCmd::CmdName(cmd);
  }
  
  static DuplexedIO* Grab(OID, IoCmd::Type);
  void Release();

  void AddRequest(Request* pReq);

  bool CommitRequest(BlockDev *ctrlr, void *activeDev);
  /* return true if this call completes the I/O: */
  void FinishRequest(bool completed);
  
#ifdef OPTION_DDB
  static void ddb_dump();
  static void ddb_dump_hist();
#endif

  BlockDev *dplxWait;		/* duplex stall chain */
  void DuplexWakeup();

#ifdef OPTION_KERN_STATS
  uint64_t  useCount;		/* for histograms */
#endif
};

struct RequestQueue {
  void InsertRequest(Request*);
  void RemoveRequest(Request*);
#ifndef NDEBUG
  bool ContainsRequest(Request*);
#endif

  /* OPERATION QUEUE: */
  
  Request*	curReq;		/* current request */
  Request*	opq;		/* queue of pending operations */
  Request*	insertBase;	/* I/O sequencing support */

  bool IsEmpty()
  {
    return (opq == 0);
  }
  
  Request* GetNextRequest();
  
  RequestQueue()
  {
    curReq = 0;
    opq = 0;
    insertBase = 0;
  }
};

#endif /* __REQUEST_HXX__ */
