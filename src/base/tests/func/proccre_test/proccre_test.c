/*
 * Copyright (C) 1998, 1999, Jonathan S. Shapiro.
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

#include <eros/target.h>
#include <eros/Invoke.h>
#include <eros/KeyConst.h>
#include <idl/capros/Process.h>
#include <idl/capros/Node.h>
#include <idl/capros/ProcCre.h>
#include <idl/capros/PCC.h>
#include <domain/domdbg.h>

#define KR_VOID 0

#define KR_CONSTIT     1
#define KR_SELF        2
#define KR_SPCBANK     4
#define KR_SCHED       5

#define KR_SCRATCH     6
#define KR_NEWDOM      7	/* new domain */
#define KR_PROCCRE      8	/* new domain creator */

#define KR_KEYBITS     9
#define KR_OSTREAM    10
#define KR_HELLO_PC   11
#define KR_HELLO_SEG  12
#define KR_DCC        15



#define KC_OSTREAM   0
#define KC_KEYBITS   1
#define KC_DCC       2
#define KC_HELLO_PC  3
#define KC_HELLO_SEG 4

void ShowKey(uint32_t krConsole, uint32_t krKeyBits, uint32_t kr);

const uint32_t __rt_stack_pages = 1;
const uint32_t __rt_stack_pointer = 0x20000;

int
main()
{
  Message msg;
  uint32_t result;

  capros_Node_getSlot(KR_CONSTIT, KC_OSTREAM, KR_OSTREAM);
  capros_Node_getSlot(KR_CONSTIT, KC_KEYBITS, KR_KEYBITS);
  capros_Node_getSlot(KR_CONSTIT, KC_DCC, KR_DCC);
  capros_Node_getSlot(KR_CONSTIT, KC_HELLO_PC, KR_HELLO_PC);
  capros_Node_getSlot(KR_CONSTIT, KC_HELLO_SEG, KR_HELLO_SEG);

  capros_Number_value nkv;
  capros_Number_getValue(KR_HELLO_PC, &nkv);
  
  kdprintf(KR_OSTREAM, "About to invoke pcc\n");

  result = capros_PCC_createProcessCreator(KR_DCC, KR_SPCBANK, KR_SCHED,
               KR_PROCCRE);
    
  ShowKey(KR_OSTREAM, KR_KEYBITS, KR_PROCCRE);
  kdprintf(KR_OSTREAM, "GOT PROCCRE Result is 0x%08x\n", result);

  kdprintf(KR_OSTREAM, "About to invoke new proccre\n");
  
  result = capros_ProcCre_createProcess(KR_PROCCRE, KR_SPCBANK, KR_NEWDOM);
  ShowKey(KR_OSTREAM, KR_KEYBITS, KR_PROCCRE);
  ShowKey(KR_OSTREAM, KR_KEYBITS, KR_NEWDOM);
  kdprintf(KR_OSTREAM, "Result is 0x%08x\n", result);

  if (result != RC_OK) {
    kdprintf(KR_OSTREAM, "EXIT with 0x%08x\n", result);
    return 0;
  }

  kdprintf(KR_OSTREAM, "Populate new process\n");

  /* Install the schedule key into the domain: */
  (void) capros_Process_swapSchedule(KR_NEWDOM, KR_SCHED, KR_VOID);
  
  /* Install hello address space and PC into the process: */
  (void) capros_Process_swapAddrSpaceAndPC32(KR_NEWDOM, KR_HELLO_SEG,
           nkv.value[0], KR_VOID);

  /* Install the bank and domain key for this domain: */
  (void) capros_Process_swapKeyReg(KR_NEWDOM, KR_SELF, KR_NEWDOM, KR_VOID);
  (void) capros_Process_swapKeyReg(KR_NEWDOM, KR_SPCBANK, KR_SPCBANK, KR_VOID);
  (void) capros_Process_swapKeyReg(KR_NEWDOM, 5, KR_OSTREAM, KR_VOID);

  kdprintf(KR_OSTREAM, "About to call get fault key\n");

  /* Make a resume key to start up the new domain creator: */
  (void) capros_Process_makeResumeKey(KR_NEWDOM, KR_SCRATCH);
  ShowKey(KR_OSTREAM, KR_KEYBITS, KR_SCRATCH);

  msg.snd_key0 = KR_VOID;
  msg.snd_key1 = KR_VOID;
  msg.snd_key2 = KR_VOID;
  msg.snd_rsmkey = KR_VOID;
  msg.snd_code = 0;
  msg.snd_len = 0;
  msg.snd_invKey = KR_SCRATCH;

  SEND(&msg);

  kdprintf(KR_OSTREAM, "About to mk start key\n");

  /* Now make a NODESTROY start key to return: */
  (void) capros_Process_makeStartKey(KR_NEWDOM, 0, KR_NEWDOM);

  ShowKey(KR_OSTREAM, KR_KEYBITS, KR_NEWDOM);
  kdprintf(KR_OSTREAM, "Got start key. Invoke it:\n");

  msg.snd_key0 = KR_VOID;
  msg.snd_key1 = KR_VOID;
  msg.snd_key2 = KR_VOID;
  msg.snd_rsmkey = KR_VOID;
  msg.snd_data = 0;
  msg.snd_len = 0;

  msg.rcv_key0 = KR_VOID;
  msg.rcv_key1 = KR_VOID;
  msg.rcv_key2 = KR_VOID;
  msg.rcv_rsmkey = KR_VOID;
  msg.rcv_limit = 0;		/* no data returned */

  msg.snd_code = 1234;	// newdom will ignore this
  msg.snd_invKey = KR_NEWDOM;
  result = CALL(&msg);

  kdprintf(KR_OSTREAM, "Result is 0x%08x\n", result);

  result = capros_ProcCre_destroyProcess(KR_PROCCRE, KR_SPCBANK, KR_NEWDOM);
  kdprintf(KR_OSTREAM, "Destroy process, result is 0x%08x\n", result);

  kprintf(KR_OSTREAM, "DONE!!!\n");
  return 0;
} /* end of main */
