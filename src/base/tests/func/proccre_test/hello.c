/*
 * Copyright (C) 1998, 1999, 2001, Jonathan S. Shapiro.
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


#include <eros/target.h>
#include <eros/Invoke.h>
#include <domain/domdbg.h>

#define KR_VOID 0
#define KR_OSTREAM    5

#define KR_ARG0    12
#define KR_ARG1    13
#define KR_ARG2    14
#define KR_RESUME  15

const uint32_t __rt_stack_pages = 1;
const uint32_t __rt_stack_pointer = 0x20000;

int
main()
{
  Message msg;

  msg.rcv_key0 = KR_ARG0;
  msg.rcv_key1 = KR_ARG1;
  msg.rcv_key2 = KR_ARG2;
  msg.rcv_rsmkey = KR_RESUME;
  msg.rcv_limit = 0;
  msg.snd_key0 = KR_VOID;
  msg.snd_key1 = KR_VOID;
  msg.snd_key2 = KR_VOID;
  msg.snd_rsmkey = KR_VOID;
  msg.snd_len = 0;
  msg.snd_invKey = KR_VOID;
  msg.snd_code = RC_OK;
  msg.snd_w1 = 0;
  msg.snd_w2 = 0;
  msg.snd_w3 = 0;

  RETURN(&msg);
  
  kprintf(KR_OSTREAM, "hello, world\n");
  kprintf(KR_OSTREAM, "\n\nIn keeping with tradition, note punctuation and case :-)\n");

  msg.snd_invKey = KR_RESUME;
  msg.snd_code = RC_OK;
  RETURN(&msg);
  return 0;
}
