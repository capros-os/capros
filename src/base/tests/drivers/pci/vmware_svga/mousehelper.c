/*
 * Copyright (C) 2003, Jonathan S. Shapiro.
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

/* Mouse Client to the ps2 driver(ps2reader). This process gets mouse
 * data & calls the process specified in its builder Key , passing
 * (queuing?) MouseEvents */

#include <eros/target.h>
#include <eros/ProcessKey.h>
#include <eros/Invoke.h>
#include <eros/NodeKey.h>
#include <eros/cap-instr.h>

#include <idl/capros/key.h>
#include <idl/capros/Ps2.h>

#include <stdlib.h>

#include <domain/domdbg.h>
#include <domain/Runtime.h>

#include "constituents.h"

#define KR_OSTREAM    KR_APP(0)
#define KR_PS2READER  KR_APP(1)
#define KR_START      KR_APP(2)
#define KR_PARENT     KR_APP(3)
#define KR_SCRATCH    KR_APP(4)

#define MOUSE_BUTTONS(mask) (button & mask)

#define LEFT   0x1u
#define RIGHT  0x2u
#define MIDDLE 0x4u


/* Globals */
int mousePktNo = -1;
int8_t  button=0;
int     xmotion=0;
int     ymotion=0;
int     zmotion=0;
int     Is4Button;
int32_t w1,w2,w3;

/* Function prototypes */
bool ProcessRequest(int,int,int);

int 
main() {
  Message msg;

  node_extended_copy(KR_CONSTIT, KC_OSTREAM, KR_OSTREAM);
  
  COPY_KEYREG(KR_ARG(0), KR_PS2READER);
  process_make_start_key(KR_SELF, 0, KR_START);
  
  /* Return back a start key to our builder */
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
  msg.rcv_limit  = 0;
  msg.rcv_w1 = 0;
  msg.rcv_w2 = 0;
  msg.rcv_w3 = 0;
  
  kprintf(KR_OSTREAM, "** Helper is alive and about to go available...\n");

  RETURN(&msg);
  
  msg.snd_key0 = KR_VOID;
  msg.rcv_key0 = KR_VOID;
  
  /* We now have all the keys we need: PS2READER_KEY & PARENT_KEY
   * Send back to parent and keep running...
   * We start our job of looking for mouse packets and pushing them to
   * our parent */
  SEND(&msg);
    
  /* Call ps2reader to look for any mouse packets in the ps2 h/w buffer */
  do {
    (void)capros_Ps2_getMousedata(KR_PS2READER,&w1,&w2,&w3);
  } while (ProcessRequest(w1,w2,w3));

  return 0;
}

static result_t
send_mouse_data(cap_t kr_parent, uint32_t mask, int8_t xmotion, int8_t ymotion) 
{
  Message m;

  m.snd_invKey = kr_parent;
  m.snd_code = 0;
  m.snd_key0 = KR_VOID;
  m.snd_key1 = KR_VOID;
  m.snd_key2 = KR_VOID;
  m.snd_rsmkey = KR_VOID;
  m.snd_data = 0;
  m.snd_len = 0;
  m.snd_w1 = mask;
  m.snd_w2 = xmotion;
  m.snd_w3 = ymotion;

  m.rcv_rsmkey = KR_VOID;
  m.rcv_key0 = KR_VOID;
  m.rcv_key1 = KR_VOID;
  m.rcv_key2 = KR_VOID;
  m.rcv_data = 0;
  m.rcv_limit = 0;
  m.rcv_code = 0;
  m.rcv_w1 = 0;
  m.rcv_w2 = 0;
  m.rcv_w3 = 0;

  return CALL(&m);
}

bool
ProcessRequest(int w1,int w2,int w3) 
{
  int mask;
  
  /* Mouse data and keyboard data may be intermingled in the 
   * ps2 h/w buffer. So we count off 3 (4 if intellimouse) bytes 
   * before "creating" a mouseevent to queue up with our customer */
  
  Is4Button = w3;
    
  if(Is4Button) mousePktNo = (mousePktNo + 1)%4;
  else mousePktNo = (mousePktNo + 1)%3;
	
  if(mousePktNo==0) {
    button =  (uint8_t)w1;
    return 1;
  } else if(mousePktNo==1) {
    xmotion =  (int8_t)w1;
    return 1;
  }else if(mousePktNo==2) {
    ymotion =  (int8_t)w1;
    if(Is4Button) return 1;
  } else if(mousePktNo==3 && Is4Button) {
    zmotion =  (int8_t)w1;
    ymotion -= zmotion;  /* For the intellimouse */
  }
  mask = 0;
    
  if(MOUSE_BUTTONS(LEFT)==LEFT)  {     /* Left Button */
    mask = mask | LEFT;
  }
  if(MOUSE_BUTTONS(MIDDLE)==MIDDLE) { /* Middle Button */
    mask = mask | MIDDLE;
  }
  if(MOUSE_BUTTONS(RIGHT)==RIGHT)  {  /* Right Button */
    mask = mask | RIGHT;
  }
    
  /* Shoot off a message containing this event to our customer */
  send_mouse_data(KR_PARENT, mask, xmotion, ymotion);

  return true;  
}
