/*
 * Copyright (C) 1998, 1999, 2001, Jonathan S. Shapiro.
 * Copyright (C) 2006, 2007, Strawberry Development Group.
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

/* Process Creator

   Process Creators are fabricated by DCC.  DCC builds them a segment
   with code and a stack, if possible out of a single page (depends on
   the architecture).
   
   The structure of a process depends on the target architcture.  At
   the moment, we know about five distinct process structures:


       x86        Root + keys node, excludes FP regs.
       RISC32     Root + keys node + annex node, includes FP regs.
       RISC64     Root + keys node + annex node, includes FP regs.
       SPARC32    Root + keys node + 3 annex nodes, +1 for FP regs.
       SPARC64    Root + keys node + 4 annex nodes, includes
                  supports only 5 register windows.  This needs
		  rethinking.
   

   All of these are fairly simple to build.  My guess is that domcre
   could be made to run in a single page on all architectures.  The
   present design merely assumes that domcre fits in less than 15
   pages, which is quite generous.

   Note that domcre has no writable data.  It isn't necessary, and
   it's absence considerably simplifies the construction of the process.
   */

#include <stddef.h>
#include <eros/target.h>
#include <eros/Invoke.h>
#include <eros/StdKeyType.h>
#include <disk/DiskNodeStruct.h>

#include <idl/capros/key.h>
#include <idl/capros/ProcTool.h>
#include <idl/capros/Number.h>
#include <idl/capros/GPT.h>
#include <idl/capros/SpaceBank.h>
#include <idl/capros/Node.h>
#include <idl/capros/Process.h>
#include <idl/capros/ProcCre.h>

#include <domain/domdbg.h>
#include <domain/Runtime.h>
#include "constituents.h"

uint32_t __rt_unkept = 1;

#define DEBUG if (0)
/* #define DEBUG if (1) */

#define KR_OSTREAM    KR_APP(0)
#define KR_SCRATCH0   KR_APP(1)	/* from the space bank */
#define KR_SCRATCH1   KR_APP(2)
#define KR_SCRATCH2   KR_APP(3)	/* Where we put the components we buy */
#define KR_OUTKEY0    KR_APP(4)
#define KR_OURBRAND   KR_APP(5)	/* A 65536 start key to us */
#define KR_OUTKEY1    KR_APP(6)	/* where the 2nd created key (if any) goes */
#define KR_PROCTOOL   KR_APP(7)	/* The process tool. */

#define KR_ARG0    KR_ARG(0)
#define KR_ARG1    KR_ARG(1)
#define KR_ARG2    KR_ARG(2)


#define FALSE 0
#define TRUE 0

uint32_t destroy_process(uint32_t bankKeyReg);
uint32_t amplify_gate_key(uint32_t krStart, uint32_t krTo,
			  uint32_t *capType, uint32_t *capInfo);
uint32_t amplify_segment_key(uint32_t krStart, uint32_t krTo,
			  uint32_t *capType, uint32_t *capInfo);
int is_our_progeny(uint32_t krGate, uint32_t krNode);

#include "create_new_process.h"

/* On entry,  KR_ARG[013] hold the respective arguments.
   The entry block contains  KR_ARG3:0:KR_ARG1:KR_ARG0,
      0 len.
      
   The exit block contains one of:
       0:0:KR_OUTKEY1:KR_OUTKEY0, (uint8_t *) 0, and a 0 length
       0:0:0:KR_OUTKEY0, (uint8_t *) 0, and a 0 length
   */

bool
ProcessRequest(Message *argmsg)
{
  uint32_t result = RC_OK;

  /* Reset receive args for next invocation: */
  argmsg->snd_key0 = 0;		/* until proven otherwise */
  argmsg->snd_key1 = 0;		/* until proven otherwise */

  switch (argmsg->rcv_code) {
  case OC_capros_ProcCre_createProcess:
    argmsg->snd_key0 = KR_OUTKEY0;
    result = create_new_process(KR_ARG0, KR_OUTKEY0);
    break;
    
  case OC_capros_ProcCre_destroyProcess:
    if (capros_ProcTool_identifyProcess(KR_PROCTOOL,
          KR_ARG1, KR_OURBRAND, KR_SCRATCH0) != RC_OK)
      result = RC_capros_ProcCre_Paternity;
    else
      result = destroy_process(KR_ARG0);
    break;

  case OC_capros_ProcCre_destroyCallerAndReturn:
    {
      if (! is_our_progeny(KR_RETURN, KR_SCRATCH0)) {
        // FIXME: should not accept a start key
        result = RC_capros_ProcCre_Paternity;
        break;
      }

      result = destroy_process(KR_ARG0);
      if (result != RC_OK)
	break;

      argmsg->snd_invKey = KR_ARG1;	// return to ARG1
      result = argmsg->rcv_w1;		// with this return code
      break;
    }
    
  case OC_capros_ProcCre_reduce:
    {
      capros_Process_makeStartKey(KR_SELF, 1, KR_OUTKEY0);
      argmsg->snd_key0 = KR_OUTKEY0;
      result = RC_OK;
      break;
   }
    
  case OC_capros_ProcCre_amplifyGateKey:
    {
      uint32_t capType;
      uint32_t capInfo;

      result = amplify_gate_key(KR_ARG0, KR_OUTKEY0, &capType, &capInfo);

      argmsg->snd_key0 = KR_OUTKEY0;
      argmsg->snd_w1 = capType;
      argmsg->snd_w2 = capInfo;
      break;
    }
    
  case OC_capros_ProcCre_amplifyGPTKey:	// really AmplifyGPTKey
    {
      uint32_t capType;
      uint32_t capInfo;

      result = amplify_segment_key(KR_ARG0, KR_OUTKEY0, &capType, &capInfo);
      argmsg->snd_key0 = KR_OUTKEY0;
      argmsg->snd_w1 = capType;
      argmsg->snd_w2 = capInfo;
      break;
    }
    
  case OC_capros_key_getType:
    argmsg->snd_w1 = AKT_DomCre;
    break;
    
  case OC_capros_key_destroy:
    return false;
    
  default:
    result = RC_capros_key_UnknownRequest;
    break;
  };

  argmsg->snd_code = result;
  return true;
}

void
init_domcre()
{
  capros_Node_getSlot(KR_CONSTIT, KC_OSTREAM, KR_OSTREAM);
  capros_Node_getSlot(KR_CONSTIT, KC_DOMTOOL, KR_PROCTOOL);

  /* Fabricate the key that we will use for a brand key. */
  (void) capros_Process_makeStartKey(KR_SELF, 65535, KR_OURBRAND);
}

int
main()
{
  Message msg;

  init_domcre();

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
  msg.rcv_rsmkey = KR_RETURN;
  msg.rcv_limit = 0;
  msg.rcv_code = 0;
  msg.rcv_w1 = 0;
  msg.rcv_w2 = 0;
  msg.rcv_w3 = 0;

  do {
    RETURN(&msg);
    msg.snd_invKey = KR_RETURN;
  } while ( ProcessRequest(&msg) );

  return 0;
}

int
is_our_progeny(uint32_t krGate, uint32_t krNode)
{
  uint32_t capType;
  uint32_t result = capros_ProcTool_canOpener(KR_PROCTOOL, krGate, KR_OURBRAND,
					    krNode, &capType, 0);
  
  if (result == RC_OK && capType != 0)
    return 1;
  return 0;
}

/* Verify bank, and destroy the process whose root is in KR_SCRATCH0. */
uint32_t
destroy_process(uint32_t krBank)
{
  uint32_t isGood;

  DEBUG kdprintf(KR_OSTREAM, "About to destroy process, bank %d...\n",
		 krBank);

  if (capros_SpaceBank_verify(KR_BANK, krBank, &isGood) != RC_OK ||
      isGood == 0)
    return RC_capros_key_BadBank;
  
  /* Extract the annex node(s). */

  (void) capros_Node_getSlot(KR_SCRATCH0, ProcGenKeys, KR_SCRATCH1);
  
  if (capros_SpaceBank_free2(krBank, KR_SCRATCH0, KR_SCRATCH1) != RC_OK)
    return RC_capros_ProcCre_WrongBank;
  
  return RC_OK;
}

uint32_t
amplify_gate_key(uint32_t krStart, uint32_t krTo, uint32_t *capType,
		 uint32_t *capInfo)
{
  uint32_t result = 
    capros_ProcTool_canOpener(KR_PROCTOOL, krStart, KR_OURBRAND,
			    krTo, capType, capInfo);

  if (result != RC_OK)
    return result;

  (void) capros_ProcTool_makeProcess(KR_PROCTOOL, krTo, krTo);
  return result;
}

uint32_t
amplify_segment_key(uint32_t krSeg, uint32_t krTo, uint32_t *capType,
		    uint32_t *capInfo)
{
  uint32_t result = 
    capros_ProcTool_identGPTKeeper(KR_PROCTOOL, krSeg, KR_OURBRAND,
				     krTo, capType, capInfo);

  if (result != RC_OK)
    return result;
  (*capType)++;

  // krTo has a non-opaque GPT key.
  capros_GPT_getSlot(krTo, capros_GPT_keeperSlot, krTo);
  return amplify_gate_key(krTo, krTo, capType, capInfo);
}

