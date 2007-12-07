/*
 * Copyright (C) 2007, Strawberry Development Group.
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

#include <eros/Invoke.h>
#include <idl/capros/Sleep.h>

void mach_Delay(unsigned int n);

/* Delay for w microseconds. 
   w must be <= 32768 and calibrationConstant must be < (1UL << 17)
   (to avoid overflow in the multiplication). */
static void
delay(uint32_t w, uint32_t calibrationConstant)
{
  w *= calibrationConstant;
  w /= 8;
  if (w > 0)
    mach_Delay(w);
}

result_t
capros_Sleep_delayMicroseconds(cap_t _self,
  uint32_t microseconds, uint32_t calibrationConstant)
{
  if (microseconds > capros_Sleep_DelayMaxMicroseconds)
    return RC_capros_key_RequestError;

  if (calibrationConstant >= (1UL << 17))
    return RC_capros_key_RequestError;

  while (microseconds >= 32768) {
    delay(32768, calibrationConstant);
    microseconds -= 32768;
  }
  delay(microseconds, calibrationConstant);

  return RC_OK;
}
