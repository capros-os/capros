/*
 * Copyright (C) 1998, 1999, Jonathan S. Shapiro.
 * Copyright (C) 2005, Strawberry Development Group
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

/* Hello World

   A silly test program that we use often enough that I decided to
   just put it in the domain tree.
   */

#include <eros/target.h>
#include <eros/Invoke.h>
#include <eros/NodeKey.h>
#include <eros/ProcessKey.h>
#include <idl/capros/Sleep.h>
#include <domain/domdbg.h>
#if 0
#include <eros/ProcessKey.h>
#include <eros/KeyConst.h>
#include <eros/StdKeyType.h>
#endif

#define KR_CONSTIT  1
#define KR_SELF     2
#define KR_BANK     4
#define KR_OSTREAM  5
#define KR_SLEEP    6
#define KR_ARG0     16
#define KR_ARG1     17
#define KR_ARG2     18
#define KR_PRODUCT  30
#define KR_RETURN   31

#define KC_OSTREAM  0
#define KC_SLEEP    1


const uint32_t __rt_stack_pages = 1;
#if EROS_NODE_SIZE == 16
const uint32_t __rt_stack_pointer = 0x10000;
#elif EROS_NODE_SIZE == 32
const uint32_t __rt_stack_pointer = 0x20000;
#else
#error "Unhandled node size"
#endif

int ProcessRequest( Message *msg )
{
  int i, start, end, wait;

  msg->snd_key0 = KR_VOID;
  msg->snd_key1 = KR_VOID;
  msg->snd_key2 = KR_VOID;
  msg->snd_rsmkey = KR_VOID;
  msg->snd_code = 0;
  msg->snd_w1   = 0;
  msg->snd_w2   = 0;
  msg->snd_w3   = 0;
  msg->snd_len  = 0;
  msg->snd_code = 0;
  msg->snd_invKey = KR_RETURN;;
  
  switch( msg->rcv_code ) {
  case OC_Destroy:
    kprintf( KR_OSTREAM, "child>> OC_Destroy: Dont know how to shoot myself :(\n" );
    /* how is this handled? We are NOT in a small space! */
    return 1;
  case 2:
    start = msg->rcv_w1;
    end   = msg->rcv_w2;
    wait  = msg->rcv_w3;
  
    kprintf( KR_OSTREAM, "child>> Going to count %d..%d, %d msec sleep...\n", start, end, wait );
    for( i=start; i <= end; i++ ) {
      kprintf( KR_OSTREAM, "child>> %d...\n", i );
      capros_Sleep_sleep( KR_SLEEP, wait );
    };
    return 1;
  default:
    kprintf( KR_OSTREAM, "child>> Unknown order code %d!\n", msg->rcv_code );
    return 1;
  };
}

int
main()
{
  Message msg;

  node_copy( KR_CONSTIT, KC_OSTREAM, KR_OSTREAM);
  node_copy( KR_CONSTIT, KC_SLEEP, KR_SLEEP );
  
  process_make_start_key(KR_SELF, 0, KR_PRODUCT);

  kprintf( KR_OSTREAM, "child>> Init done.\n" );

  msg.snd_invKey = KR_RETURN;
  msg.snd_key0 = KR_PRODUCT;
  msg.snd_key1 = KR_VOID;
  msg.snd_key2 = KR_VOID;
  msg.snd_rsmkey = KR_VOID;
  msg.snd_data = 0;
  msg.snd_len  = 0;
  msg.snd_code = 0;
  msg.snd_w1   = 0;
  msg.snd_w2   = 0;
  msg.snd_w3   = 0;

  do{
    msg.rcv_key0 = KR_ARG0;
    msg.rcv_key1 = KR_ARG1;
    msg.rcv_key2 = KR_ARG2;
    msg.rcv_rsmkey = KR_RETURN;
    msg.rcv_len  = 0;
    msg.rcv_code = 0;
    msg.rcv_w1 = 0;
    msg.rcv_w2 = 0;
    msg.rcv_w3 = 0;

    RETURN(&msg);
  } while ( ProcessRequest(&msg) );


  
  return 0;
}
