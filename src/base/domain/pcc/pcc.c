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

/* PCC: Process Creator Creator

   This is one of the primordial processes; it is not created by a
   factory.

   PCC creates all process creators.
   */

#include <stddef.h>
#include <eros/target.h>
#include <eros/Invoke.h>
#include <eros/NodeKey.h>
#include <eros/ProcessKey.h>
#include <eros/ProcessState.h>
#include <eros/cap-instr.h>
#include <eros/KeyConst.h>
#include <eros/StdKeyType.h>
#include <eros/machine/Registers.h>

#include <idl/eros/key.h>
#include <idl/eros/ProcTool.h>
#include <idl/eros/Number.h>

#include <domain/SpaceBankKey.h>
#include <domain/PccKey.h>
#include <domain/Runtime.h>
#include <domain/domdbg.h>
#include "constituents.h"

uint32_t __rt_unkept = 1;

#define DEBUG if (0)
/* #define DEBUG if (1) */

typedef struct {
  uint32_t domcre_pc;
} domcre_info;


#define KR_SCRATCH	  KR_APP(0)	/* scratch reg */
#define KR_NEW_DOMCRE	  KR_APP(1)	/* scratch reg */
#define KR_DOMCRE_CONSTIT KR_APP(2)
#define KR_OSTREAM	  KR_APP(3)	/* so PCC can tell us what's up */
#define KR_PROCTOOL	  KR_APP(4)	/* domain tool */
#define KR_DOMCRE_SEG	  KR_APP(5)	/* code seg for DomCre (RO) */
#define KR_NEWKEYREGS	  KR_APP(6)	/* new key regs node */
#define KR_OURBRAND	  KR_APP(7)	/* distinguished start key to PCC */
#define KR_ARG0		  KR_ARG(0)	/* space bank */
#define KR_ARG1		  KR_ARG(1)	/* schedule */
#define KR_ARG2           KR_ARG(2)
#define KR_ARG3           KR_ARG(3)

#define FALSE 0
#define TRUE 1

uint32_t create_new_domcre(uint32_t krBank, uint32_t krSched, uint32_t domKeyReg,
		       domcre_info *pInfo);
uint32_t create_new_domain(uint32_t krBank, uint32_t domKeyReg);
uint32_t identify_domcre(uint32_t krDomCre);

int
ProcessRequest(Message *argmsg, domcre_info *pInfo)
{
  uint32_t result = RC_OK;
  argmsg->snd_key0 = 0;		/* until proven otherwise */

  switch (argmsg->rcv_code) {
  case OC_PCC_CreateProcessCreator:
    {
      result = create_new_domcre(KR_ARG0, KR_ARG1, KR_NEW_DOMCRE, pInfo);
      if (result == RC_OK)
	argmsg->snd_key0 = KR_NEW_DOMCRE;
      break;
    }
    
  case OC_PCC_IdentifyProcCre:
    result = identify_domcre(KR_ARG0);
    break;

  case OC_eros_key_getType:
    argmsg->snd_w1 = AKT_PCC;
    break;

  default:
    result = RC_eros_key_UnknownRequest;
    break;
  };

  argmsg->snd_code = result;
  return 1;
}

void
init_pcc(domcre_info *pInfo)
{
  node_copy(KR_CONSTIT, KC_DOMCRE_PC, KR_SCRATCH);

  eros_Number_getWord(KR_SCRATCH, &pInfo->domcre_pc);


  node_copy(KR_CONSTIT, KC_OSTREAM, KR_OSTREAM);
  node_copy(KR_CONSTIT, KC_DOMTOOL, KR_PROCTOOL);
  node_copy(KR_CONSTIT, KC_DOMCRE_SEG, KR_DOMCRE_SEG);
  node_copy(KR_CONSTIT, KC_OURBRAND, KR_OURBRAND);
  node_copy(KR_CONSTIT, KC_DOMCRE_CONSTIT, KR_DOMCRE_CONSTIT);

  DEBUG kdprintf(KR_OSTREAM, "PCC Initialized...\n");
}

int
main()
{
  Message msg;
  domcre_info info;
  
  init_pcc(&info);
  
  msg.snd_invKey = KR_VOID;
  msg.snd_key0 = KR_VOID;
  msg.snd_key1 = KR_VOID;
  msg.snd_key2 = KR_VOID;
  msg.snd_rsmkey = KR_VOID;
  msg.snd_data = 0;
  msg.snd_len = 0;
  msg.snd_code = 0;
  msg.snd_w1 = 0;
  msg.snd_w2 = 0;
  msg.snd_w3 = 0;

  msg.rcv_key0 = KR_ARG0;
  msg.rcv_key1 = KR_ARG1;
  msg.rcv_key2 = KR_VOID;
  msg.rcv_rsmkey = KR_ARG3;
  msg.rcv_data = 0;
  msg.rcv_limit = 0;
  msg.rcv_code = 0;
  msg.rcv_w1 = 0;
  msg.rcv_w2 = 0;
  msg.rcv_w3 = 0;

  do {
    RETURN(&msg);
    msg.snd_invKey = KR_ARG3;
  } while ( ProcessRequest(&msg, &info) );

  return 0;
}


#ifdef TARGET_i486

int
destroy_domain(uint32_t krBank, uint32_t krDomKey)
{
  if (eros_ProcTool_canOpener(KR_PROCTOOL, krDomKey, KR_OURBRAND, krDomKey, 0, 0) != RC_OK)
    return FALSE;

  (void) node_copy(krDomKey, ProcGenKeys, KR_NEWKEYREGS);
  (void) spcbank_return_node(krBank, KR_NEWKEYREGS);

  (void) spcbank_return_node(krBank, krDomKey);
      
  DEBUG kdprintf(KR_OSTREAM, "Sold domain back to bank\n");

  return TRUE;
}

uint32_t
create_new_domain(uint32_t krBank, uint32_t krDomKey)
{
  uint32_t isGood;
  int i;
  const eros_Number_value zeroNumber = {{0, 0, 0}};

  DEBUG kdprintf(KR_OSTREAM, "Check official bank...\n");

  if (spcbank_verify_bank(KR_BANK, krBank, &isGood) != RC_OK ||
      isGood == 0)
    return RC_PCC_BadBank;
  
  DEBUG kdprintf(KR_OSTREAM, "OK -- buy domain nodes\n");

  /* Bank is okay, try to buy the space: */
  if (spcbank_buy_nodes(krBank, 2, krDomKey, KR_NEWKEYREGS,
			KR_VOID) != RC_OK)
    return RC_eros_key_NoMoreNodes;

  DEBUG kdprintf(KR_OSTREAM, "Assemble them\n");
  /* We have the nodes.  Make the second the key registers node of the
     first: */
  (void) node_swap(krDomKey, ProcGenKeys, KR_NEWKEYREGS, KR_VOID);

  /* Initialize the fixed registers to zero number keys. */
  for (i = ProcFirstRootRegSlot; i <= ProcLastRootRegSlot; i++)
    (void) node_write_number(krDomKey, i, &zeroNumber);
  
  /* Now install the brand: */
  (void) node_swap(krDomKey, ProcBrand, KR_OURBRAND, KR_VOID);

  DEBUG kdprintf(KR_OSTREAM, "Build domain key:\n");

  /* Now make a domain key of this: */
  (void) eros_ProcTool_makeProcess(KR_PROCTOOL, krDomKey, krDomKey);

  DEBUG kdprintf(KR_OSTREAM, "Return from create_new_domain()\n");

  return RC_OK;
}

uint32_t
create_new_domcre(uint32_t krBank, uint32_t krSched, uint32_t krDomKey,
		  domcre_info *pInfo)
{
  uint32_t result;
  struct Registers regs;

  DEBUG kdprintf(KR_OSTREAM, "About to call create_new_domain()\n");
  result = create_new_domain(krBank, krDomKey);
  DEBUG kdprintf(KR_OSTREAM, "Result = 0x%08x\n", result);

  if (result != RC_OK)
    return result;

#if 0
  /* Buy a node to hold the new segment root. */
  result = spcbank_buy_nodes(krBank, 1, PCC_NEWSEG, KR_VOID, KR_VOID);
  if ( result != RC_OK) {
    DEBUG kdprintf(KR_OSTREAM, "buy node result is 0x%08x\n", result);
    /* Acquisition failed - return what we have and give up. */
    destroy_domain(krBank, krDomKey);
    return RC_PCC_NoSpace;
  }

  DEBUG kdprintf(KR_OSTREAM, "Got new aspace node\n");

  /* Install into newseg slot 0 the basic domcre segment: */
  node_swap(PCC_NEWSEG, 0, KR_DOMCRE_SEG, KR_VOID);

  DEBUG kdprintf(KR_OSTREAM, "Installed codeseg\n");

  /* Buy a page to hold the new domcre's stack. */
  result = spcbank_buy_pages(krBank, 1, PCC_NEWSTACK, KR_VOID, KR_VOID);
  if ( result != RC_OK) {
    DEBUG kdprintf(KR_OSTREAM, "buy page result is 0x%08x\n", result);
    /* Acquisition failed - return what we have and give up. */
    spcbank_return_node(krBank, PCC_NEWSEG);
    destroy_domain(krBank, krDomKey);
    return RC_PCC_NoSpace;
  }

  DEBUG kdprintf(KR_OSTREAM, "Got new page\n");

  /* Install into newseg slot 1 the stack page: */
  node_swap(PCC_NEWSEG, 1, PCC_NEWSTACK, KR_VOID);

  DEBUG kdprintf(KR_OSTREAM, "Installed it\n");

  /* WE WIN -- THE REST IS INITIALIZATION */
  
  /* Fetch a node key of the right BLSS for this address space: */
  (void) node_make_node_key(PCC_NEWSEG, 4 /* BLSS */, PCC_NEWSEG);

  DEBUG kdprintf(KR_OSTREAM, "Made segtree key\n");

  /* Install this address space into the domain root: */
  (void) process_swap(krDomKey, ProcAddrSpace, PCC_NEWSEG, KR_VOID);
#else
  (void) process_swap(krDomKey, ProcAddrSpace, KR_DOMCRE_SEG, KR_VOID);
#endif

  DEBUG kdprintf(KR_OSTREAM, "Installed aspace\n");

  /* Install the schedule key into the domain: */
  (void) process_swap(krDomKey, ProcSched, krSched, KR_VOID);
  
  DEBUG kdprintf(KR_OSTREAM, "Installed sched\n");

  /* Fetch out the register values, mostly for the benefit of
     Retrieving the PC -- this prevents us from needing to hard-code
     the PC, which will inevitably change. */
  (void) process_get_regs(krDomKey, &regs);

  DEBUG kdprintf(KR_OSTREAM, "Got regs\n");

  /* Unless we set them otherwise, the register values are zero.  The
     PC has already been set.  We now need to initialize the stack
     pointer and the segment registers. */
  regs.pc = pInfo->domcre_pc;
  regs.nextPC = pInfo->domcre_pc;
  regs.CS = DOMAIN_CODE_SEG;
  regs.SS = DOMAIN_DATA_SEG;
  regs.DS = DOMAIN_DATA_SEG;
  regs.ES = DOMAIN_DATA_SEG;
  regs.FS = DOMAIN_DATA_SEG;
  regs.GS = DOMAIN_DATA_SEG;
  regs.EFLAGS = 0x200;
  regs.faultCode = 0;
  regs.faultInfo = 0;
  regs.domState = RS_Waiting;
  regs.domFlags = 0;
  
  /* Set the new register values. */
  (void) process_set_regs(krDomKey, &regs);

  DEBUG kdprintf(KR_OSTREAM, "Wrote regs\n");

  /* Populate the new domcre's key registers: */
  process_swap_keyreg(krDomKey, KR_CONSTIT, KR_DOMCRE_CONSTIT, KR_VOID);
  process_swap_keyreg(krDomKey, KR_BANK, KR_BANK, KR_VOID);
  process_swap_keyreg(krDomKey, KR_SELF, krDomKey, KR_VOID);
  
  DEBUG kdprintf(KR_OSTREAM, "About to call get fault key\n");
  /* Make a restart key to start up the new domain creator: */
  (void) process_make_fault_key(krDomKey, KR_SCRATCH);

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

  DEBUG kdprintf(KR_OSTREAM, "About to call get start key\n");
  /* Now make a start key to return: */
  (void) process_make_start_key(krDomKey, 0, krDomKey);

  DEBUG kdprintf(KR_OSTREAM, "Got start key\n");
  return RC_OK;
}
#endif

int
is_our_progeny(uint32_t krStart, uint32_t krNode)
{
  uint32_t capType;
  uint32_t result = 
    eros_ProcTool_canOpener(KR_PROCTOOL, krStart, KR_OURBRAND,
			    krNode, &capType, 0);
  
  if (result == RC_OK && capType != 0)
    return 1;
  return 0;
}

uint32_t make_start_key(uint32_t krDomain, uint16_t keyData, uint32_t krStart);

uint32_t spcbank_return_node(uint32_t krBank, uint32_t krNode);

uint32_t
identify_domcre(uint32_t krDomCre)
{
  return RC_eros_key_UnknownRequest;
}
