/*
 * Copyright (C) 1998, 1999, Jonathan S. Shapiro.
 *
 * This file is part of the EROS Operating System runtime library.
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

#include <eros/target.h>
#include <eros/Invoke.h>
#include <eros/DevCreKey.h>

uint32_t
devcre_make_instance(uint32_t krDevCre, uint32_t ndx, uint32_t krDev)
{
  Message msg;

  msg.snd_w1 = ndx;
  msg.snd_w2 = 0;
  msg.snd_w3 = 0;
  
  msg.snd_key0=KR_VOID;
  msg.snd_key1=KR_VOID;
  msg.snd_key2=KR_VOID;
  msg.snd_rsmkey=KR_VOID;
  msg.snd_len=0;

  msg.rcv_key0=krDev;
  msg.rcv_key1=KR_VOID;
  msg.rcv_key2=KR_VOID;
  msg.rcv_rsmkey=KR_VOID;
  msg.rcv_limit=0;

  msg.snd_invKey=krDevCre;
  msg.snd_code=OC_DevCre_CreateInstanceKey;

  return CALL(&msg);
}
