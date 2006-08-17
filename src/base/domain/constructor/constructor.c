/*
 * Copyright (C) 1998, 1999, 2001, Jonathan S. Shapiro.
 * Copyright (C) 2005, 2006, Strawberry Development Group.
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

/* A constructor is responsible for building program instances.  The
 * constructor holds copies of each entry of the constituents node. In
 * addition, it holds the target process' keeper key, address space
 * key, symbol table ke, and initial PC.
 *
 * BOOTSTRAP NOTE 1:
 * 
 * To simplify system image construction, the constructor does some
 * minimal analysis at startup time.  If KC_PROD_CON0 and KC_PROD_XCON
 * are not void, they are accepted as holding the product constituents,
 * and KC_DCC should hold the domain creator for the constituents.
 * 
 * If initial constituents are found, the factory startup code assumes
 * that the factory should be initially sealed, and that no holes beyond
 * those that are apparent from the initial constituents are present,
 * and that KC_DCC is the actual domain creator.
 */

#include <string.h>
#include <stdbool.h>
#include <eros/target.h>
#include <eros/Invoke.h>
#include <eros/cap-instr.h>
#include <eros/NodeKey.h>
#include <eros/ProcessKey.h>
#include <eros/KeyConst.h>
#include <eros/StdKeyType.h>
#include <eros/ProcessState.h>

#include <idl/eros/key.h>
#include <idl/eros/Discrim.h>

#include <domain/domdbg.h>
#include <domain/ConstructorKey.h>
#include <domain/ProcessCreatorKey.h>
#include <domain/PccKey.h>
#include <domain/ProtoSpace.h>
#include <domain/SpaceBankKey.h>
#include <domain/Runtime.h>

#include "constituents.h"
#include "debug.h"

uint32_t __rt_unkept = 1;

#define KR_SCRATCH      KR_APP(0)
#define KR_OSTREAM      KR_APP(2)
#define KR_YIELDCRE     KR_APP(3)
#define KR_YIELDBITS    KR_APP(4)
#define KR_NEWDOM       KR_APP(5)

#define KR_PROD_CON0    KR_APP(6) /* product's constituents */
#define KR_PROD_XCON    KR_APP(7) /* product's extended constituents */
#define KR_RO_YIELDBITS KR_APP(8) /* product's extended constituents */

#define KR_ARG0    KR_ARG(0)
#define KR_ARG1    KR_ARG(1)
#define KR_ARG2    KR_ARG(2)

/* Extended Constituents: */
#define XCON_KEEPER     0
#define XCON_ADDRSPACE  1
#define XCON_SYMTAB     2
#define XCON_PC         3

typedef struct {
  int frozen;
  int has_holes;
} ConstructorInfo;

void
CheckDiscretion(uint32_t kr, ConstructorInfo *ci)
{
  uint32_t result;
  uint32_t keyInfo;
  bool isDiscreet;
  
  node_copy(KR_CONSTIT, KC_DISCRIM, KR_SCRATCH);

  result = eros_Discrim_verify(KR_SCRATCH, kr, &isDiscreet);
  if (result == RC_OK && isDiscreet)
    return;

  node_copy(KR_CONSTIT, KC_YIELDCRE, KR_SCRATCH);

  result = proccre_amplify_gate(KR_SCRATCH, kr, KR_SCRATCH, 0, &keyInfo);

  if (result == RC_OK && keyInfo == 0) {
    uint32_t isDiscreet;
    /* This key is a requestor's key to a constructor. Ask the
       constructor if it is discreet */
    result = constructor_is_discreet(kr, &isDiscreet);
    if (result == RC_OK && isDiscreet)
      return;
  }

  ci->has_holes = 1;
}

void
InitConstructor(ConstructorInfo *ci)
{
  uint32_t result;
  uint32_t keyType;

  ci->frozen = 0;
  ci->has_holes = 0;
  
  node_copy(KR_CONSTIT, KC_OSTREAM, KR_OSTREAM);

  DEBUG(init) kdprintf(KR_OSTREAM, "constructor init\n");
  
  node_copy(KR_CONSTIT, KC_PROD_CON0, KR_PROD_CON0);

  result = eros_key_getType(KR_PROD_CON0, &keyType);
  if (result != RC_eros_key_Void) {
    /* This is an initially frozen constructor.  Use the provided
       constituents, and assume that KC_YIELDCRE already holds the
       proper domain creator. */
    node_copy(KR_CONSTIT, KC_YIELDCRE, KR_YIELDCRE);
    node_copy(KR_CONSTIT, KC_PROD_CON0, KR_PROD_CON0);
    node_copy(KR_CONSTIT, KC_PROD_XCON, KR_PROD_XCON);

    ci->frozen = 1;
  }
  else {
    /* Build a new domain creator for use in crafting products */

    /* use KR_YIELDCRE and KR_DISCRIM as scratch regs for a moment: */
    node_copy(KR_CONSTIT, KC_PCC, KR_YIELDCRE);
    node_copy(KR_SELF, ProcSched, KR_SCRATCH);

    {
      Message msg;

      msg.snd_key0 = KR_BANK;
      msg.snd_key1 = KR_SCRATCH;
      msg.snd_key2 = KR_VOID;
      msg.snd_rsmkey = KR_VOID;
      msg.snd_data = 0;
      msg.snd_len = 0;
      msg.snd_code = OC_PCC_CreateProcessCreator;
      msg.snd_invKey = KR_YIELDCRE;

      msg.rcv_key0 = KR_YIELDCRE;
      msg.rcv_key1 = KR_VOID;
      msg.rcv_key2 = KR_VOID;
      msg.rcv_rsmkey = KR_VOID;
      msg.rcv_limit = 0;	/* no data returned */

      result = CALL(&msg);
      DEBUG(init) kdprintf(KR_OSTREAM, "GOT DOMCRE Result is 0x%08x\n", result);
    }
  
    spcbank_buy_nodes(KR_BANK, 2, KR_PROD_CON0, KR_PROD_XCON, KR_VOID);
  }
    

  /* Create a runtime bits node appropriate for our yields: */
  spcbank_buy_nodes(KR_BANK, 1, KR_YIELDBITS, KR_VOID, KR_VOID);
  node_clone(KR_YIELDBITS, KR_RTBITS);
  node_swap(KR_YIELDBITS, RKT_CREATOR, KR_YIELDCRE, KR_VOID);

  /* Now make the yieldbits key read-only. */
  node_make_node_key(KR_YIELDBITS, EROS_PAGE_BLSS, SEGPRM_RO,
		     KR_RO_YIELDBITS); 
}

/* In spite of unorthodox fabrication, the constructor self-destructs
   in the usual way. */
void
Sepuku()
{
  /* FIX: giving up the constituent nodes breaks our products */
  
  /* Give up the first constituent node. */
  /* Give up the second constituent node */
  spcbank_return_node(KR_BANK, KR_PROD_CON0);
  spcbank_return_node(KR_BANK, KR_PROD_XCON);
  spcbank_return_node(KR_BANK, KR_YIELDBITS);

  /* node_copy(KR_CONSTIT, KC_MYDOMCRE, KR_DOMCRE); */
  node_copy(KR_CONSTIT, KC_PROTOSPACE, KR_PROD_CON0);

  spcbank_return_node(KR_BANK, KR_CONSTIT);

  /* Invoke the protospace with arguments indicating that we should be
     demolished as a small space domain */
  protospace_destroy(KR_VOID, KR_PROD_CON0, KR_SELF,
		     KR_CREATOR, KR_BANK, 1);
}

uint32_t
MakeNewProduct(Message *msg)
{
  uint32_t result;
  struct Registers regs;

  DEBUG(product) kdprintf(KR_OSTREAM, "Making new product...\n");

  result = proccre_create_process(KR_YIELDCRE, KR_ARG0, KR_NEWDOM);
  if (result != RC_OK) {
    kdprintf(KR_OSTREAM, "make prod failed with 0x%x\n", result);
    return result;
  }

  /* NOTE that if proccre_create_process succeeded, we know it's a good
     space bank. */
  
  /* Build a constiuents node, since we will need the scratch register
     later */
  result = spcbank_buy_nodes(KR_ARG0, 1, KR_SCRATCH, KR_VOID, KR_VOID);
  if (result != RC_OK)
    goto destroy_product;

  /* clone the product constituents into the new constituents node: */
  node_clone(KR_SCRATCH, KR_PROD_CON0);

  (void) process_swap_keyreg(KR_NEWDOM, KR_CONSTIT, KR_SCRATCH, KR_VOID);
  (void) process_swap_keyreg(KR_NEWDOM, KR_RTBITS, KR_RO_YIELDBITS, KR_VOID);

  DEBUG(product) kdprintf(KR_OSTREAM, "Populate new domain\n");

  /* Install protospace into the domain root: */
  (void) node_copy(KR_CONSTIT, KC_PROTOSPACE, KR_SCRATCH);
  (void) process_swap(KR_NEWDOM, ProcAddrSpace, KR_SCRATCH, KR_VOID);

  DEBUG(product) kdprintf(KR_OSTREAM, "Installed protospace\n");

  /* Install the schedule key into the domain: */
  (void) process_swap(KR_NEWDOM, ProcSched, KR_ARG1, KR_VOID);
  
  DEBUG(product) kdprintf(KR_OSTREAM, "Installed sched\n");

  /* Keeper constructor to keeper slot */
  (void) node_copy(KR_PROD_XCON, XCON_KEEPER, KR_SCRATCH);
  (void) process_swap(KR_NEWDOM, ProcKeeper, KR_SCRATCH, KR_VOID);

  /* Fetch out the register values, mostly for the benefit of
     Retrieving the PC -- this prevents us from needing to hard-code
     the PC, which will inevitably change. */
  (void) process_get_regs(KR_NEWDOM, &regs);

  DEBUG(product) kdprintf(KR_OSTREAM, "Got regs\n");

  regs.faultCode = 0;
  regs.faultInfo = 0;
  regs.domState = RS_Waiting;
  regs.domFlags = 0;
  regs.pc = 0;			/* Place Holder!! */
#if defined(EROS_TARGET_i486)
  /* Unless we set them otherwise, the register values are zero.  The
     PC has already been set.  We now need to initialize the stack
     pointer and the segment registers. */
  regs.CS = DOMAIN_CODE_SEG;
  regs.SS = DOMAIN_DATA_SEG;
  regs.DS = DOMAIN_DATA_SEG;
  regs.ES = DOMAIN_DATA_SEG;
  regs.FS = DOMAIN_DATA_SEG;
  regs.GS = DOMAIN_PSEUDO_SEG;
  regs.nextPC = 0;		/* Place Holder!! */
  regs.EFLAGS = 0x200;
#elif defined(EROS_TARGET_arm)
#else
#error unknown target
#endif
  
  /* Set the new register values. */
  (void) process_set_regs(KR_NEWDOM, &regs);

  DEBUG(product) kdprintf(KR_OSTREAM, "Installed program counter\n");

  (void) process_swap_keyreg(KR_NEWDOM, KR_SELF, KR_NEWDOM, KR_VOID);
  (void) process_swap_keyreg(KR_NEWDOM, KR_CREATOR, KR_YIELDCRE, KR_VOID);
  (void) process_swap_keyreg(KR_NEWDOM, KR_BANK, KR_ARG0, KR_VOID);
  (void) process_swap_keyreg(KR_NEWDOM, KR_SCHED, KR_ARG1, KR_VOID);

  DEBUG(product) kdprintf(KR_OSTREAM, "Sched in target KR_SCHED\n");

  (void) node_copy(KR_PROD_XCON, XCON_ADDRSPACE, KR_SCRATCH);
  (void) process_swap_keyreg(KR_NEWDOM, PSKR_SPACE, KR_SCRATCH, KR_VOID);

  (void) node_copy(KR_PROD_XCON, XCON_SYMTAB, KR_SCRATCH);
  (void) process_swap(KR_NEWDOM, ProcSymSpace, KR_SCRATCH, KR_VOID);

  (void) node_copy(KR_PROD_XCON, XCON_PC, KR_SCRATCH);
  (void) process_swap_keyreg(KR_NEWDOM, PSKR_PROC_PC, KR_SCRATCH, KR_VOID);

  /* User ARG2 to key arg slot 0 */
  (void) process_swap_keyreg(KR_NEWDOM, KR_ARG(0), KR_ARG2, KR_VOID);

  /* Resume key to KR_RETURN */
  (void) process_swap_keyreg(KR_NEWDOM, KR_RETURN, KR_RETURN, KR_VOID);

  /* Make up a fault key to the new process so we can set it in motion */

  DEBUG(product) kdprintf(KR_OSTREAM, "About to call get fault key\n");
  (void) process_make_fault_key(KR_NEWDOM, KR_SCRATCH);

  DEBUG(start) kdprintf(KR_OSTREAM, "Invoking fault key to yield...\n");

  msg->snd_key0 = KR_VOID;
  msg->snd_key1 = KR_VOID;
  msg->snd_key2 = KR_VOID;
  msg->snd_rsmkey = KR_VOID;
  msg->snd_code = 0;		/* ordinary restart */
  msg->snd_w1 = 0;
  msg->snd_w2 = 0;
  msg->snd_w3 = 0;
  msg->snd_len = 0;
  msg->snd_invKey = KR_SCRATCH;
  
  return RC_OK;

destroy_product:
  (void) proccre_destroy_process(KR_YIELDCRE, KR_ARG0, KR_NEWDOM);
  return RC_eros_key_NoMoreNodes;
}

int
is_not_discreet(uint32_t kr, ConstructorInfo *ci)
{
  uint32_t result;
  uint32_t keyInfo;
  bool isDiscreet;
  
  node_copy(KR_CONSTIT, KC_DISCRIM, KR_SCRATCH);
  
  DEBUG(misc) kdprintf(KR_OSTREAM, "constructor: is_not_discreet(): discrim_verify\n");

  eros_Discrim_verify(KR_SCRATCH, kr, &isDiscreet);
  if (isDiscreet)
    return 0;			/* ok */

  DEBUG(misc) kdprintf(KR_OSTREAM, "constructor: is_not_discreet(): proccre_amplify\n");
  result = proccre_amplify_gate(KR_YIELDCRE, kr, KR_SCRATCH, 0, &keyInfo);
  if (result == RC_OK && keyInfo == 0) {
    uint32_t isDiscreet;
    /* This key is a requestor's key to a constructor. Ask the
       constructor if it is discreet */
    result = constructor_is_discreet(kr, &isDiscreet);
    if (result == RC_OK && isDiscreet)
      return 0;			/* ok */
  }

  return 1;
}


/* Someday this should start building up a list of holes... */
void
add_new_hole(uint32_t kr, ConstructorInfo *ci)
{
  ci->has_holes = 1;
}

uint32_t
insert_constituent(uint32_t ndx, uint32_t kr, ConstructorInfo *ci)
{
  DEBUG(build) kdprintf(KR_OSTREAM, "constructor: insert constituent %d\n", ndx);

  if (ndx >= EROS_NODE_SIZE)
    return RC_eros_key_RequestError;
    
  if ( is_not_discreet(kr, ci) )
    add_new_hole(kr, ci);
  
  /* now insert the constituent in the proper constituents node: */
  node_swap(KR_PROD_CON0, ndx, kr, KR_VOID);
	    
  return RC_OK;
}

uint32_t
insert_xconstituent(uint32_t ndx, uint32_t kr, ConstructorInfo *ci)
{
  DEBUG(build) kdprintf(KR_OSTREAM, "constructor: insert xconstituent %d\n", ndx);

  if (ndx == XCON_PC) {
    uint32_t obClass;
    
    /* This copy IS redundant with the one in is_not_discreet(), but
       this path is not performance critical and clarity matters too. */
    node_copy(KR_CONSTIT, KC_DISCRIM, KR_SCRATCH);
  
    eros_Discrim_classify(KR_SCRATCH, kr, &obClass);
    if (obClass != eros_Discrim_clNumber)
      return RC_eros_key_RequestError;
  }

  if (ndx > XCON_PC)
    return RC_eros_key_RequestError;
    
  if ( is_not_discreet(kr, ci) )
    add_new_hole(kr, ci);
  
  /* now insert the constituent in the proper constituents node: */
  node_swap(KR_PROD_XCON, ndx, kr, KR_VOID);
	    
  return RC_OK;
}

int
ProcessRequest(Message *msg, ConstructorInfo *ci)
{
  
  /*initialize the keys being sent*/
  msg->snd_len = 0;
  msg->snd_key0 = KR_VOID;
  msg->snd_key1 = KR_VOID;
  msg->snd_key2 = KR_VOID;
  msg->snd_rsmkey = KR_VOID;
  msg->snd_code = RC_OK;
  msg->snd_w1 = 0;
  msg->snd_w2 = 0;
  msg->snd_w3 = 0;
  msg->snd_invKey = KR_RETURN;

  switch (msg->rcv_code) {
  case OC_Constructor_IsDiscreet:
    {
      if (ci->frozen && !ci->has_holes)
	msg->snd_w1 = 1;
      else
	msg->snd_w1 = 0;
      msg->snd_code = RC_OK;

      return 1;
    }      
    
  case OC_Constructor_Request:
    {
      DEBUG(product) kdprintf(KR_OSTREAM, "constructor: request product\n");

      msg->snd_code = MakeNewProduct(msg);
      return 1;
    }      
    
  case OC_Constructor_Seal:
    {
      DEBUG(build) kdprintf(KR_OSTREAM, "constructor: seal\n");

      ci->frozen = 1;

      process_make_start_key(KR_SELF, 0, KR_NEWDOM);
      msg->snd_key0 = KR_NEWDOM;

      return 1;
    }      

  case OC_Constructor_Insert_Constituent:
    {
      msg->snd_code = insert_constituent(msg->rcv_w1, KR_ARG0, ci);
      
      return 1;
    }      

  case OC_Constructor_Insert_Keeper:
    {
      msg->snd_code = insert_xconstituent(XCON_KEEPER, KR_ARG0, ci);
      
      return 1;
    }      

  case OC_Constructor_Insert_AddrSpace:
    {
      msg->snd_code = insert_xconstituent(XCON_ADDRSPACE, KR_ARG0, ci);
      
      return 1;
    }      

  case OC_Constructor_Insert_Symtab:
    {
      msg->snd_code = insert_xconstituent(XCON_SYMTAB, KR_ARG0, ci);
      
      return 1;
    }      

  case OC_Constructor_Insert_PC:
    {
      msg->snd_code = insert_xconstituent(XCON_PC, KR_ARG0, ci);
      
      return 1;
    }      

  case OC_eros_key_destroy:
    {
      Sepuku();
    }
  
  case OC_eros_key_getType:			/* check alleged keytype */
    {
      switch(msg->rcv_keyInfo) {
      case 0:
	msg->snd_code = RC_OK;
	msg->snd_w1 = AKT_ConstructorRequestor;
	break;
      case 1:
	msg->snd_code = RC_OK;
	msg->snd_w1 = AKT_ConstructorBuilder;
	break;
      }
      return 1;
    }      

  default:
    break;
  }

  msg->snd_code = RC_eros_key_UnknownRequest;
  return 1;
}

int
main()
{
  Message msg;
  ConstructorInfo ci;
  
  InitConstructor(&ci);

  /* Recover our own process key from constit1 into slot 1 */
  process_make_start_key(KR_SELF, 1, KR_SCRATCH);

  msg.snd_key0 = KR_SCRATCH;
  msg.snd_key1 = KR_VOID;
  msg.snd_key2 = KR_VOID;
  msg.snd_rsmkey = KR_VOID;
  msg.snd_code = 0;
  msg.snd_w1 = 0;
  msg.snd_w2 = 0;
  msg.snd_w3 = 0;
  msg.snd_len = 0;
  msg.snd_data = 0;
  msg.snd_invKey = KR_RETURN;

  msg.rcv_key0 = KR_ARG0;
  msg.rcv_key1 = KR_ARG1;
  msg.rcv_key2 = KR_ARG2;
  msg.rcv_rsmkey = KR_RETURN;
  msg.rcv_data = 0;
  msg.rcv_limit = 0;

  do {
    /* no need to re-initialize rcv_len, since always 0 */
    RETURN(&msg);
  } while (ProcessRequest(&msg, &ci));

  return 0;
}
