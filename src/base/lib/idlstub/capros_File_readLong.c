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
#include <idl/capros/File.h>

result_t
capros_File_readLong(uint32_t _self, uint64_t at, uint32_t length,
	   uint8_t * data, uint32_t * lengthRead)
{
  result_t result;
  uint32_t resid = length;

  while (resid) {
    uint32_t rqLen = resid;
    if (rqLen > capros_File_maxLength)
      rqLen = capros_File_maxLength;
    
    uint32_t thisLengthRead;
    result = capros_File_read(_self, at, rqLen, data, &thisLengthRead);
    if (result != RC_OK) {
      if (length - resid == 0)	// haven't gotten any data
        return result;
      break;		// return the data we have and ignore result.
    }

    resid -= thisLengthRead;
    if (thisLengthRead < rqLen)
      break;	// read all we could
    data += thisLengthRead;
    at += thisLengthRead;
  }

  *lengthRead = length - resid;
  return RC_OK;
}
