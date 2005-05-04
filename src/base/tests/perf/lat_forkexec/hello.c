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

/* Pipe -- limited size buffering object for unidirection streams.

   This version assumes single writer/single reader.  Multi
   reader/writer variants will come later. */

#include <eros/target.h>
#include <eros/Invoke.h>
#include <eros/cap-instr.h>
#include <eros/ProcessKey.h>
#include <eros/NodeKey.h>

#if 0
#include <memory.h>
#endif
#include <domain/domdbg.h>
#include <domain/PipeKey.h>
#include <domain/ProtoSpace.h>

#define dbg_init	0x01u   /* requests */
#define dbg_req		0x02u   /* requests */
#define dbg_ack		0x04u   /* acknowledgements */
#define dbg_sleep	0x08u   /* sleep/wakeup */
#define dbg_eof		0x10u   /* sleep/wakeup */

/* Following should be an OR of some of the above */
#define dbg_flags   ( 0u )

#define CND_DEBUG(x) (dbg_##x & dbg_flags)
#define DEBUG(x) if (CND_DEBUG(x))


const uint32_t __rt_stack_pointer = 0x20000;
const uint32_t __rt_stack_pages = 1;

#define KR_VOID        0
#define KR_CONSTIT     1
#define KR_SELF        2
#define KR_DOMCRE      3
#define KR_BANK        4
#define KR_SCHED       5

#if EROS_NODE_SIZE != 32
#error "wrong node size!"
#endif

#define KR_ME          27
#define KR_PROTOSPC    28
#define KR_OSTREAM     29
#define KR_RESUME      31

#define KC_PROTOSPC    2
#define KC_OSTREAM     3

volatile void
teardown(uint32_t caller)
{
  if (caller != KR_RESUME)
    COPY_KEYREG(caller, KR_RESUME);
  
  /* get the protospace */
  node_copy(KR_CONSTIT, KC_PROTOSPC, KR_PROTOSPC);

  /* destroy as small space. */
  protospace_destroy(KR_VOID, KR_PROTOSPC, KR_SELF, KR_DOMCRE,
		     KR_BANK, 1);
  /* NOTREACHED */
}

int
main(void)
{
  Message msg;
#if 0
  uint32_t result;
#endif
  
  node_copy(KR_CONSTIT, KC_OSTREAM, KR_OSTREAM);

  msg.snd_key0 = KR_ME;
  msg.snd_key1 = KR_VOID;
  msg.snd_key2 = KR_VOID;
  msg.snd_rsmkey = KR_VOID;
  msg.rcv_key0 = KR_VOID;
  msg.rcv_key1 = KR_VOID;
  msg.rcv_key2 = KR_VOID;
  msg.rcv_rsmkey = KR_RESUME;
  msg.snd_invKey = KR_RESUME;
  msg.rcv_len = 0;
  msg.snd_len = 0;

#if 0
  result = process_make_start_key(KR_SELF, 0, KR_ME);
  if (result != RC_OK)
    kdprintf(KR_OSTREAM, "Result from hello cre strt key: 0x%x\n",
	     result);
#endif

  kprintf(KR_VOID, "Hello, world\n");

  teardown(KR_RESUME);

  return 0;
}
