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

#if 0
#include <eros/target.h>
#include <eros/Invoke.h>
#include <eros/KeyBitsKey.h>

int
GetKeyBits(uint32_t kbr, uint32_t kr, uint32_t *version, Bits *bits)
{
  int i;
  struct kbinfo {
    uint32_t ver;
    uint32_t valid;
    Bits bits;
  } kbi;

  Message msg;

  msg.snd_w1 = 0;
  msg.snd_w2 = 0;
  msg.snd_w3 = 0;
  
  msg.snd_key0 = kr;
  msg.snd_key1 = KR_VOID;
  msg.snd_key2 = KR_VOID;
  msg.snd_rsmkey = KR_VOID;

  msg.rcv_key0 = KR_VOID;
  msg.rcv_key1 = KR_VOID;
  msg.rcv_key2 = KR_VOID;
  msg.rcv_rsmkey = KR_VOID;

  msg.rcv_data = &kbi;
  msg.rcv_limit = sizeof(kbi);

  msg.snd_len = 0;		/* no data returned */

  msg.snd_invKey = kbr;
  msg.snd_code = OC_KeyBits_Translate;

  (void) CALL(&msg);
  
  if (version) *version = kbi.ver;
  if (bits) 
    for(i = 0; i < 4; i++)
      bits->w[i] = kbi.bits.w[i];

  return kbi.valid;
}
#endif
