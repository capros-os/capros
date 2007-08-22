/*
 * Copyright (C) 2002, Jonathan S. Shapiro.
 * Copyright (C) 2007, Strawberry Development Group.
 *
 * This file is part of the CapROS Operating System distribution.
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
/* This material is based upon work supported by the US Defense Advanced
Research Projects Agency under Contract No. W31P4Q-07-C-0070.
Approved for public release, distribution unlimited. */

/* Keyboard Client to the ps2 driver(keyb). This process gets keyboard
 * scan codes from the ps2 driver, translates them to ASCII and calls 
 * its builder, passing(queuing?) these KeyEvents */
 
#include <eros/target.h>
#include <eros/NodeKey.h>
#include <eros/KeyConst.h>
#include <eros/Invoke.h>
#include <eros/cap-instr.h>

#include <idl/capros/key.h>
#include <idl/capros/Process.h>
#include <idl/capros/Ps2.h>

#include <domain/ConstructorKey.h>
#include <domain/domdbg.h>
#include <domain/ProcessCreatorKey.h>
#include <domain/Runtime.h>

#include "constituents.h"
#include "keytrans.h"
#include "ps2test.h"

#define KR_OSTREAM      KR_APP(0)
#define KR_PS2READER    KR_APP(2)
#define KR_MCLI_C       KR_APP(3)
#define KR_MCLI_S       KR_APP(4)
#define KR_START        KR_APP(5)
#define KR_PARENT       KR_APP(6)
#define KR_SCRATCH      KR_APP(7)

/* function declarations */
unsigned char getChar(void);
int getstate(int scan, int *status);
int ProcessRequest();
void updateLeds(int *status);

int
provide_parent_key(cap_t krProc) 
{
  Message msg; /* Message to the mouse client */
  
  /* Distribute this key to the mouse client also, since
   * the mouseclient has to pass mouse data to the parent */
  msg.snd_invKey = krProc;
  msg.snd_code   = 0;
  msg.snd_key0   = KR_VOID; 
  msg.snd_key1   = KR_PARENT;
  msg.snd_key2   = KR_VOID; 
  msg.snd_rsmkey = KR_VOID;
  msg.snd_data = 0;
  msg.snd_len  = 0;
  msg.snd_w1 = 0;
  msg.snd_w2 = 0;
  msg.snd_w3 = 0;
  
  msg.rcv_key0   = KR_VOID;
  msg.rcv_key1   = KR_VOID;
  msg.rcv_key2   = KR_VOID;
  msg.rcv_rsmkey = KR_VOID;
  msg.rcv_data = 0;
  msg.rcv_limit = 0;
  msg.rcv_code = 0;
  msg.rcv_w1 = 0;
  msg.rcv_w2 = 0;
  msg.rcv_w3 = 0;
  CALL(&msg);
  
  return 0; 
}


/* Get char from the ps2 driver (ps2reader). ASCII Characters may 
 * comprise of multiple scancodes */
unsigned char
getChar(void) 
{
  static int state = 0;
  int i, prevcode, scode;
  unsigned char kcode;
  int32_t valid;
  int32_t data;
  
  kcode = 0;
  prevcode = 0;
  
  /* Call ps2reader */
  while (kcode == 0) {
    (void)capros_Ps2_getKeycode(KR_PS2READER,&data,&valid);
    
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
            
      (void)capros_Ps2_getKeycode(KR_PS2READER,&data,&valid);
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

/* Update the LEDs incase of Caps, scroll,Num lock */
void
updateLeds(int *status) 
{
  int result;
  int state = *status;
    
  /* clear update flag */
  state &= ~LED_UPDATE;
  *status = state;

  /* clear unnecessary bits */
  state = state << 5;
  state = state >> 5;

  result = capros_Ps2_setLed(KR_PS2READER,state);
  
  if (result != RC_OK) {
    kprintf(KR_OSTREAM, "Problem setting LEDs.errcode = %d",result);
  }

  return;
}

/* Call our parent to notify if of new characters */
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
  msg.snd_invKey = KR_PARENT;
  
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

int
main(void) 
{
  Message msg;

  node_extended_copy(KR_CONSTIT, KC_OSTREAM, KR_OSTREAM);
  
  COPY_KEYREG(KR_ARG(0),KR_PS2READER);
  
  /* We are done with all the initial setup and will now return
   * our start key. */
  capros_Process_makeStartKey(KR_SELF,0,KR_START);

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

  msg.rcv_key0   = KR_PARENT;
  msg.rcv_key1   = KR_VOID;
  msg.rcv_key2   = KR_VOID;
  msg.rcv_rsmkey = KR_RETURN;
  msg.rcv_data = 0;
  msg.rcv_limit = 0;
  msg.rcv_code = 0;
  msg.rcv_w1 = 0;
  msg.rcv_w2 = 0;
  msg.rcv_w3 = 0;

  RETURN(&msg);
  
  /* Receive the parent key */
  msg.rcv_key0 = KR_VOID;

  /* Send back to our parent to get it running. Don't RETURN as
   * we need to run too */
  SEND(&msg);
  
  /* Infinitely loop getting char from ps2reader and 
   * passing it to (queuing it up at) the parent's */
  do {
  }while(ProcessRequest());
  
  return 0;
}
