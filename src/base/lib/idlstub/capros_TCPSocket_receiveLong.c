/*
 * Copyright (C) 2009, Strawberry Development Group.
 *
 * This file is part of the CapROS Operating System runtime library.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, 59 Temple Place - Suite 330 Boston, MA 02111-1307, USA.
 */
/* This material is based upon work supported by the US Defense Advanced
Research Projects Agency under Contract No. W31P4Q-07-C-0070.
Approved for public release, distribution unlimited. */

#include <eros/target.h>
#include <eros/Invoke.h>
#include <idl/capros/TCPSocket.h>
#include <domain/domdbg.h>////

result_t
capros_TCPSocket_receiveLong(uint32_t _self, uint32_t length,
  uint32_t * lengthRead,
  uint8_t * flags,
  uint8_t * data)
{
  result_t result;
  uint8_t myFlags;
  uint32_t resid = length;

  while (resid) {
    uint32_t rqLen = resid;
    if (rqLen > capros_TCPSocket_maxReceiveLength)
      rqLen = capros_TCPSocket_maxReceiveLength;

kprintf(7,"TCPSocket_receiveLong: want %d have %d receiving %d to %#x\n",
        length, length - resid, rqLen, data);////
    uint32_t thisLengthRead;
    result = capros_TCPSocket_receive(_self,
               rqLen, &thisLengthRead, &myFlags, data);
    if (result != RC_OK) {
kprintf(7,"TCPSocket_receiveLong: rc=%#x\n", result);////
      if (length - resid == 0)	// haven't gotten any data
        return result;
      break;		// return the data we have and ignore result.
    }

kprintf(7,"TCPSocket_receiveLong: read=%d flg=%#x\n",
  thisLengthRead, myFlags, data[0],data[1],data[thisLengthRead-2],data[thisLengthRead-1]);////
    if (thisLengthRead == 0)	// can this happen?
      break;	// couldn't read anything
    resid -= thisLengthRead;
    data += thisLengthRead;
  }

  if (flags)
    *flags = myFlags;
  if (lengthRead)
    *lengthRead = length - resid;
  return RC_OK;
}
