/*
 * Copyright (C) 2007, Strawberry Development Group
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

#include <string.h>
#include <eros/target.h>
#include <eros/Invoke.h>
#include <eros/ProcessKey.h>
#include <eros/ConsoleKey.h>
#include <eros/StdKeyType.h>
#include <idl/capros/Sleep.h>
#include <domain/Runtime.h>
#include <domain/domdbg.h>
#include <idl/capros/arch/arm/SysTrace.h>

#define KR_ECHO 8
#define KR_SLEEP 9
#define KR_OSTREAM 10
#define KR_SYSTRACE 11
#define KR_PROC2_PROCESS 12

#define ADDR1 0x40000
#define BUF_SIZE EROS_PAGE_SIZE

/* It is intended that this should be a small space domain */
const uint32_t __rt_stack_pages = 0;
const uint32_t __rt_stack_pointer = 0x21000;
const uint32_t __rt_unkept = 1;	/* do not mess with keeper */

Message msg;

#define checkEqual(a,b) \
  if ((a) != (b)) \
    kprintf(KR_OSTREAM, "Line %d, expecting %s=%d(0x%x), got %d(0x%x)\n", \
            __LINE__, #a, (b), (b), (a), (a));

void
checkMessage(uint32_t code, uint32_t w1, uint32_t w2, uint32_t w3,
  unsigned int sentSize, unsigned int xmittedSize,
  uint8_t * strPtr, int lineNo)
{
  int i;
  Message * const sharedMsg = (Message *)ADDR1;

  checkEqual(sharedMsg->rcv_code, code);
  checkEqual(sharedMsg->rcv_w1, w1);
  checkEqual(sharedMsg->rcv_w2, w2);
  checkEqual(sharedMsg->rcv_w3, w3);
  // checkEqual(sharedMsg->rcv_sent, sentSize);
  if (sharedMsg->rcv_sent != sentSize) {
    kprintf(KR_OSTREAM, "Line %d, expecting %s=%d(0x%x), got %d(0x%x)\n", \
            lineNo, "sent length",
            sentSize, sentSize,
            (sharedMsg->rcv_sent), (sharedMsg->rcv_sent) );
  } else {
    if (memcmp(strPtr,
               (char *)sharedMsg + sizeof(Message),
               xmittedSize)) {
      kprintf(KR_OSTREAM, "Line %d, string mismatch\n",
              lineNo);
    } else {
      // Check that the rest of the buffer was undisturbed.
      uint32_t * p = (uint32_t *) ADDR1;
      for (i = (sizeof(Message) + xmittedSize + 3)/4;
           i < BUF_SIZE/4; i++) {
        checkEqual(p[i], 0xbadbad00);
      }
    }
  }
}

int
main()
{
  capros_key_type keyType;
  result_t ret;

  msg.snd_key0 = KR_VOID;
  msg.snd_key1 = KR_VOID;
  msg.snd_key2 = KR_VOID;
  msg.snd_rsmkey = KR_VOID;
  msg.snd_len = 0;

  msg.rcv_key0 = KR_ARG(0);
  msg.rcv_key1 = KR_ARG(1);
  msg.rcv_key2 = KR_ARG(2);
  msg.rcv_rsmkey = KR_RETURN;
  msg.rcv_limit = 0;		/* no data returned */

  /*** Call process key to proc2 ***/

  ret = capros_key_getType(KR_ARG(0), &keyType);
  checkEqual(ret, RC_OK);
  checkEqual(keyType, AKT_Process);

  /*** Call Console key ***/

  msg.snd_invKey = KR_OSTREAM;
  msg.snd_code = OC_capros_key_getType;
  msg.rcv_w1 = msg.rcv_w2 = msg.rcv_w3 = 0xbadbad03;
  CALL(&msg);
  checkEqual(msg.rcv_code, RC_OK);
  checkEqual(msg.rcv_w1, AKT_Console);
  checkEqual(msg.rcv_w2, 0);
  checkEqual(msg.rcv_w3, 0);

  ret = capros_key_getType(KR_ARG(0), &keyType);
  checkEqual(ret, RC_capros_key_Void);
//// check arg1 and 2

  /* Get echo process started. */
  process_make_fault_key(KR_PROC2_PROCESS, KR_RETURN);
  msg.snd_invKey = KR_RETURN;
  msg.snd_code = RC_OK;
  SEND(&msg);

  ret = capros_key_getType(KR_PROC2_PROCESS, &keyType);
  checkEqual(ret, RC_OK);
  checkEqual(keyType, AKT_Process);

  kprintf(KR_OSTREAM, "NP Return to Process key, no returnee\n");

  msg.snd_invKey = KR_PROC2_PROCESS;
  msg.snd_code = OC_capros_key_getType;	////OC_Process_GetRegs32;
  msg.snd_rsmkey = KR_VOID;
  msg.invType = IT_Return;
  INVOKECAP(&msg);

  kprintf(KR_OSTREAM, "Called by proc2\n");

  msg.snd_invKey = KR_RETURN;	// return to proc2
  msg.snd_code = 5;
  msg.snd_w1 = 77;
  msg.snd_w2 = 88;
  msg.snd_w3 = 99;
  uint8_t teststring[] = "Test string";
  msg.snd_len = sizeof(teststring);
  msg.snd_data = &teststring;
  RETURN(&msg);

  kprintf(KR_OSTREAM, "Called back by proc2\n");
  checkMessage(5, 77, 88, 99,
               sizeof(teststring), sizeof(teststring),
               (uint8_t *)&teststring, __LINE__);

  RETURN(&msg);

  kprintf(KR_OSTREAM, "Called by proc2, short string\n");
  checkMessage(5, 77, 88, 99,
               sizeof(teststring), 5, (uint8_t *)&teststring, __LINE__);

  kprintf(KR_OSTREAM, "Done\n");

  return 0;
}
