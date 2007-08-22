/*
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

#include <eros/target.h>
#include <domain/domdbg.h>
#include <eros/Invoke.h>
#include <eros/NodeKey.h>

#include <idl/capros/key.h>
#include <idl/capros/Process.h>
#include <idl/capros/Ps2.h>

#include <domain/ConstructorKey.h>
#include <domain/Runtime.h>

#include "constituents.h"
#include "textconsole.h"
#include "ps2test.h"

#define KR_OSTREAM      KR_APP(0)
#define KR_TEXCON_S     KR_APP(1)
#define KR_KEYCLI_C     KR_APP(2)
#define KR_KEYCLI_S     KR_APP(3)
#define KR_START        KR_APP(4)
#define KR_SLEEP        KR_APP(5)
#define KR_PS2READER_C  KR_APP(6)
#define KR_PS2READER_S  KR_APP(7)
#define KR_MCLI_C       KR_APP(8)
#define KR_MCLI_S       KR_APP(9)

#define MOUSE_EVENT(mask) (w1 & mask)

#define LEFT   0x1u
#define RIGHT  0x2u
#define MIDDLE 0x4u

/* Globals::These values are for testing purpose only
 * These depend on screen handling system */
int MouseX = 40;
int MouseY = 12;
int ScreenX = 80;
int ScreenY = 25;
int8_t w1,w2,w3;

/* Return the length of a string*/
uint32_t
strlen_u(const uint8_t *s)
{
  uint32_t len = 0;
  while (*s++)
    len++;

  return len;
}

/* This function is for testing purposes only and should go away*/
void 
putColCharAtPos(int pos,uint8_t ch,int color) {
  Message msg;

  msg.snd_invKey = KR_TEXCON_S;
  msg.snd_key0 = KR_VOID; msg.snd_key1 = KR_VOID;
  msg.snd_key2 = KR_VOID; msg.snd_rsmkey = KR_VOID;
  msg.snd_data = 0;
  msg.snd_len = 0;
  msg.snd_code = OC_put_colchar_AtPos;
  msg.snd_w1 = pos;
  msg.snd_w2 = ch;
  msg.snd_w3 = color;

  msg.rcv_key0 = KR_VOID; msg.rcv_key1 = KR_VOID;
  msg.rcv_key2 = KR_VOID; msg.rcv_rsmkey = KR_VOID;
  msg.rcv_data = 0;
  msg.rcv_limit = 0;
  msg.rcv_code = 0;
  msg.rcv_w1 = 0;
  msg.rcv_w2 = 0;
  msg.rcv_w3 = 0;
  CALL(&msg);
  return ;
}


/* Update the mouse on the screen */
void 
UpdateMouse(int8_t x,int8_t y) {

  putColCharAtPos(80*MouseY+MouseX,0,RED);
  if(((MouseX + x) < ScreenX) && (MouseX + x) >= 0) {
    MouseX += x;
  }else {
    if(MouseX + x >=ScreenX) MouseX = ScreenX-1;
    else MouseX = 0;
  }

  if(((MouseY - y) < ScreenY) && (MouseY - y) >= 0) {
    MouseY -= y;
  }else {
    if(MouseY - y >=ScreenY) MouseY = ScreenY-1;
    else MouseY = 0;
  }
  putColCharAtPos(80*MouseY+MouseX,127,RED); /* some character */
  return;
}

void
put_char(int8_t c) 
{
  Message msg;
  
  msg.snd_invKey = KR_TEXCON_S;
  msg.snd_key0 = KR_VOID; msg.snd_key1 = KR_VOID;
  msg.snd_key2 = KR_VOID; msg.snd_rsmkey = KR_VOID;
  msg.snd_data = 0;
  msg.snd_len = 0;
  msg.snd_code = OC_put_char;
  msg.snd_w1 = c;
  msg.snd_w2 = 0;
  msg.snd_w3 = 0;

  msg.rcv_key0 = KR_VOID; msg.rcv_key1 = KR_VOID;
  msg.rcv_key2 = KR_VOID; msg.rcv_rsmkey = KR_VOID;
  msg.rcv_data = 0;
  msg.rcv_limit = 0;
  msg.rcv_code = 0;
  msg.rcv_w1 = 0;
  msg.rcv_w2 = 0;
  msg.rcv_w3 = 0;
  
  CALL(&msg);
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
  case OC_queue_keyevent:
    {
      put_char(w1);
      return 1;
    }
  case OC_queue_mouseevent:
    {
      putColCharAtPos(77,0,RED);
      putColCharAtPos(78,0,RED);
      putColCharAtPos(79,0,RED);
      
      if(MOUSE_EVENT(LEFT)==LEFT)  {//Left Button
	putColCharAtPos(77,'L',RED);
      }
      if(MOUSE_EVENT(MIDDLE)==MIDDLE)  {//Middle Button
	putColCharAtPos(78,'M',RED);
      }
      if(MOUSE_EVENT(RIGHT)==RIGHT)  {//Right Button
	putColCharAtPos(79,'R',RED);
      }
      
      UpdateMouse(w2,w3);
      
      return 1;
    }
  default:
    break;
  }

  msg->snd_code = RC_capros_key_UnknownRequest;
  return 1;
}

int
main(void)
{
  Message msg;
  uint32_t result;
  uint8_t *intro = "Testing TextConsole...[SUCCESS]\r\r";

  node_extended_copy(KR_CONSTIT,KC_OSTREAM,KR_OSTREAM);
  node_extended_copy(KR_CONSTIT,KC_TEXCON_S,KR_TEXCON_S);
  node_extended_copy(KR_CONSTIT,KC_KEYCLI_C,KR_KEYCLI_C);
  node_extended_copy(KR_CONSTIT,KC_MCLI_C,KR_MCLI_C);
  node_extended_copy(KR_CONSTIT,KC_PS2READER_C,KR_PS2READER_C);
    
  capros_Process_makeStartKey(KR_SELF, 0, KR_START);
    
  /* start textconsole & test it */
  msg.snd_invKey = KR_TEXCON_S;
  msg.snd_key0   = KR_VOID;
  msg.snd_key1   = KR_VOID;
  msg.snd_key2   = KR_VOID;
  msg.snd_rsmkey = KR_VOID;
  msg.snd_data = 0;
  msg.snd_len = 0;
  msg.snd_code = OC_clear_screen;
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
    
  /* Testing our TEXTCONSOLE */
  msg.snd_data = intro;
  msg.snd_len = strlen_u(intro);
  msg.snd_code = OC_put_char_arry;
  
  CALL(&msg);
        
  /* Construct the ps2 core */
  result = constructor_request(KR_PS2READER_C,KR_BANK,KR_SCHED,
			       KR_VOID,KR_PS2READER_S);
  if (result != RC_OK) {
    kprintf(KR_OSTREAM, "ps2test failed to start (ps2reader).\n");
  }

  /* Initialise the ps2 driver */
  result = capros_Ps2_initPs2(KR_PS2READER_S);
  if(result!=RC_OK) {
    kprintf(KR_OSTREAM,"ps2test:: Starting ps2reader...[Failed]");
    kprintf(KR_OSTREAM,"Error Code returned %d",result);
  }

  /* Construct the keyclient and get ready to receive ps2 events */
  result = constructor_request(KR_KEYCLI_C, KR_BANK, KR_SCHED, 
			       KR_PS2READER_S,KR_KEYCLI_S);
  if (result != RC_OK) {
    kprintf(KR_OSTREAM, "Constest:Constructing Keyclient...[FAILED]\n");
  }
  
  /* Construct the mouse client and give it the start key to ps2reader */
  result = constructor_request(KR_MCLI_C,KR_BANK,KR_SCHED,
			       KR_PS2READER_S,KR_MCLI_S);
  if (result != RC_OK) {
    kprintf(KR_OSTREAM, "KeyClient failed to start mouseclient.\n");
  }
  
  /* Give the mouseclient & the keyclient our start key */
  msg.snd_invKey = KR_KEYCLI_S;  
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
  
  CALL(&msg);
  
  msg.snd_invKey = KR_MCLI_S;  
  CALL(&msg);
  
  
  /* Return to null */
  msg.snd_invKey = KR_VOID;
  msg.rcv_rsmkey = KR_RETURN;
  do {
    RETURN(&msg);
    w1 = msg.rcv_w1;
    w2 = msg.rcv_w2;
    w3 = msg.rcv_w3;
    msg.rcv_rsmkey = KR_RETURN;
    msg.snd_invKey = KR_RETURN;
  }while(ProcessRequest(&msg));
  
  return 0;
}

