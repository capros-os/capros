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
#include <eros/ProcessKey.h>
#include <eros/ProcessState.h>
#include <eros/machine/Registers.h>
#include <eros/KeyBitsKey.h>
#include <eros/NodeKey.h>
#include <domain/domdbg.h>
#include <domain/PccKey.h>
#include <domain/ProcessCreatorKey.h>
#include <eros/KeyBitsKey.h>

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
  struct Registers regs;

  node_copy(KR_CONSTIT, KC_OSTREAM, KR_OSTREAM);
  node_copy(KR_CONSTIT, KC_KEYBITS, KR_KEYBITS);
  node_copy(KR_CONSTIT, KC_DCC, KR_DCC);
  node_copy(KR_CONSTIT, KC_HELLO_PC, KR_HELLO_PC);
  node_copy(KR_CONSTIT, KC_HELLO_SEG, KR_HELLO_SEG);

  capros_Number_value nkv;
  capros_Number_getValue(KR_HELLO_PC, &nkv);
  
  kdprintf(KR_OSTREAM, "About to invoke dcc\n");

  msg.snd_key0 = KR_SPCBANK;
  msg.snd_key1 = KR_SCHED;
  msg.snd_key2 = KR_VOID;
  msg.snd_rsmkey = KR_VOID;
  msg.snd_data = 0;
  msg.snd_len = 0;

  msg.rcv_key0 = KR_PROCCRE;
  msg.rcv_key1 = KR_VOID;
  msg.rcv_key2 = KR_VOID;
  msg.rcv_rsmkey = KR_VOID;
  msg.rcv_len = 0;		/* no data returned */

  {
    uint32_t result;
    
    msg.snd_code = OC_PCC_CreateProcessCreator;
    msg.snd_invKey = KR_DCC;
    result = CALL(&msg);
    ShowKey(KR_OSTREAM, KR_KEYBITS, KR_PROCCRE);
    kdprintf(KR_OSTREAM, "GOT PROCCRE Result is 0x%08x\n", result);
  }

  kdprintf(KR_OSTREAM, "About to invoke new proccre\n");
  
  msg.snd_key0 = KR_SPCBANK;
  msg.snd_key1 = KR_VOID;
  msg.snd_key2 = KR_VOID;
  msg.snd_rsmkey = KR_VOID;
  msg.snd_data = 0;
  msg.snd_len = 0;

  msg.rcv_key0 = KR_NEWDOM;
  msg.rcv_key1 = KR_VOID;
  msg.rcv_key2 = KR_VOID;
  msg.rcv_rsmkey = KR_VOID;
  msg.rcv_len = 0;		/* no data returned */

  {
    uint32_t result;
    msg.snd_code = OC_ProcCre_CreateProcess;
    msg.snd_invKey = KR_PROCCRE;
    result = CALL(&msg);
    ShowKey(KR_OSTREAM, KR_KEYBITS, KR_PROCCRE);
    ShowKey(KR_OSTREAM, KR_KEYBITS, KR_NEWDOM);
    kdprintf(KR_OSTREAM, "Result is 0x%08x\n", result);

    if (result != RC_OK) {
      kdprintf(KR_OSTREAM, "EXIT with 0x%08x\n", result);
      return 0;
    }
  }

  kdprintf(KR_OSTREAM, "Populate new process\n");

  /* Install the schedule key into the domain: */
  (void) process_swap(KR_NEWDOM, ProcSched, KR_SCHED, KR_VOID);
  
  kdprintf(KR_OSTREAM, "Installed sched\n");

  /* Install hello address space into the domain root: */
  (void) process_swap(KR_NEWDOM, ProcAddrSpace, KR_HELLO_SEG, KR_VOID);

  /* Install the program counter key into the domain: */
  (void) process_swap(KR_NEWDOM, ProcPCandSP, KR_HELLO_PC, KR_VOID);
  
  kdprintf(KR_OSTREAM, "Installed addrspace and program counter\n");

  /* Fetch out the register values, mostly for the benefit of
     Retrieving the PC -- this prevents us from needing to hard-code
     the PC, which will inevitably change. */
  (void) process_get_regs(KR_NEWDOM, &regs);

  kdprintf(KR_OSTREAM, "Got regs\n");

  /* Unless we set them otherwise, the register values are zero.  The
     PC has already been set.  We now need to initialize the stack
     pointer and the segment registers. */
  regs.CS = DOMAIN_CODE_SEG;
  regs.SS = DOMAIN_DATA_SEG;
  regs.DS = DOMAIN_DATA_SEG;
  regs.ES = DOMAIN_DATA_SEG;
  regs.FS = DOMAIN_DATA_SEG;
  regs.GS = DOMAIN_DATA_SEG;
  regs.EFLAGS = 0x0200;
  regs.domState = RS_Waiting;
  regs.domFlags = 0;
  regs.faultCode = 0;
  regs.faultInfo = 0;
  
  /* Set the new register values. */
  (void) process_set_regs(KR_NEWDOM, &regs);

  kdprintf(KR_OSTREAM, "Wrote regs\n");

  /* Install the bank and domain key for this domain: */
  kdprintf(KR_OSTREAM, "Give it bank, dom key\n");

  (void) process_swap_keyreg(KR_NEWDOM, KR_SELF, KR_NEWDOM, KR_VOID);
  (void) process_swap_keyreg(KR_NEWDOM, KR_SPCBANK, KR_SPCBANK, KR_VOID);
  (void) process_swap_keyreg(KR_NEWDOM, 5, KR_OSTREAM, KR_VOID);

  kdprintf(KR_OSTREAM, "About to call get fault key\n");

  /* Make a restart key to start up the new domain creator: */
  (void) process_make_fault_key(KR_NEWDOM, KR_SCRATCH);
  ShowKey(KR_OSTREAM, KR_KEYBITS, KR_SCRATCH);

  {
    Message msg;
    msg.snd_key0 = KR_VOID;
    msg.snd_key1 = KR_VOID;
    msg.snd_key2 = KR_VOID;
    msg.snd_rsmkey = KR_VOID;
    msg.snd_code = 0;		/* ordinary restart */
    msg.snd_len = 0;
    msg.snd_invKey = KR_SCRATCH;

    SEND(&msg);
  }

  kdprintf(KR_OSTREAM, "About to mk start key\n");

  /* Now make a NODESTROY start key to return: */
  (void) process_make_start_key(KR_NEWDOM, 0, KR_NEWDOM);

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
  msg.rcv_len = 0;		/* no data returned */

  {
    uint32_t result;
    
    msg.snd_code = OC_ProcCre_CreateProcess;
    msg.snd_invKey = KR_NEWDOM;
    result = CALL(&msg);
    kdprintf(KR_OSTREAM, "Result is 0x%08x -- DONE!!!\n", result);
  }
  return 0;
} /* end of main */
