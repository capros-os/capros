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

/* Mutex Object
 
   The mutex object is intended to serve as a backing mutex for a
   shared-memory based semaphore.  The mutex object is intended to
   resolve serialization in the exceptional case where the lock was
   already held at the time the caller wanted it.  The usual strategy
   is to have the clients share a single word of memory containing a
   client count, and to implement a fast mutex as follows:

   Note that the DESTROY operation doesn't work right.

   fetch current lock COUNT
   increment it in local register
   compare-and-swap
   if old count was zero, you have the lock,
   else call 'grab expensive lock'
        store resulting resume cap

   *** protected processing ***

   fetch current lock COUNT
   atomic decrement lock COUNT
   if old count != 1 somebody is sleeping, so call RELEASE
     on the semaphore.

     */

#include <eros/target.h>
#include <eros/Invoke.h>
#include <eros/ProcessKey.h>
#include <domain/Runtime.h>

/*
 * Key Registers:
 *
 * KR15: arg3  (resume cap)
 *
 * Initial stack pointer at 0x4096
 * Accepts no data
 */

#define KR_START        KR_APP(0)

int
ProcessRequest(Message *msg)
{
  /* THIS NEEDS TO BE REPAIRED */
  Message lockReply;

  lockReply.snd_len = 0;
  lockReply.snd_key0 = KR_VOID;
  lockReply.snd_key1 = KR_VOID;
  lockReply.snd_key2 = KR_VOID;
  lockReply.snd_rsmkey = KR_VOID;
  lockReply.snd_code = RC_OK;
  lockReply.snd_invKey = KR_RETURN;
  
  lockReply.rcv_limit = 0;
  lockReply.rcv_key0 = KR_VOID;
  lockReply.rcv_key1 = KR_VOID;
  lockReply.rcv_key2 = KR_VOID;
  lockReply.rcv_rsmkey = KR_RETURN;

  CALL(&lockReply);
  return 1;
}

int
main()
{
  Message msg;

  /* could sell our constituents node back to the bank here, but we
     will eventually want to be able to call destroy on this, so hold
     off. */
  process_make_start_key(KR_SELF, 0, KR_START);
  
  msg.snd_invKey = KR_RETURN;
  msg.snd_key0 = KR_START;
  msg.snd_key1 = KR_VOID;
  msg.snd_key2 = KR_VOID;
  msg.snd_rsmkey = KR_VOID;
  msg.snd_data = 0;
  msg.snd_len = 0;
  msg.snd_code = 0;
  msg.snd_w1 = 0;
  msg.snd_w2 = 0;
  msg.snd_w3 = 0;

  msg.rcv_key0 = KR_VOID;
  msg.rcv_key1 = KR_VOID;
  msg.rcv_key2 = KR_VOID;
  msg.rcv_rsmkey = KR_RETURN;
  msg.rcv_data = 0;
  msg.rcv_limit = 0;
  msg.rcv_code = 0;
  msg.rcv_w1 = 0;
  msg.rcv_w2 = 0;
  msg.rcv_w3 = 0;

  do {
    RETURN(&msg);
    msg.snd_invKey=KR_RETURN;
    msg.snd_key0=KR_VOID;
 } while ( ProcessRequest(&msg) );

 return 0;
}
