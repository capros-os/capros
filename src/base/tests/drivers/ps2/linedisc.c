/*
 * Copyright (C) 2005, 2007, Strawberry Development Group
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

#include <eros/target.h>
#include <domain/domdbg.h>
#include <eros/KeyConst.h>
#include <eros/NodeKey.h>
#include <eros/Invoke.h>
#include <idl/capros/Process.h>
#include <idl/capros/Sleep.h>
#include <idl/capros/ProcCre.h>
#include <eros/i486/atomic.h>
#include <domain/ConstructorKey.h>
#include <domain/SpaceBankKey.h>
#include <domain/Runtime.h>
#include <domain/drivers/ps2/ps2.h>

#include "constituents.h"
#include "textconsole.h"
#include "linedisc.h"

#define KR_DOMCRE      3
#define KR_OSTREAM     7
#define KR_KEYCLI_C    8
#define KR_KEYCLI_S    9
#define KR_TEXCON_S    11
#define KR_START       14
#define KR_NEWPROC     15
#define KR_NEWFAULT    16
#define KR_SAVEDRESUME 18
#define KR_SLEEP       19
#define KR_SCRATCH     20
#define KR_RETURN      31

#define CODE_BS  0x08
#define CODE_CR  0x0D

Line* allocLine(void);
void appendLine(Line *ln);
void freeLine(Line *ln);
Line* getLine(void);
void ldInit(void);
void makeSharedProc(void);
int  ProcessRequest(Message *msg);
void saveCaller(void);
void sharedMain(void);
void wakeCaller(void);

/* Globals */
uint8_t mybuf[4096];
Line lines[MAXLINES];
Line *freelist;
Line *lineHead, *lineTail; /* These are head and tail pointers for
			      lines that are in use */
uint32_t appenderMustInvoke;

int
main(void) 
{
  Message msg;
  
  ldInit();

  makeSharedProc();
  
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
  msg.rcv_len = sizeof(mybuf);
  msg.rcv_code = 0;
  msg.rcv_w1 = 0;
  msg.rcv_w2 = 0;
  msg.rcv_w3 = 0;

  do {
    msg.rcv_len = sizeof(mybuf);
    RETURN(&msg);
    msg.snd_invKey = KR_RETURN;
    msg.snd_key0 = KR_VOID;
    msg.snd_rsmkey = KR_RETURN;
  } while (ProcessRequest(&msg));

  return 0;
}


Line*
allocLine(void)
{
  Line *next, *cur;

  if (freelist) {
  
    for (;;) {
      next = freelist->next;
      cur = freelist;
      if (ATOMIC_SWAP32((uint32_t *) &freelist, (uint32_t) cur,
			(uint32_t) next))
	break;
    }

  }

  else
    cur = 0;
  
  return cur;

}

void
appendLine(Line *ln)
{

  Line *old, *new;

  for (;;) {

    old = lineTail;
    ln->next = 0;
    if (old == 0) {
      new = ln;
    }
    else {
      new = old;
      new->next = ln;
    }
    if (ATOMIC_SWAP32((uint32_t *) &lineTail, (uint32_t) old,
		      (uint32_t) new))
      break;
  }

  if (old == 0) {
    ATOMIC_SWAP32((uint32_t *) &lineHead, (uint32_t) 0, (uint32_t)
		  lineTail);
  }

  return;
  
}

void
freeLine(Line *ln)
{
  Line *cur;


  for (;;) {

    cur = freelist;
    ln->next = cur;
    if (ATOMIC_SWAP32((uint32_t *) &freelist, (uint32_t) cur,
		     (uint32_t) ln))
      break;

  }
	 
  return;
}

Line*
getLine(void)
{
  Line *cur, *next;


  for (;;) {

    cur = lineHead;
    if (!cur) {
      
      /* I was going to call capros_Sleep_sleep here, but figured it would be
	 better not to block and have the function caller check for
	 the return of a null pointer and call capros_Sleep_sleep instead
      */
      return 0;
    }
    next = cur->next;
    if (ATOMIC_SWAP32((uint32_t *) &lineHead, (uint32_t) cur,
		     (uint32_t) next))
      break;
  }
  if (next == 0) {
    ATOMIC_SWAP32((uint32_t *) &lineTail, (uint32_t) cur, (uint32_t)
		  0);
  }

  return cur;
}

void
ldInit(void)
{

  Message msg;
  uint32_t result;
  int i;

  node_copy(KR_CONSTIT, KC_OSTREAM, KR_OSTREAM);
  node_copy(KR_CONSTIT, KC_KEYCLI, KR_KEYCLI_C);
  node_copy(KR_CONSTIT, KC_TEXCON, KR_TEXCON_S);
  node_copy(KR_CONSTIT, KC_SLEEP, KR_SLEEP);
    
  result = capros_Process_getSchedule(KR_SELF, KR_SCHED);
  if(result != RC_OK) {
    kprintf(KR_OSTREAM,"linedisc:: Process Copy -- Failed");
  }
  
  result = capros_Process_makeStartKey(KR_SELF, 0, KR_START);
  if(result != RC_OK) {
    kprintf(KR_OSTREAM,"linedisc:: Start Key -- Failed");
  }
  
  result = constructor_request(KR_KEYCLI_C, KR_BANK, KR_SCHED, KR_VOID,
			       KR_KEYCLI_S);
  
  if (result != RC_OK) {
    kprintf(KR_OSTREAM, "Linedisc: Failed to construct keyclient!\n");
  }
  kprintf(KR_OSTREAM, "Linedisc: constructing keyclient ... [SUCCESS]\n");
    
  msg.snd_invKey = KR_KEYCLI_S;
  msg.snd_key0 = KR_VOID;
  msg.snd_key1 = KR_VOID;
  msg.snd_key2 = KR_VOID;
  msg.snd_rsmkey = KR_VOID;
  msg.snd_data = 0;
  msg.snd_len = 0;
  msg.snd_code = OC_init_kbd;
  msg.snd_w1 = 0;
  msg.snd_w2 = 0;
  msg.snd_w3 = 0;

  msg.rcv_key0 = KR_VOID;
  msg.rcv_key1 = KR_VOID;
  msg.rcv_key2 = KR_VOID;
  msg.rcv_rsmkey = KR_VOID;
  msg.rcv_data = 0;
  msg.rcv_len = 0;
  msg.rcv_code = 0;
  msg.rcv_w1 = 0;
  msg.rcv_w2 = 0;
  msg.rcv_w3 = 0;

  CALL(&msg);
  if (msg.rcv_code != RC_OK) {
    kprintf(KR_OSTREAM, "Linedisc: keyclient failed to init keyboard\n");
  }

  msg.snd_invKey = KR_TEXCON_S;
  msg.snd_code = OC_clear_screen;
  capros_Sleep_sleep(KR_SLEEP,10);
  CALL(&msg); 

  /* Init line structs */

  freelist = &lines[0];
  for (i = 0; i < (MAXLINES - 1); i++) {
    lines[i].next = &lines[i+1];
  }
  lines[MAXLINES - 1].next = 0;

  lineHead = 0;
  lineTail = 0;

  appenderMustInvoke = 0; /* Make sure we start shared proc after
			     setting this */
  
  return;
}

void
makeSharedProc(void) 
{


  uint32_t result;
  Message msg;
  
  kprintf(KR_OSTREAM,"Starting makeSharedProc...");
  
  result = capros_ProcCre_createProcess(KR_DOMCRE, KR_BANK, KR_NEWPROC);
  if (result != RC_OK) {
    kprintf (KR_OSTREAM, "Failed to create new process\n");
  }

  result = capros_Process_getAddrSpace(KR_SELF, KR_SCRATCH);
  if (result != RC_OK) {
    kprintf(KR_OSTREAM, "Couldn't get my address space?\n");
  }

  result = capros_Process_swapAddrSpaceAndPC32(KR_NEWPROC, KR_SCRATCH,
             (unsigned) &sharedMain, KR_VOID);
  if (result != RC_OK) {
    kprintf(KR_OSTREAM, "Couldn't insert address space in new process?\n");
  }

  result = capros_Process_getSchedule(KR_SELF, KR_SCRATCH);
  if (result != RC_OK) {
    kprintf(KR_OSTREAM, "Couldn't get schedule key into scratch reg\n");
  }

  result = capros_Process_swapSchedule(KR_NEWPROC, KR_SCRATCH, KR_VOID);
  if (result != RC_OK) {
    kprintf(KR_OSTREAM, "Couldn't insert schedule key into new process\n");
  }

  result = capros_Process_swapKeyReg(KR_NEWPROC, KR_SCHED, KR_SCRATCH, KR_VOID);
  if (result != RC_OK) {
    kprintf(KR_OSTREAM, "Couldn't copy schedule key to new proc\n");
  }
  
  result = capros_Process_getKeyReg(KR_SELF, KR_OSTREAM, KR_SCRATCH);
  if (result != RC_OK) {
    kprintf(KR_OSTREAM, "Couldn't copy Console Key\n");
  }

  result = capros_Process_swapKeyReg(KR_NEWPROC, KR_OSTREAM, KR_SCRATCH, KR_VOID);
  if (result != RC_OK) {
    kprintf(KR_OSTREAM, "Couldn't swap console key into new process\n");
  }
  
  result = capros_Process_getKeyReg(KR_SELF, KR_SLEEP, KR_SCRATCH);
  if (result != RC_OK) {
    kprintf(KR_OSTREAM, "Couldn't copy Sleep key\n");
  }
  
  result = capros_Process_swapKeyReg(KR_NEWPROC, KR_SLEEP, KR_SCRATCH, KR_VOID);
  if (result != RC_OK) {
    kprintf(KR_OSTREAM, "Couldn't swap sleep key into new process\n");
  }
  
  result = capros_Process_getKeyReg(KR_SELF, KR_KEYCLI_S, KR_SCRATCH);
  if (result != RC_OK) {
    kprintf(KR_OSTREAM, "Couldn't copy start key to keycli\n");
  }
  
  result = capros_Process_swapKeyReg(KR_NEWPROC, KR_KEYCLI_S, KR_SCRATCH, KR_VOID);
  if (result != RC_OK) {
    kprintf(KR_OSTREAM, "Couldn't swap keycli start key into new process\n");
  }

  result = capros_Process_getKeyReg(KR_SELF, KR_TEXCON_S, KR_SCRATCH);
  if (result != RC_OK) {
    kprintf(KR_OSTREAM, "Couldn't copy start key to textcon\n");
  }

  result = capros_Process_swapKeyReg(KR_NEWPROC, KR_TEXCON_S, KR_SCRATCH, KR_VOID);
  if (result != RC_OK) {
    kprintf(KR_OSTREAM, "Couldn't swap textcons start key into new process\n");
  }

  { // Set the SP.
    struct capros_Process_CommonRegisters32 regs;
    result = capros_Process_getRegisters32(KR_NEWPROC, &regs);
    if (result != RC_OK) {
      kprintf(KR_OSTREAM, "Something is broken. It should probably be fixed\n");
    }
  
    /* ::Was 0x30000u. I have no idea on this one!!! */
    regs.sp = 0x10000u;
  
    result = capros_Process_setRegisters32(KR_NEWPROC, regs);
    if (result != RC_OK) {
      kprintf(KR_OSTREAM, "Something is broken. It should probably be fixed\n");
    }
  }
  
  result = capros_Process_makeResumeKey(KR_NEWPROC, KR_NEWFAULT);
  if (result != RC_OK) {
    kprintf(KR_OSTREAM, "Problem Making fault key...\n");
  }

#ifdef KERNEL_BARF
  kdprintf(KR_OSTREAM, "Starting process in KR%d\n", KR_NEWPROC);
#endif /* KERNEL_BARF (vomit) */

  msg.snd_invKey = KR_NEWFAULT;
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
  
  result = SEND(&msg);
  if(result != RC_OK) {
    kprintf(KR_OSTREAM,"SEND ... [FAILED]");
  }
  return;
}

void
saveCaller(void)
{

  capros_Process_getKeyReg(KR_SELF, KR_RETURN, KR_SAVEDRESUME);
  capros_Process_swapKeyReg(KR_NEWPROC, KR_SAVEDRESUME /* his */, 
		      KR_SAVEDRESUME /* mine */, 
		      KR_VOID);
  ATOMIC_SWAP32(&appenderMustInvoke, (uint32_t) 0, (uint32_t) 1);
  return;  
}

void
wakeCaller(void)
{

  /* Either process may call this, it simply clears the appender must
     invoke flag before sending a retry message to the caller client */

  Message msg;

  if (ATOMIC_SWAP32(&appenderMustInvoke, (uint32_t) 1, (uint32_t) 0)) {

    //msg.snd_invKey = KR_RETURN;
    msg.snd_invKey = KR_SAVEDRESUME;
    msg.snd_key0 = KR_VOID;
    msg.snd_key1 = KR_VOID;
    msg.snd_key2 = KR_VOID;
    msg.snd_rsmkey = KR_SAVEDRESUME;
    msg.snd_data = 0;
    msg.snd_len = 0;
    msg.snd_code = RC_retry;
    msg.snd_w1 = 0;
    msg.snd_w2 = 0;
    msg.snd_w3 = 0;
    
    msg.rcv_key0 = KR_VOID;
    msg.rcv_key1 = KR_VOID;
    msg.rcv_key2 = KR_VOID;
    msg.rcv_rsmkey = KR_VOID;
    msg.rcv_data = 0;
    msg.rcv_len = 0;
    msg.rcv_code = 0;
    msg.rcv_w1 = 0;
    msg.rcv_w2 = 0;
    msg.rcv_w3 = 0;

    SEND(&msg);
  }
  return;
  
}

void
sharedMain(void) 
{

  Line *ln;
  uint8_t chr;
  int i,pos;
  Message mess;
    
  kprintf(KR_OSTREAM," Starting sharedMain ... [SUCCESS]");
 
  mess.snd_invKey = KR_KEYCLI_S;
  mess.snd_key0 = KR_VOID;
  mess.snd_key1 = KR_VOID;
  mess.snd_key2 = KR_VOID;
  mess.snd_rsmkey = KR_VOID;
  mess.snd_data = 0;
  mess.snd_len = 0;
  mess.snd_code = OC_flush_kbd_buf;
  mess.snd_w1 = 0;
  mess.snd_w2 = 0;
  mess.snd_w3 = 0;

  mess.rcv_key0 = KR_VOID;
  mess.rcv_key1 = KR_VOID;
  mess.rcv_key2 = KR_VOID;
  mess.rcv_rsmkey = KR_VOID;
  mess.rcv_data = 0;
  mess.rcv_len = 0;
  mess.rcv_code = 0;
  mess.rcv_w1 = 0;
  mess.rcv_w2 = 0;
  mess.rcv_w3 = 0;
  
  CALL(&mess);

  ln = 0;
  
  for(;;) {
    
    /* Append full line */
    if (ln) {
      if (ln->len == MAXLEN) {
	appendLine(ln);
	ln = 0; /* NULL */
	if (appenderMustInvoke)
	  wakeCaller();
      }
    }
    /* If no line, get a new one */
    
    if (!ln) {
      ln = allocLine();
      while (!ln) {
	capros_Sleep_sleep(KR_SLEEP, 10);
	ln = allocLine();
      }
      for (i = 0; i < MAXLEN; i++) {
	ln->chars[i] = 0;
      }
      ln->len = 0;
      ln->offset = 0;
      ln->next = 0;
    }
    
    mess.snd_invKey = KR_KEYCLI_S;
    mess.snd_code = OC_get_char;
    CALL(&mess);
    chr = mess.rcv_w1;
      
    switch (chr) {
      
    case CODE_CR:
      {
	ln->len++;
	pos = (ln->len) - 1;
	ln->chars[pos] = chr;
	appendLine(ln);
	ln = 0; /* Next time around, we should get a new line... */
	if (appenderMustInvoke)
	  wakeCaller();
	break;
      }
      
    case CODE_BS:
      {
	pos = (ln->len) - 1;
	ln->chars[pos] = 0;
	ln->len--;
	break;
      }
      
    default:
      {
	ln->len++;
	pos = (ln->len) - 1;
	ln->chars[pos] = chr;
	break;
      }
      
    }
    /* Put char here */    
    mess.snd_invKey = KR_TEXCON_S;
    mess.snd_code = OC_put_char;
    mess.snd_w1 = chr;
    CALL(&mess);
  }
}

int
ProcessRequest(Message *msg) 
{

  static Line *curln = 0;

  /* Issue: What's all this, then? */
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

  case OC_print_line:
    {
      /* Quickly copies recieved data out to console.  Lines over 4096
	 are truncated. */

      Message ms;

      ms.snd_invKey = KR_TEXCON_S;
      ms.snd_key0 = KR_VOID;
      ms.snd_key1 = KR_VOID;
      ms.snd_key2 = KR_VOID;
      ms.snd_rsmkey = KR_VOID;
      ms.snd_data = &mybuf;
      ms.snd_len = msg->rcv_len;
      ms.snd_code = OC_put_char_arry;
      ms.snd_w1 = 0;
      ms.snd_w2 = 0;
      ms.snd_w3 = 0;

      ms.rcv_key0 = KR_VOID;
      ms.rcv_key1 = KR_VOID;
      ms.rcv_key2 = KR_VOID;
      ms.rcv_rsmkey = KR_VOID;
      ms.rcv_data = 0;
      ms.rcv_len = 0;
      ms.rcv_code = 0;
      ms.rcv_w1 = 0;
      ms.rcv_w2 = 0;
      ms.rcv_w3 = 0;

      CALL(&ms);

      msg->snd_code = RC_OK;

      return 1;
    }

  case OC_read_line:
    {
      int i;
      uint32_t j,k;
      
      /* Return spent lines here and get another one if avail */
      if (curln) {
	if (curln->offset == curln->len) {
	  freeLine(curln);
	  curln = 0; /* Set to null so we'll try for a new one */
	}
      }
      
      /* Check if we have a line to read.  Save caller, return to void
	 if we do not */

      if (!curln) {
	saveCaller();
	curln = getLine();
	if (!curln) {
	  saveCaller();
	  curln = getLine();
	  if (curln) {
	    wakeCaller();
	    msg->snd_invKey = KR_VOID;
	    msg->snd_rsmkey = KR_VOID;
	    return 1;
	  }
	  else {
	    msg->snd_invKey = KR_VOID;
	    msg->snd_rsmkey = KR_VOID;
	    return 1;
	  }
	}
      }

      /* Now curln should have a line */

      /* Empty mybuf so we can use it to return a line to the caller
       */
      for (i = 0; i < MAXLEN; i++) {
	mybuf[i] = 0;
      }

      /* Now copy a line of requested length out of the line struct,
	 into mybuf and hand it off to caller. */

      if ((curln->len) > (msg->rcv_w1 + curln->offset))
	j = msg->rcv_w1 + curln->offset;
      else
	j = curln->len;
      /* Return either requested length or greatest amount of line
	 that is available */
      k = 0;
      for (i = curln->offset; i < j; i++) {
	mybuf[k] = curln->chars[i];
	k++;
      }
      curln->offset = i; /* adjust the offset once finished */
      msg->snd_len = k + 1;
      msg->snd_data = &mybuf;
      msg->snd_code = RC_OK;
      return 1;
    }
    
  default:
    break;

  }
  msg->snd_code = RC_UnknownRequest;
  return 1;
}
