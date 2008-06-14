/*
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

#include <string.h>
#include <kerninc/kernel.h>
#include <kerninc/Key.h>
#include <kerninc/Activity.h>
#include <kerninc/Invocation.h>
#include <kerninc/IORQ.h>
#include <kerninc/ObjectSource.h>
#include <eros/Invoke.h>
#include <idl/capros/key.h>
#include <eros/machine/IORQ.h>

#define dbg_wait	0x1
#define dbg_complete	0x2

/* Following should be an OR of some of the above */
#define dbg_flags   ( 0u | dbg_wait | dbg_complete )

#define DEBUG(x) if (dbg_##x & dbg_flags)

extern const struct ObjectSource IOObSource;

void
IORQKey(Invocation * inv)
{
  IORequest * ioreq;

  inv_GetReturnee(inv);

  uint32_t index = inv->key->u.nk.value[0];
  assert(index < KTUNE_NIORQS);
  IORQ * iorq = &IORQs[index];

  inv->exit.code = RC_OK;	// default

  switch (inv->entry.code) {
  case OC_capros_IOReqQ_registerOIDRange:
  case OC_capros_IOReqQ_registerLIDRange:
    COMMIT_POINT();

    if (inv->entry.len < sizeof(OID)) {
      inv->exit.code = RC_capros_key_RequestError;
    } else {
      ObjectRange rng;
      memcpy(&rng.end, inv->entry.data, sizeof(OID));
      rng.start = inv->entry.w1 | (((OID)inv->entry.w2) << 32);
      rng.u.rq.opaque = inv->entry.w3;
      rng.u.rq.iorq = iorq;
      rng.source = &IOObSource;
      if (inv->entry.code == OC_capros_IOReqQ_registerOIDRange)
        objC_AddRange(&rng);
      else
        AddLIDRange(&rng);
    }
    break;

  case OC_capros_IOReqQ_waitForRequest:
    DEBUG(wait) printf("IOReqQ wait\n");

    if (link_isSingleton(& iorq->lk)) {	// no requests
      act_SleepOn(&iorq->waiter);
      act_Yield();
    }

    proc_SetupExitString(inv->invokee, inv, sizeof(capros_IOReqQ_IORequest));

    COMMIT_POINT();

    ioreq = container_of(iorq->lk.prev, IORequest, lk);
    link_Unlink(&ioreq->lk);

    ObjectRange * rng = ioreq->objRange;

    capros_IOReqQ_IORequest capReq = {
      .rangeStartOID = rng->start,
      .rangeOpaque = rng->u.rq.opaque,
      .rangeLoc = ioreq->rangeLoc,
      .bufferDMAAddr = pageH_ToPhysAddr(ioreq->pageH),
      .requestType = ioreq->requestCode,
      // For the requestID just use the address of the IORequest.
      // This is admittedly not very robust.
      .requestID = (unsigned long)ioreq
    };
    inv_CopyOut(inv, sizeof(capros_IOReqQ_IORequest), &capReq);
    break;

  case OC_capros_IOReqQ_completeRequest:
    ioreq = (IORequest *)inv->entry.w1;
    unsigned long errno = inv->entry.w2;
    if (errno)
      printf("IOReqQ complete errno=%d!\n", errno);

    DEBUG(complete) printf("IOReqQ complete %#x\n", ioreq);

    // Call the done function:
    (*ioreq->doneFn)(ioreq);
    IOReq_Deallocate(ioreq);

    COMMIT_POINT();
    break;

  case OC_capros_IOReqQ_disableWaiting:
  case OC_capros_IOReqQ_enableWaiting:
    assert(!"complete");

    COMMIT_POINT();
    break;

  case OC_capros_key_destroy:
    COMMIT_POINT();

    // This is really "deallocate" not "destroy", since it doesn't
    // rescind any keys.
    IORQ_Deallocate(iorq);
    break;

  case OC_capros_key_getType:
    COMMIT_POINT();

    inv->exit.w1 = IKT_capros_IOReqQ;
    break;

  default:
    COMMIT_POINT();

    inv->exit.code = RC_capros_key_UnknownRequest;
    break;
  }
  ReturnMessage(inv);
}
