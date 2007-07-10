/*
 * Copyright (C) 2002, Jonathan S. Shapiro.
 *
 * This file is part of the EROS Operating System distribution.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, 59 Temple Place - Suite 330 Boston, MA 02111-1307, USA.
 */

/* Stream interface
 * 3-way threaded.
 * Main thread - Stream interface front end which handles stream requests
 *               & redirects calls appropriately onto in/out backends
 * ConsoleIn   - Thread which handles getchar operations
 * ConsoleOut  - Thread which handles putchar operations
 */

#include <stddef.h>
#include <eros/target.h>
#include <eros/Invoke.h>
#include <eros/NodeKey.h>
#include <eros/ProcessKey.h>
#include <eros/KeyConst.h>
#include <eros/ProcessState.h>
#include <eros/cap-instr.h>

#include <domain/domdbg.h>
#include <domain/ConstructorKey.h>
#include <domain/Runtime.h>
#include <domain/ProcessCreatorKey.h>

#include <idl/capros/key.h>
#include <idl/capros/Stream.h>
#include <idl/capros/Ps2.h>
#include <idl/capros/Sleep.h>

#include <idl/console/textconsole.h>
#include <idl/console/keyclient.h>

#include "constituents.h"

#include <ethread/ethread.h>

#define KR_DOMCRE      3

#define KR_OSTREAM     KR_APP(0)
#define KR_START       KR_APP(1)
#define KR_TEXCON_S    KR_APP(2)
#define KR_KEYCLI_C    KR_APP(3)
#define KR_KEYCLI_S    KR_APP(4)
#define KR_MCLI_C      KR_APP(5)
#define KR_MCLI_S      KR_APP(6)
#define KR_PS2READER_C KR_APP(7)
#define KR_PS2READER_S KR_APP(8)
#define KR_CHARSRC     KR_APP(9)
#define KR_SRCSTART    KR_APP(10)
#define KR_CHARSINK    KR_APP(11)
#define KR_SINKSTART   KR_APP(12)
#define KR_SRCFAULT    KR_APP(13)
#define KR_SINKFAULT   KR_APP(14)

#define KR_SCRATCH     KR_APP(20)

/* Functions prototypes */
void redirect(Message *msg,int,uint32_t);
void console_in(void);
void console_out(void);

void 
provide_start_key(cap_t krProc,cap_t start)
{
  Message msg;
  
  msg.snd_invKey = krProc;  
  msg.snd_key0 = start;
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
  msg.rcv_rsmkey = KR_VOID;
  msg.rcv_data = 0;
  msg.rcv_limit = 0;
  msg.rcv_code = 0;
  msg.rcv_w1 = 0;
  msg.rcv_w2 = 0;
  msg.rcv_w3 = 0;
  
  CALL(&msg);
}

int
ProcessConsoleInRequest(Message *msg) 
{
  msg->snd_len = 0;
  msg->snd_key0 = KR_VOID;
  msg->snd_key1 = KR_VOID;
  msg->snd_key2 = KR_VOID;
  msg->snd_rsmkey = KR_RETURN;
  msg->snd_code = RC_OK;
  msg->snd_w1 = 0;
  msg->snd_w2 = 0;
  msg->snd_w3 = 0;
  msg->snd_invKey = KR_RETURN;
  
  switch (msg->rcv_code) {
  case OC_capros_Stream_read:
    {
      char ch;
      
      /* Get ASCII character from the keyclient */
      (void)console_keyclient_getChar(KR_KEYCLI_S,&ch);
      
      msg->snd_w1 = ch;
      return 1;
    }
  case OC_capros_Stream_nread:
    {
      /* Stream nread called */
      
      return 1;
    }
  default:
    kprintf(KR_OSTREAM,"ConsoleIn default");
    break;
  }

  msg->snd_code = RC_capros_key_UnknownRequest;
  return 1;
}

int
ProcessConsoleOutRequest(Message *msg) 
{
  msg->snd_len = 0;
  msg->snd_key0 = KR_VOID;
  msg->snd_key1 = KR_VOID;
  msg->snd_key2 = KR_VOID;
  msg->snd_rsmkey = KR_RETURN;
  msg->snd_code = RC_OK;
  msg->snd_w1 = 0;
  msg->snd_w2 = 0;
  msg->snd_w3 = 0;
  msg->snd_invKey = KR_RETURN;
  
  switch (msg->rcv_code) {
  case OC_capros_Stream_write:
    {
      (void)console_textconsole_putChar(KR_TEXCON_S,msg->rcv_w1);
      return 1;
    }
    /*
      case OC_capros_Stream_nwrite:
      {
      kprintf(KR_OSTREAM,"Strean. nwrite called");
      return 1;
      }
    */
  default:
    kprintf(KR_OSTREAM,"ConsoleOut default");
    break;
  }

  msg->snd_code = RC_capros_key_UnknownRequest;
  return 1;
}


/* Characters come in from here i.e the bridge to the ps2 reader */
void 
console_in()
{
  Message msg;
  uint32_t result;
  
  kprintf(KR_OSTREAM,"Into console in");

  //Construct the ps2reader 
  result = constructor_request(KR_PS2READER_C, KR_BANK, KR_SCHED, KR_VOID,
                               KR_PS2READER_S);
  if (result != RC_OK) 
    kprintf(KR_OSTREAM, "ConsoleIn:Constructing PS2READER...[FAILED]\n");
 
  // Initialise the ps2 driver 
  result = capros_Ps2_initPs2(KR_PS2READER_S);
  if(result!=RC_OK) {
    kprintf(KR_OSTREAM,"ConsoleIn:: Starting ps2reader...[Failed]");
    kprintf(KR_OSTREAM,"Error Code returned %d",result);
  }
  
  //Construct the keyclient 
  result = constructor_request(KR_KEYCLI_C, KR_BANK, KR_SCHED, KR_PS2READER_S,
                               KR_KEYCLI_S);
  if (result != RC_OK) {
    kprintf(KR_OSTREAM, "ConsoleIn:Constructing Keyclient...[FAILED]\n");
  }
    
  //Construct the mouseclient & forget about it
  result = constructor_request(KR_MCLI_C, KR_BANK, KR_SCHED, KR_PS2READER_S,
                               KR_MCLI_S);
  if (result != RC_OK) {
    kprintf(KR_OSTREAM, "ConsoleIn:Constructing mouseclient...[FAILED]\n");
  }
  
  //Return to null and wait for events
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
    
  kprintf(KR_OSTREAM, "ConsoleIn is now available...\n");

  do {
    RETURN(&msg);
    msg.rcv_rsmkey = KR_RETURN;
    msg.snd_invKey = KR_RETURN;
  }while(ProcessConsoleInRequest(&msg));
    
  return;
}


/* Characters out to the textconsole */
void 
console_out()
{
  Message msg;
  
  kprintf(KR_OSTREAM, "ConsoleOut clearing the screen...\n");

  // Clear the screen
  console_textconsole_clearScreen(KR_TEXCON_S);
  
  //Return to null and wait for events
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
  
  kprintf(KR_OSTREAM, "ConsoleOut is now available...\n");

  do {
    RETURN(&msg);
    msg.rcv_rsmkey = KR_RETURN;
    msg.snd_invKey = KR_RETURN;
  }while(ProcessConsoleOutRequest(&msg));
  
   
  return;
}

/* Redirect the call to keep our stream interface available forever */
void
redirect(Message *msg,int key,uint32_t snd_code) {
  msg->snd_key0 = key;
  msg->snd_code = snd_code;
  msg->snd_w1 = RETRY_SET_LIK;
  msg->invType = IT_Retry;
}


int
ProcessRequest(Message *msg) 
{
  msg->snd_len = 0;
  msg->snd_key0 = KR_VOID;
  msg->snd_key1 = KR_VOID;
  msg->snd_key2 = KR_VOID;
  msg->snd_rsmkey = KR_RETURN;
  msg->snd_code = RC_OK;
  msg->snd_w1 = 0;
  msg->snd_w2 = 0;
  msg->snd_w3 = 0;
  msg->snd_invKey = KR_RETURN;
  
  switch (msg->rcv_code) {
  case OC_capros_Stream_write:
    {
      /* Redirect using RETRY onto console out */
      redirect(msg,KR_SINKSTART,OC_capros_Stream_write);
      
      return 1;
    }
  case OC_capros_Stream_read:
    {
      /* Redirect using RETRY onto console in */
      redirect(msg,KR_SRCSTART,OC_capros_Stream_read);
      
      return 1;
    }
  case OC_capros_Stream_nread:
    {
      redirect(msg,KR_SRCSTART,OC_capros_Stream_nread);
    }
  default:
    kprintf(KR_OSTREAM,"console streamdefault");
    break;
  }

  msg->snd_code = RC_capros_key_UnknownRequest;
  return 1;
}

int
main(void)
{
  Message msg;
    
  node_extended_copy(KR_CONSTIT,KC_OSTREAM,KR_OSTREAM);
  node_extended_copy(KR_CONSTIT,KC_TEXCON_S,KR_TEXCON_S);
  node_extended_copy(KR_CONSTIT,KC_KEYCLI_C,KR_KEYCLI_C);
  node_extended_copy(KR_CONSTIT,KC_MCLI_C,KR_MCLI_C);
  node_extended_copy(KR_CONSTIT,KC_PS2READER_C,KR_PS2READER_C);
    
  process_make_start_key(KR_SELF, 0, KR_START);
  
  /* make a thread for the console in */
  {
    result_t result = ethread_new_thread(KR_BANK, KR_SCRATCH, 4000,
					 (uint32_t)&console_in, KR_CHARSRC);
    if (result != RC_OK) {
      kprintf(KR_OSTREAM, "** ERROR: consolestream unable to create "
	      "first thread (rc = 0x%08x)\n", result);
      return -1;
    }
  }
  COPY_KEYREG(KR_CHARSRC, KR_SRCSTART);
  
  /* make a thread for console out */
  {
    result_t result = ethread_new_thread(KR_BANK, KR_SCRATCH, 4000,
					 (uint32_t)&console_out, KR_CHARSINK);
    if (result != RC_OK) {
      kprintf(KR_OSTREAM, "** ERROR: consolestream unable to create "
	      "second thread (rc = 0x%08x)\n", result);
      return -1;
    }
  }
  COPY_KEYREG(KR_CHARSINK, KR_SINKSTART);
  
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
  msg.invType = IT_PReturn;
  
  kprintf(KR_OSTREAM, "ConsoleStream is now available ...\n");

  do {
    INVOKECAP(&msg);
    msg.snd_key0 = KR_VOID;
    msg.snd_key0 = KR_VOID;
    msg.rcv_rsmkey = KR_RETURN;
    msg.invType = IT_PReturn;
  }while(ProcessRequest(&msg));

  return 0;
}
