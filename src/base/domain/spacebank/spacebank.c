/*
 * Copyright (C) 1998, 1999, Jonathan Adams.
 * Copyright (C) 2001, Jonathan S. Shapiro.
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

/* SpaceBank -- Controls allocation of nodes and pages.
 *
 * 7/2/97 -- Rebuilt from scratch to incorporate Object Frames,
 *         the SuperRange key, etc.  Wheee.
 *           This file holds the main routine, crt0 intializers,
 *         and little else.
 */

#include <stddef.h>
#include <eros/target.h>
#define _REVEAL_KERNEL_KEY_TYPES_
#include <disk/DiskNodeStruct.h>
#include <eros/NodeKey.h>
#include <eros/ProcessKey.h>
#include <eros/Invoke.h>
#include <eros/StdKeyType.h>

#include <idl/eros/key.h>
#include <idl/eros/Range.h>
#include <idl/eros/ProcTool.h>

#include <domain/SpaceBankKey.h>
#include <domain/domdbg.h>
#include <domain/Runtime.h>
#include <stdlib.h>
#include "misc.h"
#include "debug.h"
#include "constituents.h"
#include "spacebank.h"

#include "Bank.h"
#include "ObjSpace.h"
#include "malloc.h"

#define min(a,b) ((a) <= (b) ? (a) : (b))

uint32_t objects_per_frame[eros_Range_otNUM_TYPES];
uint32_t objects_map_mask[eros_Range_otNUM_TYPES];
static const char *type_names[eros_Range_otNUM_TYPES];

/* functions */
int
ProcessRequest(Message *argmsg);
/* ProcessRequest:
 *     Called to interpret and respond to each message received.  
 *   It is passed the Bank the message is for and the Message recieved.
 *   The response, success or failure, is put into the argmsg argument, 
 *   and ProcessRequest should return 1 when it is finished.
 * 
 *     Note that Bank can == NULL, which is prob. a failure.
 *
 *     If 0 is ever returned, the SpaceBank process will quit.  This is a 
 *   *bad thing*, and should never happen. (consider not allowing?)
 */

void InitSpaceBank(void);
/**Handle the stack stuff**/
const uint32_t __rt_stack_pointer = 0x100000;

uint32_t __rt_unkept = 1;

int
main(void)
{
  Message msg;
  char buff[sizeof(struct bank_limits) + 2]; /* two extra for failure
						detection */
  
  node_copy(KR_CONSTIT, KC_PRIMERANGE, KR_SRANGE);
  node_copy(KR_CONSTIT, KC_VOLSIZE, KR_VOLSIZE);
  node_copy(KR_CONSTIT, KC_OSTREAM, KR_OSTREAM);
  node_copy(KR_CONSTIT, KC_PRIMEBANK, KR_PRIMEBANK);
  node_copy(KR_CONSTIT, KC_VERIFIER, KR_VERIFIER);

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
  msg.rcv_key2 = KR_ARG2;
  msg.rcv_rsmkey = KR_RETURN;
  msg.rcv_limit = sizeof(buff);
  msg.rcv_data = buff;
  msg.rcv_code = 0;
  msg.rcv_w1 = 0;
  msg.rcv_w2 = 0;
  msg.rcv_w3 = 0;

  DEBUG(init) kdprintf(KR_OSTREAM, "spacebank: calling InitSpaceBank()\n");
  /* Initialization is not permitted to fail -- this would constitute
     an unrunnable system! */
  InitSpaceBank();
  
  DEBUG(init) kdprintf(KR_OSTREAM, "spacebank: accepting requests()\n");

  for(;;) {
#ifdef TIMESTAMPS
    uint64_t timestamp_before;
    uint64_t timestamp_taken;
#endif
    RETURN(&msg);

    msg.snd_invKey = KR_RETURN;
#ifdef TIMESTAMPS
    timestamp_before = rdtsc();
#endif
    (void) ProcessRequest(&msg);
#ifdef TIMESTAMPS
    timestamp_taken = rdtsc() - timestamp_before;    
    kprintf(KR_OSTREAM,
	     "SpaceBank:  Request %02x took "DW_HEX" cycles.\n",
	     msg.rcv_code,
	     DW_HEX_ARG(timestamp_taken));
    
#endif
  }
}

static int request_ob_type[] = {
  -1,				/* none */ 

  eros_Range_otNode,		/* OC_SpaceBank_Alloc1Node */
  eros_Range_otNode,		/* OC_SpaceBank_Alloc2Nodes */
  eros_Range_otNode,		/* OC_SpaceBank_Alloc3Nodes */
  eros_Range_otNode,		/* OC_SpaceBank_Reclaim1Node */
  eros_Range_otNode,		/* OC_SpaceBank_Reclaim2Nodes */
  eros_Range_otNode,		/* OC_SpaceBank_Reclaim3Nodes */
  eros_Range_otNode,	       /* OC_SpaceBank_ReclaimNodesFromNode */
  eros_Range_otNode,		/* OC_SpaceBank_SeverNode */
	      
  eros_Range_otPage,		/* OC_SpaceBank_Alloc1DataPage */
  eros_Range_otPage,		/* OC_SpaceBank_Alloc2DataPages */
  eros_Range_otPage,		/* OC_SpaceBank_Alloc3DataPages */
  eros_Range_otPage,		/* OC_SpaceBank_Reclaim1DataPage */
  eros_Range_otPage,		/* OC_SpaceBank_Reclaim2DataPages */
  eros_Range_otPage,		/* OC_SpaceBank_Reclaim3DataPages */
  eros_Range_otPage,	        /* OC_SpaceBank_ReclaimDataPagesFromNode */
  eros_Range_otPage,		/* OC_SpaceBank_SeverDataPage */
	      
  -1,-1,-1,-1,-1,-1,-1,-1,      /* 16-23, unused */
	      
  eros_Range_otNode,		/* OC_SpaceBank_Identify1Node */
  eros_Range_otNode,		/* OC_SpaceBank_Identify2Nodes */
  eros_Range_otNode,		/* OC_SpaceBank_Identify3Nodes */

  eros_Range_otPage,		/* OC_SpaceBank_Identify1DataPage */
  eros_Range_otPage,		/* OC_SpaceBank_Identify2DataPages */
  eros_Range_otPage,		/* OC_SpaceBank_Identify3DataPages */
};

/* General note on workings of functions called by ProcessRequest:
 *
 *    Before ProcessRequest calls a function, it zeroes the code of the
 *  snd message structure.  It only modifies the structure if the
 *  function returns a value != RC_OK.  This means if you can do a
 *  dispatch call to another key by mucking with the structure and
 *  returning RC_OK.  The resume key the Spacebank was called with is
 *  held in KR_RETURN.
 *
 */
 
int
ProcessRequest(Message *argmsg)
{
  uint32_t result = RC_OK;
  uint32_t code = argmsg->rcv_code;
  
  Bank *bank = BankFromInvocation(argmsg);
  BankPrecludes preclude = PrecludesFromInvocation(argmsg);
    
  uint32_t count = 0;

  argmsg->snd_len = 0;
  argmsg->snd_w1 = 0;
  argmsg->snd_w2 = 0;
  argmsg->snd_w3 = 0;
  argmsg->snd_key0 = KR_VOID;
  argmsg->snd_key1 = KR_VOID;
  argmsg->snd_key2 = KR_VOID;
  argmsg->snd_code = RC_OK;
  
#ifdef PARANOID
  if (preclude > BANKPREC_MASK) {
    kpanic(KR_OSTREAM, "spacebank: bad preclude 0x%08x\n", preclude);
    return 1;
  }
#endif
  
  switch (code) {
    /* ALLOCATIONS */
  case OC_SpaceBank_Alloc3DataPages:
  case OC_SpaceBank_Alloc3Nodes:
    argmsg->snd_key2 = KR_ARG2;
    count ++;
    /* fall through */
  case OC_SpaceBank_Alloc2DataPages:
  case OC_SpaceBank_Alloc2Nodes:
    argmsg->snd_key1 = KR_ARG1;
    count ++;
    /* fall through */
  case OC_SpaceBank_Alloc1DataPage:
  case OC_SpaceBank_Alloc1Node:
    {      
      uint8_t type = request_ob_type[code];

      count ++;

      argmsg->snd_key0 = KR_ARG0;
      result = BankAllocObjects(bank, type, count, KR_ARG0);

      if (result != RC_OK) {
	argmsg->snd_key0 = KR_VOID;
	argmsg->snd_key1 = KR_VOID;
	argmsg->snd_key2 = KR_VOID;
      }

      argmsg->snd_code = result;

      DEBUG(realloc) {
	if ( ((bank->allocs[eros_Range_otPage] % 20) == 0) ||
	     ((bank->deallocs[eros_Range_otPage] % 20) == 0) )
	  kprintf(KR_OSTREAM, "*%d pages allocd, %d deallocd\n",
		  bank->allocs[eros_Range_otPage],
		  bank->deallocs[eros_Range_otPage]
		  );
      }
      break;
    }

  /* DEALLOCATIONS */
  case OC_SpaceBank_Reclaim3DataPages:
  case OC_SpaceBank_Reclaim3Nodes:
    count++;
    /* fall through */
  case OC_SpaceBank_Reclaim2DataPages:
  case OC_SpaceBank_Reclaim2Nodes:
    count++;
    /* fall through */
  case OC_SpaceBank_Reclaim1DataPage:
  case OC_SpaceBank_Reclaim1Node:
    {
      uint8_t type = request_ob_type[code];

      count++;

      result = BankDeallocObjects(bank, type, count, KR_ARG0);

      if (result != RC_OK) {
	DEBUG(dealloc)
	  kdprintf(KR_OSTREAM, "Spacebank: dealloc of %s failed (0x%1x)\n",
		   type_name(type),
		   result);
	argmsg->snd_code = result;
      }

      break;
    }

  case OC_SpaceBank_ReclaimDataPagesFromNode:
  case OC_SpaceBank_ReclaimNodesFromNode:
    {
      uint32_t type;
      
      /* verify that they actually passed us a node key */
      if (eros_Range_identify(KR_SRANGE, KR_ARG0, &type, NULL) != RC_OK
          || type != eros_Range_otNode) {
        argmsg->snd_code = RC_eros_key_RequestError;
        break;
      } else {
	/* it's a node */
	uint32_t slot;
	uint32_t mask = 0;
	
	for (slot = 0; slot < EROS_NODE_SIZE; slot++) {
	  uint32_t result;
	
	  result = node_copy(KR_ARG0, slot, KR_ARG1);

	  if (result != RC_OK) {
	    DEBUG(dealloc)
	      kdprintf(KR_OSTREAM,
		       "Spacebank: copy of %s from slot %d failed (0x%1x)\n",
		       type_name(type),
		       slot,
		       result);
	    mask |= (1u << slot);
	    continue;
	  }
	  result = BankDeallocObjects(bank, type, 1, KR_ARG1);

	  if (result != RC_OK) {
	    DEBUG(dealloc)
	      kdprintf(KR_OSTREAM,
		       "Spacebank: dealloc of %s in slot %d failed (0x%1x)\n",
		       type_name(type),
		       slot,
		       result);
	    mask |= (1u << slot);
	    continue;
	  }
	}
      
	argmsg->snd_code = result;
	break;
      }
    }
  /* IDENTIFICATIONS */
  case OC_SpaceBank_Identify3DataPages:
  case OC_SpaceBank_Identify3Nodes:
    count++;
    /* fall through */
  case OC_SpaceBank_Identify2DataPages:
  case OC_SpaceBank_Identify2Nodes:
    count++;
    /* fall through */
  case OC_SpaceBank_Identify1DataPage:
  case OC_SpaceBank_Identify1Node:
    {
      uint8_t type = request_ob_type[code];

      count++;
      result = BankIdentifyObjects(bank, type, count, KR_ARG0);

      if (result != RC_OK) {
	argmsg->snd_code = result;
      }

      break;
    }
  case OC_SpaceBank_SeverDataPage:
  case OC_SpaceBank_SeverNode:
    {
#if 0
      uint8_t type = request_ob_type[code];
#endif

      /* not implemented yet */
      argmsg->snd_code = RC_eros_key_UnknownRequest;
      break;
    }

  case OC_SpaceBank_Preclude:
    {
      preclude |= argmsg->rcv_w3;
      if (preclude > BANKPREC_MASK) {
	argmsg->snd_code = RC_eros_key_RequestError;
	break;
      }

      result = BankCreateKey(bank, preclude, KR_ARG0);
      if (result == RC_OK) argmsg->snd_key0 = KR_ARG0;
      break;
    }

  case OC_eros_key_destroy:
    {
      /* destroy bank, returning space to parent */

      if (BANKPREC_CAN_DESTROY(preclude))
	argmsg->snd_code = BankDestroyBankAndStorage(bank, false);
      else
	argmsg->snd_code = RC_eros_key_UnknownRequest;

      break;
    }


  case OC_SpaceBank_DestroyBankAndSpaces:
    {
      /* destroy bank, deallocating space */

      if (BANKPREC_CAN_DESTROY(preclude))
	argmsg->snd_code = BankDestroyBankAndStorage(bank, true);
      else
	argmsg->snd_code = RC_eros_key_UnknownRequest;

      break;
    } 
      
  case OC_SpaceBank_SetLimits:
    {
      fixreg_t got = min(argmsg->rcv_limit, argmsg->rcv_sent);

      if ( !BANKPREC_CAN_MOD_LIMIT(preclude) ) 
	argmsg->snd_code = RC_eros_key_UnknownRequest;
      else if (got != sizeof(struct bank_limits))
	argmsg->snd_code = RC_eros_key_RequestError;
      else {
	struct bank_limits *limPtr = (struct bank_limits *)argmsg->rcv_data;

	argmsg->snd_code = BankSetLimits(bank, limPtr);
      }

      break;
    }

  case OC_SpaceBank_GetLimits:
    {
      static struct bank_limits retLimits;
      /* static so it survives the return */

      /* FIXME: can this be precluded? */
      argmsg->snd_code = BankGetLimits(bank, &retLimits);

      if (argmsg->snd_code == RC_OK) {
	argmsg->snd_len = sizeof(retLimits);
	argmsg->snd_data = (void *)&retLimits;
      }

      break;
    }

  case OC_SpaceBank_CreateChild:
    {
      argmsg->snd_code = BankCreateChild(bank, KR_ARG0);
      if (argmsg->snd_code == RC_OK) argmsg->snd_key0 = KR_ARG0;
      
      break;
    } 

  case OC_SpaceBank_VerifyBank:
    {
      uint32_t brandMatches;
      
      /* verify that KR_ARG0 is a key to a valid spacebank */

      /* use my domain key to create the brand key */
      result = process_make_start_key(KR_SELF,
				     SB_BRAND_KEYDATA,
				     KR_TMP);

      if (result != RC_OK) {
	kpanic(KR_OSTREAM,
	       "SpaceBank: VerifyBank failed to create brand key!\n");
      }

      /* get the DomainTool key */
      node_copy(KR_CONSTIT, KC_DOMTOOL, KR_TMP2);
      
      /* use it to replace the brand key with the node key from
       * KR_ARG0 (assuming KR_ARG0 is a valid spacebank key)
       */
      result = eros_ProcTool_identWrapperKeeper(KR_TMP2, KR_ARG0, KR_TMP, KR_TMP, 
				  &brandMatches, 0);
      if (result != RC_OK) {
	kpanic(KR_OSTREAM,
	       "SpaceBank: IdentSegKpr failed to match brand key! (0x%08x, %d)\n", result, brandMatches);
      }

      /* Return RC_OK if the operation succeeded (i.e. this is a valid
       * segment key), -1 otherwise
       */

      argmsg->snd_code = RC_OK;
      argmsg->snd_w1 = (result == RC_OK && brandMatches) ? 1 : 0;
      
      break;
    }
  case OC_eros_key_getType: /* Key type */
    {
      argmsg->snd_code = RC_OK;
      argmsg->snd_w1 = AKT_SpaceBank;
      break;
    }
  default:
    {
      argmsg->snd_code = RC_eros_key_UnknownRequest;
      break;
    }
  }

  return 1;
}

void
InitSpaceBank(void)
{
  int x;
  
  DEBUG(init) kdprintf(KR_OSTREAM,
	   "spacebank: spacebank initializing\n");

  for (x = 0; x < eros_Range_otNUM_TYPES; x++) {
    objects_per_frame[x] = 0u;
    objects_map_mask[x] = 0u;
    type_names[x] = "Invalid";
  }


#define SETUP_TYPE(type,perpage) \
                             { \
			       objects_per_frame[eros_Range_ot ## type] = perpage; \
			       objects_map_mask[eros_Range_ot ## type] = \
 					              (1u << (perpage)) - 1; \
			       type_names[eros_Range_ot ## type] = #type; \
			     }

  SETUP_TYPE(Page, 1u);
  SETUP_TYPE(Node, DISK_NODES_PER_PAGE);
#undef SETUP_TYPE

#if 0
  /* setup dummy types for printing */
  SETUP_TYPE(UNKNOWN, 0);
  SETUP_TYPE(INVALID, 0);
#endif

  DEBUG(init) kdprintf(KR_OSTREAM, "Initializing banks\n");

  bank_init();

  DEBUG(init) kdprintf(KR_OSTREAM, "Initializing ObjectSpace\n");

  ob_init();			/* init master object space */
  /* ^^^ also calls bank_init(); */

  return; /* done */
}

const char *
type_name(int t)
{
  switch (t) {
  case eros_Range_otPage:
  case eros_Range_otNode:
    return type_names[t];
  default:
    return "unknown";
  }
}

bool
valid_type(int t)
{
  switch (t) {
  case eros_Range_otPage:
  case eros_Range_otNode:
    return true;
  default:
    return false;
  }
}

