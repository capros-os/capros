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

/* Keyboard Client to the ps2 driver(keyb). This process gets keyboard
 * scan codes from the ps2 driver, translates them to ASCII and calls 
 * its builder, passing(queuing?) these KeyEvents */
 
#include <eros/target.h>
#include <eros/StdKeyType.h>
#include <eros/ProcessState.h>
#include <eros/NodeKey.h>
#include <eros/KeyConst.h>
#include <eros/ProcessKey.h>
#include <eros/Invoke.h>

#include <idl/eros/key.h>

#include <domain/ConstructorKey.h>
#include <domain/domdbg.h>
#include <domain/ProcessCreatorKey.h>
#include <domain/Runtime.h>
#include <domain/drivers/ps2.h>

#include "constituents.h"
#include "keytrans.h"

#define KR_OSTREAM  KR_APP(0)
#define KR_KEYB_C   KR_APP(1)
#define KR_KEYB_S   KR_APP(2)
#define KR_MCLI_C   KR_APP(3)
#define KR_MCLI_S   KR_APP(4)
#define KR_START    KR_APP(5)
#define KR_BUILDER  KR_APP(6)
#define KR_SCRATCH  KR_APP(7)

/* function declarations */
unsigned char getChar(void);
int getstate(int scan, int *status);
int initKbdDrvr(void);
int ProcessRequest();
int ProcessKeys(Message *msg);
void updateLeds(int *status);

int
main(void) 
{
  Message msg;
  uint32_t result;

  node_extended_copy(KR_CONSTIT, KC_KEYB, KR_KEYB_C);
  node_extended_copy(KR_CONSTIT, KC_MCLI, KR_MCLI_C);
  node_extended_copy(KR_CONSTIT, KC_OSTREAM, KR_OSTREAM);
  
  process_copy(KR_SELF, ProcSched, KR_SCHED);
  
  result = constructor_request(KR_KEYB_C,KR_BANK,KR_SCHED,KR_VOID,KR_KEYB_S);
#ifdef VERBOSE
  if (result != RC_OK) {
    kprintf(KR_OSTREAM, "KeyClient failed to start (keyb).\n");
  }
#endif
    
  /* Construct the mouse client and give it the start key to keyb */
  result = constructor_request(KR_MCLI_C,KR_BANK,KR_SCHED,KR_VOID,KR_MCLI_S);
#ifdef VERBOSE
  if (result != RC_OK) {
    kprintf(KR_OSTREAM, "KeyClient failed to start mouseclient.\n");
  }
#endif
  
  /* Initialise the ps2 driver */
  result = initKbdDrvr();
#ifdef VERBOSE
  if(result!=RC_OK) {
    kprintf(KR_OSTREAM,"Keyclient:: Starting keyb...[Failed]");
    kprintf(KR_OSTREAM,"Error Code returned %d",result);
  }
#endif

  /* Pass the mouseclient the start key to keyb */
  msg.snd_invKey = KR_MCLI_S;
  msg.snd_code   = OC_keyb_key;
  msg.snd_key0   = KR_VOID; 
  msg.snd_key1   = KR_KEYB_S;
  msg.snd_key2   = KR_VOID; 
  msg.snd_rsmkey = KR_VOID;
  msg.snd_data = 0;
  msg.snd_len  = 0;
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
  
  /* We are done with all the initial setup and will now return
   * our start key. */
  result = process_make_start_key(KR_SELF,1, KR_START);
  
  msg.snd_invKey = KR_RETURN;
  msg.snd_key0   = KR_START;
  msg.snd_key1   = KR_VOID;
  msg.snd_key2   = KR_VOID;
  msg.snd_rsmkey = KR_RETURN;
  msg.snd_data = 0;
  msg.snd_len  = 0;
  msg.snd_code = 0;
  msg.snd_w1 = 0;
  msg.snd_w2 = 0;
  msg.snd_w3 = 0;

  msg.rcv_key0   = KR_VOID;
  msg.rcv_key1   = KR_SCRATCH;
  msg.rcv_key2   = KR_VOID;
  msg.rcv_rsmkey = KR_RETURN;
  msg.rcv_data = 0;
  msg.rcv_limit = 0;
  msg.rcv_code = 0;
  msg.rcv_w1 = 0;
  msg.rcv_w2 = 0;
  msg.rcv_w3 = 0;

  do {
    RETURN(&msg);
    msg.snd_invKey = KR_RETURN;
    msg.snd_key0 = KR_VOID;
    msg.snd_rsmkey = KR_RETURN;
  } while (ProcessKeys(&msg));
  
  
  /* Send back to our builder to get it running. Don't RETURN as
   * we need to run too */
  SEND(&msg);
  
  /* Infinitely loop getting char from keyb and 
   * passing it to (queuing it up at) the builder's */
  do {
  }while(ProcessRequest());
  
  return 0;
}

/* Get char from the ps2 driver (keyb). ASCII Characters may 
 * comprise of multiple scancodes */
unsigned char
getChar(void) 
{
  static int state = 0;
  Message ms;
  int data, i, prevcode, scode;
  unsigned char kcode;
  
  kcode = 0;
  prevcode = 0;

  /* Call keyb */
  ms.snd_invKey = KR_KEYB_S;
  ms.snd_key0   = KR_VOID;
  ms.snd_key1   = KR_VOID;
  ms.snd_key2   = KR_VOID;
  ms.snd_rsmkey = KR_VOID;
  ms.snd_data = 0;
  ms.snd_len  = 0;
  ms.snd_code = 0;
  ms.snd_w2 = 0;
  ms.snd_w3 = 0;

  ms.rcv_key0 = KR_VOID;
  ms.rcv_key1 = KR_VOID;
  ms.rcv_key2 = KR_VOID;
  ms.rcv_rsmkey = KR_VOID;
  ms.rcv_code = 0;
  ms.rcv_w1   = 0;
  ms.rcv_w2   = 0;
  ms.rcv_w3   = 0;

  while (kcode == 0) {
    ms.snd_code = OC_read_keyb;
    ms.snd_w1 = KEYCLREAD;
    CALL(&ms);
    data = ms.rcv_w1;
    
    while(data > -1) { 
      do {
	scode = scanToKey(data, &prevcode, state);
      } while (prevcode == -1);
      
      if ((updShiftState(scode, data & 0x80, &state)) == 0) {
	i = getstate(scode, &state);
	kcode = key_map.key[scode].map[i];
      }
      if (state & LED_UPDATE) {
	updateLeds(&state);
      }
      ms.snd_w1 = KEYCLREAD;
      ms.snd_code = OC_read_keyb;
      CALL(&ms);
      data = ms.rcv_w1;
    }
  }
  //kprintf(KR_OSTREAM,"Returning kcode = %d",kcode);
  return kcode;
}

int
getstate(int scan, int *status) 
{
  int i, state;
  state = *status;

  i = ((state & SHIFTS) ? 1 : 0)
    | ((state & CTLS) ? 2 : 0)
    | ((state & ALTS) ? 4 : 0);

  /* only upper case on letter keys */  
  if (state & CLKED) {
    if ((scan >= 0x10) && (scan <= 0x19)) {
      if (state & SHIFTS) {
	i &= ~1;
      }
      else {
	i |= 1;
      }
    }
    else if ((scan >= 0x1E) && (scan <= 0x26)) {
      if (state & SHIFTS) {
	i &= ~1;
      }
      else {
	i |= 1;
      }
    }
    else if ((scan >= 0x2C) && (scan <= 0x32)) {
      if (state & SHIFTS) {
	i &= ~1;
      }
      else {
	i |= 1;
      }
    }
  }

  /* get keypad keys when numlocked */
  if (state & NLKED) {
    if ((scan >= 0x47) && (scan <= 0x53)) {
      if (state & SHIFTS) {
	i &= ~1;
      }
      else {
	i |= 1;
      }
    }
  }

  return i;
}

/* Initialize the ps2 driver */
int
initKbdDrvr(void) 
{
  int resp = RC_OK;
  Message ms;

  ms.snd_invKey = KR_KEYB_S;
  ms.snd_key0   = KR_VOID;
  ms.snd_key1   = KR_VOID;
  ms.snd_key2   = KR_VOID;
  ms.snd_rsmkey = KR_VOID;
  ms.snd_data = 0;
  ms.snd_len  = 0;
  ms.snd_code = OC_init_keyb;
  ms.snd_w1 = 0;
  ms.snd_w2 = 0;
  ms.snd_w3 = 0;
 
  ms.rcv_key0   = KR_VOID;
  ms.rcv_key1   = KR_VOID;
  ms.rcv_key2   = KR_VOID;
  ms.rcv_rsmkey = KR_VOID;
  ms.rcv_data = 0;
  ms.rcv_limit = 0;
  ms.rcv_code = 0;
  ms.rcv_w1 = 0;
  ms.rcv_w2 = 0;
  ms.rcv_w3 = 0;
  CALL(&ms);
  
  return resp;
}


/* Update the LEDs incase of Caps, scroll,Num lock */
void
updateLeds(int *status) 
{
  Message msg;
  int state = *status;

  /* clear update flag */
  state &= ~LED_UPDATE;
  *status = state;

  /* clear unnecessary bits */
  state = state << 5;
  state = state >> 5;

  msg.snd_invKey = KR_KEYB_S;
  msg.snd_key0 = KR_VOID;
  msg.snd_key1 = KR_VOID;
  msg.snd_key2 = KR_VOID;
  msg.snd_rsmkey = KR_VOID;
  msg.snd_data = 0;
  msg.snd_len = 0;
  msg.snd_code = OC_set_leds;
  msg.snd_w1 = 0;
  msg.snd_w2 = state;
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
  
  if (msg.rcv_code != RC_OK) {
    kprintf (KR_OSTREAM, "Problem setting LEDs.  Please investigate.\n");
  }

  return;
}

/* Call our builder to notify if of new characters */
int
ProcessRequest() 
{
  Message msg;
  
  unsigned char ch = getChar();
  
  msg.snd_len  = 0;
  msg.snd_key0 = KR_VOID;
  msg.snd_key1 = KR_VOID;
  msg.snd_key2 = KR_VOID;
  msg.snd_rsmkey = KR_VOID;
  msg.snd_code   = OC_queue_keyevent;
  msg.snd_w1     = ch;
  msg.snd_w2     = 0;
  msg.snd_w3     = 0;
  msg.snd_invKey = KR_BUILDER;
  
  msg.rcv_limit = 0;
  msg.rcv_key0 = KR_VOID;
  msg.rcv_key1 = KR_VOID;
  msg.rcv_key2 = KR_VOID;
  msg.rcv_rsmkey = KR_VOID;
  msg.rcv_code = 0;
  msg.rcv_w1   = 0;
  msg.rcv_w2   = 0;
  msg.rcv_w3   = 0;
  CALL(&msg);
  
  return 1;
}


/* Process keys. This function is only called once (hopefully). 
 * We expect to get the builder's start key so that we can pass
 * him the keyboard data */
int
ProcessKeys(Message *msg) 
{
  msg->snd_len  = 0;
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
  case OC_builder_key:
    {
      Message mmsg; /* Message to the mouse client */
      
      /* Receive key to talk directly to my builder */
      process_copy_keyreg(KR_SELF,KR_SCRATCH, KR_BUILDER);
      
#ifdef VERBOSE
      kprintf(KR_OSTREAM,"keyclient:Receiving BUILDER key ... [SUCCESS]");
#endif 
      
      /* Distribute this key to the mouse client also, since
       * the mouseclient has to pass mouse data to the builder */
      mmsg.snd_invKey = KR_MCLI_S;
      mmsg.snd_code   = OC_builder_key;
      mmsg.snd_key0   = KR_VOID; 
      mmsg.snd_key1   = KR_BUILDER;
      mmsg.snd_key2   = KR_VOID; 
      mmsg.snd_rsmkey = KR_VOID;
      mmsg.snd_data = 0;
      mmsg.snd_len  = 0;
      mmsg.snd_w1 = 0;
      mmsg.snd_w2 = 0;
      mmsg.snd_w3 = 0;
      
      mmsg.rcv_key0   = KR_VOID;
      mmsg.rcv_key1   = KR_VOID;
      mmsg.rcv_key2   = KR_VOID;
      mmsg.rcv_rsmkey = KR_VOID;
      mmsg.rcv_data = 0;
      mmsg.rcv_limit = 0;
      mmsg.rcv_code = 0;
      mmsg.rcv_w1 = 0;
      mmsg.rcv_w2 = 0;
      mmsg.rcv_w3 = 0;
      CALL(&mmsg);
      
      /* This assures we are out of ProcessKeys after 1 proper call */
      return 0; 
    }

  default:
    break;
  }
  msg->snd_code = RC_eros_key_UnknownRequest;
  return 1;
}
