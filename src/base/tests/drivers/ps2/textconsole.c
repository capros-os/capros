/*
 * Copyright (C) 2007, 2008, Strawberry Development Group
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

/* Handles Screen drawing operations for 80 X 25 */

#include <eros/target.h>
#include <eros/Invoke.h>
#include <eros/i486/io.h>
#include <eros/KeyConst.h>
#include <eros/NodeKey.h>

#include <domain/Runtime.h>
#include <domain/domdbg.h>
#include <domain/SpaceBankKey.h>

#include <idl/capros/DevPrivs.h>
#include <idl/capros/Range.h>
#include <idl/capros/Process.h>
#include <idl/capros/arch/i386/Process.h>
#include <idl/capros/GPT.h>
#include <idl/capros/Stream.h>

#include <stdlib.h>

#include "constituents.h"
#include "textconsole.h"

#define KR_START     KR_APP(0)
#define KR_OSTREAM   KR_APP(1)
#define KR_DEVPRIVS  KR_APP(2)
#define KR_PHYSRANGE KR_APP(3)
#define KR_ADDRSPC   KR_APP(4)
#define KR_SCRATCH   KR_APP(10)

#define CODE_BS  0x08
#define CODE_TAB 0x09
#define CODE_CR  0x0D
#define CODE_ESC 0x1B

#define ROWS 25
#define COLS 80 

const uint32_t __rt_stack_pages = 2;
const uint32_t __rt_stack_pointer = 0x20000;

#define SCRMAX 1999

void initialize(void);
int putCharAtPos(unsigned int pos, unsigned char ch);
int ProcessRequest(Message *msg);
int IsPrint(uint8_t c);
void processChar(uint8_t ch);
void processEsc(uint8_t ch);
void processInput(uint8_t ch);
void putCursAt(int psn);
void scroll(uint32_t spos, uint32_t epos, int amt);
int putColCharAtPos(unsigned int pos, unsigned char ch,int color);

/* global variables */

uint8_t mybuf[4096];
uint16_t *screen = (uint16_t *) (EROS_PAGE_SIZE * EROS_NODE_SIZE);
uint32_t startAddrReg = 0;
uint8_t state = NotInit;
uint8_t param[10];
unsigned int npar = 0;
int pos = 0;


int 
main(void) 
{
  Message msg;
    
  initialize();

  /*Initialization done.Let's return a start key and get on with things*/
  msg.snd_invKey = KR_RETURN;
  msg.snd_key0 = KR_START;
  msg.snd_key1 = KR_VOID;
  msg.snd_key2 = KR_VOID;
  msg.snd_rsmkey = KR_RETURN;
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
  msg.rcv_data = &mybuf;
  msg.rcv_limit = sizeof(mybuf);
  msg.rcv_code = 0;
  msg.rcv_w1 = 0;
  msg.rcv_w2 = 0;
  msg.rcv_w3 = 0;
    
    
  do {
    msg.rcv_limit = sizeof(mybuf);
    RETURN(&msg);
    msg.snd_invKey = KR_RETURN;
    msg.snd_key0 = KR_VOID;
    msg.snd_rsmkey = KR_RETURN;
  } while (ProcessRequest(&msg));

  return 0;
}

void
initialize(void)
{

  uint32_t result;
  uint8_t hi, lo;
  
  node_copy(KR_CONSTIT, KC_OSTREAM, KR_OSTREAM);
  node_copy(KR_CONSTIT, KC_DEVPRIVS, KR_DEVPRIVS);
  node_copy(KR_CONSTIT, KC_PHYSRANGE, KR_PHYSRANGE);
  
  result = capros_Process_makeStartKey(KR_SELF, 0, KR_START);
  if(result!=RC_OK) kprintf(KR_OSTREAM,"textconsole::Start key--Failed");
  
  result = capros_arch_i386_Process_setIoSpace(KR_SELF, KR_DEVPRIVS);
  if(result!=RC_OK) kprintf(KR_OSTREAM,"textconsole::Process swap--Failed");
  kprintf(KR_OSTREAM, "Should now have IOspace key in slot\n");
  
  result = capros_SpaceBank_alloc1(KR_BANK, capros_Range_otGPT, KR_ADDRSPC);
  if(result!=RC_OK) kprintf(KR_OSTREAM,"textconsole::spcbank buy--Failed");
  kprintf(KR_OSTREAM, "Bought 1 GPT from spcbank\n");
  
  result=capros_GPT_setL2v(KR_ADDRSPC, EROS_PAGE_LGSIZE + EROS_NODE_LGSIZE);
  if(result!=RC_OK) kprintf(KR_OSTREAM,"textconsole::setL2v--Failed");
  result=capros_Memory_reduce(KR_ADDRSPC, capros_Memory_noCall, KR_ADDRSPC);
  if(result!=RC_OK) kprintf(KR_OSTREAM,"textconsole::reduce--Failed");
  
  result = capros_Process_getAddrSpace(KR_SELF, KR_SCRATCH);
  if(result!=RC_OK) kprintf(KR_OSTREAM,"textconsole::process copy-Failed");

  result = capros_GPT_setSlot(KR_ADDRSPC, 0, KR_SCRATCH);
  if(result!=RC_OK) kprintf(KR_OSTREAM,"textconsole::setSlot--Failed");

  result = capros_Process_swapAddrSpace(KR_SELF, KR_ADDRSPC, KR_VOID);
  if(result!=RC_OK) kprintf(KR_OSTREAM,"textconsole::process swap--Failed");

#ifdef i386

  /* This SHOULD be the VGA memory area */

  result = capros_DevPrivs_publishMem(KR_DEVPRIVS, 0xa0000u, 0xc0000u, 0);
  if(result!=RC_OK) kprintf(KR_OSTREAM,"textconsole::publish mem--Failed");

#else
#error "This is an i386 console driver.  You are not running an i386.  Sorry."
#endif

  capros_Range_obType currentType;
  result = capros_Range_waitCap(KR_PHYSRANGE,
                      capros_Range_otPage,
		      (0xb8000 / EROS_PAGE_SIZE) * EROS_OBJECTS_PER_FRAME,
                      &currentType,
		      KR_SCRATCH);
  if (result != RC_OK || currentType != capros_Range_otPage)
    kprintf(KR_OSTREAM, "textconsole::getCap--Failed");

  capros_GPT_setSlot(KR_ADDRSPC, 1, KR_SCRATCH);

  kprintf(KR_OSTREAM, "Calling outb on 0x3D4\n");
  outb(0xC, 0x3D4);
  hi = inb(0x3D5);
  outb(0xD, 0x3D4);
  lo = inb(0x3D5);
  kprintf(KR_OSTREAM, "Done with cursor init\n");

  startAddrReg = hi << 8 | lo;
  state = WaitChar;
  pos = 0;

  return;

}


int
IsPrint(uint8_t c)
{
  const uint8_t isPrint[16] = {	/* really a bitmask! */
    0x00,			/* BEL ACK ENQ EOT ETX STX SOH NUL */
    0x00,			/* SI  SO  CR  FF  VT  LF  TAB BS */
    0x00,			/* ETB SYN NAK DC4 DC3 DC2 DC1 DLE */
    0x00,			/* US  RS  GS  FS  ESC SUB EM  CAN */
    0xff,			/* '   &   %   $   #   "   !   SPC */
    0xff,			/* /   .   -   ,   +   *   )   ( */
    0xff,			/* 7   6   5   4   3   2   1   0 */
    0xff,			/* ?   >   =   <   ;   :   9   8 */
    0xff,			/* G   F   E   D   C   B   A   @ */
    0xff,			/* O   N   M   L   K   J   I   H */
    0xff,			/* W   V   U   T   S   R   Q   P */
    0xff,			/* _   ^   ]   \   [   Z   Y   X */
    0xff,			/* g   f   e   d   c   b   a   ` */
    0xff,			/* o   n   m   l   k   j   i   h */
    0xff,			/* w   v   u   t   s   r   q   p */
    0x7f,			/* DEL ~   }   |   {   z   y   x */
  };

  uint8_t mask;

  if (c > 127)
    return 0;
 
  mask = isPrint[(c >> 3)];
  c &= 0x7u;
  if (c)
    mask >>= c;
  if (mask & 1)
    return 1;
  return 0;
}

void
clearScreen(void)
{

  int i;
  for (i = 0; i <= SCRMAX; i++) {
    screen[i] = 0x700u;		/* must preserve mode attributes! */
  }

  pos = 0;
  putCursAt(pos);
  return;
}

int
putCharAtPos(unsigned int pos, uint8_t ch) 
{

  if ((pos < 0) || (pos > SCRMAX)) {
    /* Invalid range specified.  Return 1 */
    return 1;
  }

  screen[pos] = ch | 0x200u;

  return 0;
}

void
processEsc(uint8_t ch)
{

  uint32_t p;
  uint32_t posRow = pos / COLS;
  uint32_t posCol = pos % COLS;


  if (ch != 'J' && ch != 'K') {
    for (p = 0; p < 10; p++) {
      if (param[p] == 0) {
	param[p] = 1;
      }
    }
  }


  switch (ch) {
  case '@':
    {
      int distance;
      if (param[0] < (COLS - posCol))
	distance = param[0];
      else
	distance = COLS - posCol;

      scroll (pos, pos + (COLS - posCol), distance);
      break;
    }
  case 'A':
    {
      uint32_t count;
      if (param[0] < posRow)
	count = param[0];
      else
	count = posRow;

      pos -= (COLS * count);
      putCursAt(pos);
      break;
    }
  case 'B':
    {
      uint32_t count;
      if (param[0] < ROWS - posRow - 1)
	count = param[0];
      else
	count = ROWS - posRow - 1;

      pos += (COLS * count);
      putCursAt(pos);
      break;
    }
  case 'C':
    {
      uint32_t count;
      if (param[0] < COLS - posCol - 1)
	count = param[0];
      else
	count = COLS - posCol - 1;

      pos += count;
      putCursAt(pos);
      break;
    }
  case 'D':
    {
      uint32_t count;
      if (param[0] < posCol)
	count = param[0];
      else
	count = posCol;

      pos -= count;
      putCursAt(pos);
      break;
    }
  case 'f':
  case 'H':
    {
      uint32_t therow, thecol;

      if (param[0] < ROWS)
	therow = param[0];
      else
	therow = ROWS;
      if (param[1] < COLS)
	thecol = param[1];
      else
	thecol = COLS;

      therow--;
      thecol--;

      pos = (therow * COLS) + thecol;
      putCursAt(pos);
      break;
    }
  case 'I':
    {
      uint32_t dist;
      if ((param[0] * 8) < (ROWS*COLS) - pos)
	dist = param[0] * 8;
      else
	dist = (ROWS*COLS) - pos;

      pos += dist - (pos % 8);
      putCursAt(pos);
      break;
    }
  case 'J':
    {
      int newpos, oldpos;
      oldpos = pos;


      if (param[0] == 0) {
	newpos = ROWS * COLS;
	while (pos < newpos) {
	  putCharAtPos(pos, ' ');
	  pos++;
	}
	pos = oldpos;
	putCursAt(pos);
      }
      else if (param[0] == 1) {
	newpos = 0;
	while (pos > (newpos - 1)) {
	  putCharAtPos(pos, ' ');
	  pos--;
	}
	pos = oldpos;
	putCursAt(pos);
      }
      else if (param[0] == 2) {
	clearScreen();
      }

      break;
    }
  case 'K':
    {
      int start, end, oldpos;

      start = 0;

      oldpos = pos;

      if (param[0] == 0) {
	end = (pos / 80) + 80;
	while (pos < end) {
	  putCharAtPos(pos, ' ');
	  pos++;
	}
	pos = oldpos;
	putCursAt(pos);
      }
      else if (param[0] == 1) {
	end = (pos / 80);
	while (pos > end - 1) {
	  putCharAtPos(pos, ' ');
	  pos--;
	}
	pos = oldpos;
	putCursAt(pos);
      }
      else if (param[0] == 2) {
	start = pos / 80;
	end = (pos / 80) + 80;
	pos = start;
	while (pos < end) {
	  putCharAtPos(pos, ' ');
	  pos++;
	}
	pos = start;
	putCursAt(pos);
      }
      break;
    }
  case 'L':
    {
      uint32_t nrows;
      if (param[0] < ROWS - posRow)
	nrows = param[0];
      else
	nrows = ROWS - posRow;

      scroll(posRow * COLS, ROWS * COLS, nrows * COLS);
#if 0 /* Do we want the cursor to move with the line? */
      pos = pos + (nrows * COLS);
      putCursAt(pos);
#endif
      break;
    }
  case 'M':
    {
      uint32_t nrows;
      int endpos, oldpos;
      oldpos = pos;
      if (param[0] < ROWS - posRow)
	nrows = param[0];
      else
	nrows = ROWS - posRow;

      if (pos % COLS == 0) 
	endpos = pos + (nrows * COLS);
      else
	endpos = ((pos / COLS) * COLS) +  COLS + (nrows * COLS);
      while (pos < endpos) {
	putCharAtPos(pos, ' ');
	pos++;
      }
      pos = oldpos;
      putCursAt(pos);
      break;
    }
  case 'X':
  case 'P':
    {
      int dist, oldpos, endpos;
      oldpos = pos;
      
      if (param[0] < COLS - posCol)
	dist = param[0];
      else
	dist = COLS - posCol;
      endpos = pos + dist;
      while (pos < endpos) {
	putCharAtPos(pos, ' ');
	pos++;
      }
      pos = oldpos;
      putCursAt(pos);
      
      break;
    }
  case 'S':
    {
      /* This will be cooler once we have a scrollback buffer */
      uint32_t lines;
      lines = param[0];

      scroll(0, ROWS * COLS, (int) lines * COLS);
      break;
    }
  case 'T':
    {
      /* This will be more exciting once we have a scrollback buffer */
      uint32_t lines;
      lines = param[0];

      scroll(0, ROWS * COLS, - (int) lines * COLS);
      break;
    }
  case 'Z':
    {
      uint32_t dist;
      if (pos - (param[0] * 8) > 0)
	dist = param[0] * 8;
      else {
	pos = 0;
	putCursAt(0);
	break;
      }
      
      pos -= dist - (pos % 8);
      putCursAt(pos);
      break;
    }

  }

  state = WaitChar;
  return;
}


void
processInput(uint8_t ch)
{

  if (state == NotInit) {
    initialize();
  }

  switch (state) {
    
  case WaitChar:
    {
      if (ch == CODE_ESC) {
	state = GotEsc;
	return;
      }

      processChar(ch);
      break;
    }

  case GotEsc:
    {
      int i;
      if (ch == '[') {

	state = WaitParam;
	
	npar = 0;
	for (i = 0; i < 10; i++) {
	  param[i] = 0;
	}
	return;
      }
      
      state = WaitChar;
      break;
    }

  case WaitParam:
    {
      if (npar < 2 && ch >= '0' && ch <= '9') {
	param[npar] *= 10;
	param[npar] += (ch - '0');
      }
      else if (npar < 2 && ch == ';') {
	npar++;
      }
      else if (npar == 2) {
	state = WaitChar;
      }
      else {
	processEsc(ch);
      }
    }
    break;
  }
}
      
void
putCursAt(int psn)
{

  uint32_t cursAddr = (uint32_t) psn;

  cursAddr += startAddrReg;

  outb(0xE, 0x3D4);
  outb((cursAddr >> 8) & 0xFFu, 0x3D5);
  outb(0xF, 0x3D4);
  outb((cursAddr & 0xFFu), 0x3D5);
  return;
}

void
processChar(uint8_t ch)
{

  const int STDTABSTOP = 8;
  int TABSTOP = STDTABSTOP;

  if (IsPrint(ch)) {
    
    putCharAtPos(pos, ch);    
    pos++;
  }

  else {
    
    switch (ch) {
    case CODE_BS:
      {
	if ((pos - 1) >= 0) {
	  pos--;
	  putCharAtPos(pos, 0);
	}
	break;
      }

    case CODE_TAB:
      {
	int i;
	if (pos == 0) {
	  for (i = 0; i < 8; i++) {
	    putCharAtPos(pos, 0);
	    pos++;
	  }
	}
	else {
	  while (pos % TABSTOP) {
	    putCharAtPos(pos, 0);
	    pos++;
	  }
	}
	break;
      }

    case CODE_CR:
      {
	int newpos;
	newpos = ((pos / COLS) *COLS) + COLS;
	
	while (pos < newpos) {
	  putCharAtPos(pos, 0);
	  pos++;
	}
	break;
      }
    }
  }

  if (pos >= ROWS * COLS) {
    scroll(0, ROWS * COLS, - (int) COLS);
    pos -= COLS;
  }
  putCursAt(pos);

  
  return;
}


void
scroll(uint32_t spos, uint32_t epos, int amt)
{

  uint16_t oldscreen[2000];
  uint32_t gap, p;

  for (p = 0; p < 2000; p++) {
    oldscreen[p] = screen[p];
  }
  
  if (amt > 0) {
    gap = amt;
    for (p = spos + gap; p < epos; p++) {
      screen[p] = oldscreen[p - gap];
    }

    for (p = spos; p < spos + gap; p++) {
      screen[p] = (0x7 << 8);
    }

  }
  else {
    gap = -amt;
    for (p = spos; p < epos - (gap - 1); p++) {
      screen[p] = oldscreen[p + gap];
    }

    for (p = epos - gap; p < epos; p++) {
      screen[p] = (0x7 << 8);
    }
  }

  
  return;
}

int
putColCharAtPos(unsigned int pos, uint8_t ch,int color)
{
  if ((pos < 0) || (pos > SCRMAX)) {
    /* Invalid range specified.  Return 1 */
    return 1;
  }
  screen[pos] = ch | color;
  return 0;
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

  case OC_clear_screen:
    {
      clearScreen();
      msg->snd_code = RC_OK;
      return 1;
    }

  case OC_put_char_arry:
    {
      int i;
      uint32_t len;
      uint8_t ch;
      len = min(msg->rcv_limit, msg->rcv_sent);
      for (i = 0; i < len; i++) {
	ch = mybuf[i];
	processInput(ch);
      }
      msg->snd_code = RC_OK;
      return 1;
    }
  case OC_capros_Stream_write:
  case OC_put_char:
    {
      uint8_t ch;
     
      ch = msg->rcv_w1;
      processInput(ch);
      msg->snd_code = RC_OK;
      return 1;
    }
  case OC_put_colchar_AtPos:
    {
      uint8_t ch;
      int pos,color;
      
      pos = msg->rcv_w1;
      ch = msg->rcv_w2;
      color = msg->rcv_w3;
      putColCharAtPos(pos,ch,color);
      
      msg->snd_code = RC_OK;
      return 1;
    }
    
  default:
    kprintf(KR_OSTREAM,"Texcon received default call");
    break;
  }

  msg->snd_code = RC_capros_key_UnknownRequest;
  return 1;
}
