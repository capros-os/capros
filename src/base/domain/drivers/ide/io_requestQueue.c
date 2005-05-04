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

#if 0
#include <kerninc/kernel.hxx>
#include <kerninc/MsgLog.hxx>
#include <kerninc/IRQ.hxx>
#include <kerninc/IoRequest.hxx>
#endif

#include "IoRequest.hxx"

/* #define REQUEST_DEBUG */

void
requestQueue_InsertRequest(struct RequestQueue *rq, struct Request* req)
{
    struct Request *pos;
    //  IRQ::DISABLE();

    /* Insert on the basis of true start sector, NOT on the basis of
     * current start sector.
     */
    
    if (rq->opq == 0 || req->req_start < rq->opq->req_start) {
	//    assert (insertBase == 0);		/* if no opq, no insertBase */
	req->next = rq->opq;
	rq->opq = req;
    }
    else if (req->cmd == Plug_Cmd) {
	/* Always goes at the end of the queue. */
	for ( pos = (rq->insertBase ? rq->insertBase : rq->opq); ; pos = pos->next) {
	    struct Request *next = pos->next;

	    if (next == 0) {
		pos->next = req;
		req->next = next;
		rq->insertBase = req;
		break;
	    }
	}

    }
    else {
	for ( pos = (rq->insertBase ? rq->insertBase : rq->opq); ; pos = pos->next) {
	    struct Request *next = pos->next;

	    if (next == 0 || req->req_start < next->req_start) {
		req->next = next;
		pos->next = req;
		break;
	    }
	}
    }
    
    //  IRQ::ENABLE();
}

void
requestQueue_RemoveRequest(struct RequestQueue *rq, struct Request* req)
{
    //  IRQ::DISABLE();
  
    //  assert(opq);
  
#ifdef REQUEST_DEBUG
  kprintf(KR_OSTREAM, "request 0x%08x is deleted\n", req);
#endif
  
  if (rq->curReq == req)
      rq->curReq = req->next;

  /* If current top insertBase, forget top insertBase */
  if (rq->insertBase == req)
    rq->insertBase = 0;

  if (rq->opq == req) {
      rq->opq = rq->opq->next;
  }
  else {
    struct Request *pos = rq->opq;
    while (pos->next != req)
      pos = pos->next;
    pos->next = req->next;
  }

  //delete req; WE KNOW THIS WON"T WORK...hmmm
    
  //  IRQ::ENABLE();
}

#ifndef NDEBUG
bool
requestQueue_ContainsRequest(struct RequestQueue *rq, struct Request* req)
{
  bool result = false;
  struct Request *pos = rq->opq;
  
  //  IRQ::DISABLE();
  
  while (pos && pos != req)
    pos = pos->next;

  if (pos == req)
    result = true;
  
  //  IRQ::ENABLE();

  return result;
}
#endif

struct Request *
requestQueue_GetNextRequest( struct RequestQueue *rq)
{
    if (rq->curReq == 0)
	rq->curReq = rq->opq;

    if ( ( rq->curReq->cmd == Plug_Cmd ) && ( rq->curReq != rq->opq ) )
	rq->curReq = rq->opq;
  
    return rq->curReq;
}
