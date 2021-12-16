/*
 * Copyright (C) 2010, Strawberry Development Group.
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
 * Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include <eros/target.h>
#include <eros/Invoke.h>
#include <idl/capros/RTC.h>

/* This is not implemented in the kernel, because it is difficult to
 * implement correctly if the invocation is a SEND or RETURN. */
result_t
capros_RTC_sleepTillTime(cap_t _self, capros_RTC_time_t wakeupTime)
{
  result_t result;
  do {
    result = capros_RTC_sleepTillTimeOrRestart(_self, wakeupTime);
  } while (result == RC_capros_key_Restart);
  return result;
}
