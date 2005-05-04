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

/* This is the core of the ps2 driver. It waits for a signal from
 * the helpers 1/12 for IRQ1/IRQ12 respectively. It then reads the 
 * ps2 h/w Buffer for data corresponding to Keyboard/Mouse(the Aux
 * Device). Calls the appropriate client (keyclient/mouseclient) 
 * passing the Keyboard/Mouse data */

#include <stddef.h>
#include <eros/target.h>
#include <eros/NodeKey.h>
#include <eros/KeyConst.h>
#include <eros/ProcessKey.h>
#include <eros/Invoke.h>
#include <eros/i486/io.h>

#include <idl/eros/key.h>
#include <idl/eros/DevPrivs.h>
#include <idl/eros/Sleep.h>
#include <idl/eros/Ps2.h>
#include <idl/eros/Number.h>

#include <domain/domdbg.h>
#include <domain/ConstructorKey.h>
#include <domain/SpaceBankKey.h>
#include <domain/Runtime.h>

#include "constituents.h"
#include "ps2reader.h"

#define KR_OSTREAM      KR_APP(0)
#define KR_START        KR_APP(1)
#define KR_DEVPRIVS     KR_APP(2)
#define KR_SLEEP        KR_APP(3)
#define KR_HELPER12_C   KR_APP(4)
#define KR_HELPER12_S   KR_APP(5)
#define KR_HELPER1_C    KR_APP(6)
#define KR_HELPER1_S    KR_APP(7)
#define KR_ALTSTART     KR_APP(8)
#define KR_PARKNODE     KR_APP(9)
#define KR_PARKWRAP     KR_APP(10)

#define KR_SCRATCH      KR_APP(20)

/* Park requests on these */
#define KEYCLREAD      1
#define MOUSEREAD      2

/* Function Protoypes*/
int ProcessRequest(Message *);
int parkCall(Message *msg);
int wakeCall(int parkingNo);

/* Global Flag */
int BufferEmpty = 0;
int InitDone = 0;

/* Start the helper threads, helper1 & helper12. Build the two
 * helper processes helper1 & helper12 & pass them our start key
 * so that they can call us every time there is an IRQ */
int 
startHelpers() {
  uint32_t result;
  
  process_make_start_key(KR_SELF,0,KR_START);
  result = constructor_request(KR_HELPER12_C, KR_BANK, KR_SCHED, KR_START,
                                 KR_HELPER12_S);
  if (result != RC_OK) {
    kprintf(KR_OSTREAM, "Ps2reader failed to start HELPER12.\n");
    return 0;
  }
  
  process_make_start_key(KR_SELF,0,KR_START);
  result = constructor_request(KR_HELPER1_C, KR_BANK, KR_SCHED, KR_START,
                               KR_HELPER1_S);
  if (result != RC_OK) {
    kprintf(KR_OSTREAM, "Ps2reader failed to start HELPER1.\n");
    return 0;
  }

  return 1;
}


/* Allocate IRQ given by irq */
uint32_t
AllocIRQ(unsigned int irq) 
{
  uint32_t result;

  result = eros_DevPrivs_allocIRQ(KR_DEVPRIVS, irq);
  if (result != RC_OK) {
    kprintf(KR_OSTREAM, "Alloc of IRQ %d failed\n", irq);
  }
  return result;
}

/* Flush the ps2 h/w buffer */
void
ps2FlushOutBuf(void) 
{
  int cycles = 40;

  /* I'm not sure why we would have 40 bytes in a 20
   * byte keyboard buffer, but stranger things have happened
   * so I'll be redundant.  This can be fixed later if necessary
   */
  do {
     if ((kbdReadStat() & KBS_BufFull) != KBS_BufFull)
      break;
     /* Wait to make sure we have cleared the buffer 
      * thoroughly. Use kbdWaitInput instead */
     kbdWaitInput();
  } while (cycles -- > 0);
  return;
}


/* Check function Get Keyboard ID */
void
kbdIdent(void)
{
  int kbdr[5];
  int i;

  for (i = 0; i < 5; i++) {
    kbdr[i] = 0;
  }
  ps2FlushOutBuf();
  
  i = 0;
  kbdWriteO(KbdOCIDkeyb);
  do {
    kbdr[i] = kbdWaitInput();
    if (kbdr[i] == -1) {
      kbdr[i] = 0;
      break;
    }else {
      i++;
    }
  } while (i < 5);
  kprintf(KR_OSTREAM, "PS2reader: Keyboard ID: 0x%x 0x%x 0x%x 0x%x 0x%x\n",
	  kbdr[0],kbdr[1], kbdr[2], kbdr[3], kbdr[4]);
  return;
}

/* Initialize the keyboard controller and the devices that are attached
 * to it: keyboard and mouse (auxillary device) */
int
kbdInit(void) 
{
  int code;

  ps2FlushOutBuf();
  
  /* With the commented code the ps2 driver works fine on vmware 
   * but on native execution, scan codes seem to be translated to
   * codes I don't understand
   * kbdWriteC(KbdCSelfTest);
   * code = kbdWaitInput();
   * if (code != KBR_STOK) {
   * if (code == -1) {
   * kprintf(KR_OSTREAM,"KEYB:Timed out waiting for Self-Test to complete\n");
   * }
   * else {
   * kprintf(KR_OSTREAM, "KEYB: Self-Test Failed returning: 0x%x\n", code);
   * }
   * return 1;
   * }*/
  
  
  /* Perform a keyboard interface test.  This causes the controller
   * to test the keyboard clock and data lines.  The results of the
   * test are placed in the input buffer.*/
  kbdWriteC(KbdCCtrlTest);
  code = kbdWaitInput();
  if (code != KBR_CTOK) { 
    if (code==-1) kprintf(KR_OSTREAM, "KEYB:Timed out testing controller\n");
    else kprintf(KR_OSTREAM, "KEYB: Controller Test failed ret=0x%x\n", code);
    return 1;
  }
  
  /* Enable the keyboard by allowing the keyboard clock to run */
  kbdWriteC(KbdCEnable);
  
  /* Reset keyboard. If the read times out  then the assumption is 
   * that no keyboard is plugged into the machine.
   * This defaults the keyboard to scan-code set 2. */
  kbdWriteO(KbdOCReset);
  code = kbdWaitInput();
  if (code != KBR_ACK) {
    kprintf(KR_OSTREAM, "Failure on Enable\n");
    return 1;
  }
  code = kbdWaitInput();
  if (code != KBR_BATComp) {
    if (code == -1) kprintf(KR_OSTREAM, "KEYB: Timed out on reset\n");
    else  kprintf(KR_OSTREAM, "Failure on Reset\n");
    return 1;
  }
  
  /* Set keyboard typematic rate to maximum */
  kbdWriteO(KbdSetRate);
  code = kbdWaitInput();
  if(code != KBR_ACK) {
    kprintf(KR_OSTREAM,"Setting Typematic Rate ... [FAILED]");
  }
  kbdWriteO(0x00);
  code = kbdWaitInput();
  if(code != KBR_ACK) {
    kprintf(KR_OSTREAM,"Set MAX Rate ... [FAILED]");
  }
  
  
  /* Check & Enable Auxillary interface */
  kbdWriteC(KbdCCheckAux);
  code = kbdWaitInput();
  if(code != KBR_CTOK) {
    kprintf(KR_OSTREAM,"Mouse interface test ... [FAILED]");
    return 1;
  }
  kbdWriteC(KbdCEnableAux);
  

  /* Set the INT & INT2 flags. When set IRQ1,IRQ12 are generated 
   * when keyboard & mouse data are available respectively*/
  kbdWriteC(0x20);
  code = kbdWaitInput();
  kbdWriteC(0x60);
  kbdWriteO(code | 0x3);
  kprintf(KR_OSTREAM,"Setting IRQ1,IRQ12 Flags ... [DONE]");
  
  ps2FlushOutBuf();

  return 0;
}

/* Keyboard Ping*/
int
kbdPing(void) 
{
  int response;

  ps2FlushOutBuf();
  kbdWriteO(KbdOCEcho);
  response = kbdWaitInput();
  if (response != KbdOCEcho) {
    kprintf(KR_OSTREAM, "PS2READER: Ping error.  Returned 0x%x\n", response);
    return 1;
  }
  else {
    return 0;
  }
}

/* Read Data off the ps2 h/w buffer */
int
kbdReadData(void) 
{
  int data = -1; 
  uint8_t stat = kbdReadStat();

/* Return -1 if no data, otherwise get data */
  if (stat & KBS_BufFull) {
    data = inb(KbdDataPort);
  }
  return data;
}

/* Read the status register */
uint8_t
kbdReadStat(void) 
{
  uint8_t status;

  status = inb(KbdStatusPort);
  return status;
}

/* Set the LEDs, used in caps,num,scroll locks */
int
kbdSetLed(uint8_t leds) 
{
  eros_Sleep_sleep(KR_SLEEP,KBDSLEEP);
  if (kbdWriteO(KbdOCSetLed) != 0) {
    kprintf (KR_OSTREAM,"PS2READER:cTimed out while attempting to set LEDs");
    return 1;
  }
  if (kbdWriteO(leds) != 0) {
    kprintf (KR_OSTREAM,"PS2READER: Timed out while attempting to set LEDs");
  }
  return 0;
}

/* Reads data off ps2 h/w buffer. If data is not available
 * retry later */
int
kbdWaitInput(void) 
{
  /* Return data if available.
   * Return -1 if timed out waiting */
  long tmout = KBTMOUT;
  int data = -1;
  uint8_t stat;

  /* It seems the native hardware don't like banged at fast pace
   * for data. So we slow down a bit */
  do {
    stat = kbdReadStat();
    if (stat & KBS_BufFull) {
      data = (int) kbdReadData();
      return data;
    }
    else {
      tmout--;
      eros_Sleep_sleep(KR_SLEEP,KBDSLEEP);
    }
  } while (tmout > 0);

  return data;
}

/* Writes a command to the keyboard controller.
 * User is responsible for checking status of command
 * (Thru kbdReadData or what have you...)
 */
void
kbdWriteC(uint8_t cmd) 
{
  outb(cmd, KbdCtrlPort);
  return;
}
 
/* This writes out a command to the keyboard itself.
 * To do this we first have to check whether we can write
 * to the keyboard buffer and from there we proceed.
 */
int
kbdWriteO(uint8_t cmd) 
{
  uint8_t status;
  int wait = 100;
  status = kbdReadStat();
  while ((status & KBS_NotReady) && (wait-- > 0)) {
    eros_Sleep_sleep(KR_SLEEP,10);
    status = kbdReadStat();
  }
  if (wait == 0) {
    kprintf(KR_OSTREAM, "Timed out waiting for keyboard\n");
    return 1;
  }
  else {
    outb(cmd, KbdOutPort);
    return 0;
  }
}

/* Process Request logic */
int
ProcessRequest(Message *msg) 
{
  int status;

  msg->snd_len  = 0;
  msg->snd_key0 = KR_VOID;
  msg->snd_key1 = KR_VOID;
  msg->snd_key2 = KR_VOID;
  msg->snd_rsmkey = KR_RETURN;
  msg->snd_code   = RC_OK;
  msg->snd_w1 = 0;
  msg->snd_w2 = 0;
  msg->snd_w3 = 0;
  msg->snd_invKey = KR_RETURN;
    
  switch (msg->rcv_code) {
    /* Initialize the ps2 hardware */
  case OC_eros_Ps2_initPs2:
    {
      if(InitDone) {
	msg->snd_code = RC_OK;
	return 1;
      }
      if (kbdInit() != 0) {
	kprintf(KR_OSTREAM,"kbdInit ... [FAILED]");
	msg->snd_code = RC_eros_Ps2_KbdInitFailure;
      }
      
      if (mouseInit() != 0) {
	kprintf(KR_OSTREAM,"MouseInit ... [FAILED]");
	msg->snd_code = RC_eros_Ps2_MouseInitFailure;
      }
      
      /* Print out the identity of the keyboard */
      kbdIdent();
      
      Is4Button = isIntelliMouse();
      if(Is4Button) kprintf(KR_OSTREAM, "4 button....[SUCCESS]\n");

      /* Set default Sampling, Resolution & Scaling parameters.
       * Caution: The sleep time in kbdwaitinput is dependent on the 
       * sampling rate & the resolution.As sometimes the H/W needs to 
       * recuperate after calls.May have to play with the sleep time
       * in kbdwaitinput to get the mouse working again on all systems.
       */
      setSampleRate(0x64u);
      setResolution(0x03u);  
      setScaling();
      
      //setSampleRate(0x14u);
      //setResolution(0x02u);  
            
      /* Start the helpers to wake us up */
      if(!startHelpers()) {
     	kprintf(KR_OSTREAM,"Constructing Helpers ... [FAILED]");
	msg->snd_code = RC_eros_Ps2_HelperInitFailure;
      }
      
      /* Allocate Keyboard & Mouse IRQs and we are done */
      if (AllocIRQ(KBD_IRQ) != RC_OK) {
	msg->snd_code = RC_eros_Ps2_KbdAllocFailure;
	kprintf(KR_OSTREAM,"KBD IRQ Alloc ... [FAILED]");
      }
      
      if (AllocIRQ(MOUSE_IRQ) != RC_OK) {
	msg->snd_code = RC_eros_Ps2_MouseAllocFailure;
	kprintf(KR_OSTREAM,"MOUSE IRQ Alloc ... [FAILED]");
      }
      
      ps2FlushOutBuf();
      
      InitDone = 1;
      return 1;
    }
    
  case OC_eros_Ps2_irqArrived:
    {
      /* We have been called by the IRQ helper processes. Wake 
       * up the appropriate clients ( mouseclient for IRQ12 &
       * keyclient for IRQ1 ). These were earlier parked ( if at
       * all ) on the park node.*/
      
      if(msg->rcv_w1 == eros_Ps2_IRQ1)  wakeCall(KEYCLREAD);
      if(msg->rcv_w1 == eros_Ps2_IRQ12) wakeCall(MOUSEREAD);
      
      return 1;
    }
    
  case OC_eros_Ps2_getMousedata:
    msg->rcv_w1 = MOUSEREAD;
    goto READ_PS2_BUFFER;
  case OC_eros_Ps2_getKeycode:
    {
      /* Now that we have been signalled by the helpers and have 
       * wake up  the appropriate client. Read the ps2 h/w buffer
       * and return the data to the proper client */
      msg->rcv_w1 = KEYCLREAD;
    
      /* Reading of h/w buffer */
    READ_PS2_BUFFER:
    
      status = kbdReadStat();
      if((status & KBS_BufFull) != 0x1u) {
	
#if 0
	kprintf(KR_OSTREAM,"CALLER = %d,Buffer not full",msg->rcv_w1);
	if(msg->rcv_w1 == KEYCLREAD) kprintf(KR_OSTREAM,"CALLER = KEYCL");
	if(msg->rcv_w1 == MOUSEREAD) kprintf(KR_OSTREAM,"CALLER = MOUSE");

#endif
	msg->snd_code = RC_OK;
	msg->snd_w1 = -1;
	
	/* There is no data in the h/w buffer. So park the clients */
	if(msg->rcv_w1 == MOUSEREAD) parkCall(msg);
	else {
	  /* The xlate logic needs this for End Of Keycodes signalling */
	  if(BufferEmpty) parkCall(msg); 
	  BufferEmpty = 1;
	}
	
	/* The ps2reader needs to signal End Of H/W buffer to its clients
	 * We cannot park on empty buffer state. To signal this we
	 * park on the next read of the buffer if the buffer is still
	 * empty. BufferEmpty is a global flag to do just that */
	return 1;
      }  
      
      if(msg->rcv_w1 == KEYCLREAD) BufferEmpty = 0;
      
      /* Parity error in data from the h/w buffer */
      if((status & KBS_PERR) == KBS_PERR) {
	msg->snd_code = RC_OK;
	msg->snd_w1 = -1;
	parkCall(msg);
	return 1;
      }

      /* This is mouse data */
      if((status & KBS_AuxB) == KBS_AuxB) { 
	/* If we have been invoked instead by the keyboard client back off */
	if(msg->rcv_w1==KEYCLREAD) {
	  parkCall(msg);
	  wakeCall(MOUSEREAD);
	  return 1;
	}else {
	  msg->snd_code = RC_OK;
	  msg->snd_w1 = kbdWaitInput();
	  msg->snd_w3 = Is4Button;
	  return 1;
	}
      }else {
	/* This is the keyboard data.If we have been invoked
	 * by the mouse client back off */
	if(msg->rcv_w1==MOUSEREAD) {
	  parkCall(msg);
	  wakeCall(KEYCLREAD);
	  return 1;
	}	
	msg->snd_code = RC_OK;
 	msg->snd_w1 = kbdWaitInput();
	return 1;
      }
      msg->snd_code = RC_OK;
      msg->snd_w1 = -1;
      parkCall(msg); 
      return 1;
    }
    
    /* Set the LEDs on the keyboard */
  case OC_eros_Ps2_setLed:
    {
      uint8_t led = (int8_t)msg->rcv_w1;
      int resp = kbdSetLed(led);
      
      if (resp != 0) {
	msg->snd_code = RC_eros_Ps2_KbdTimeout;
      }else {
	msg->snd_code = RC_OK;
      }
      return 1;
    }
    
    /* Flush the ps2 h/w buffer */
  case OC_eros_Ps2_flushBuffer:
    {
      ps2FlushOutBuf();
      msg->snd_code = RC_OK;
      return 1;
    }

  default:
    break;
  }
  
  msg->snd_code = RC_eros_key_UnknownRequest;
  return 1;
}

/* Sets 2:1 y:x scaling for the mouse*/
void 
setScaling() {
  kbdWriteC(KbdCAuxDev);
  kbdWriteO(Mouse_SCALESET);
  mouseAck("Scaling");
}


/* This function receives the Acks (0xFA) sent by the mouse */
int 
mouseAck(char *str) {
  int ack = 0;
  int status;
  int mouseData = 0;

  while(!mouseData) {
    status = kbdReadStat();
    if ((status & KBS_BufFull) != KBS_BufFull) continue;
    if((status & KBS_AuxB)==0x20u) {
      mouseData = 1;
    }
    ack = kbdWaitInput();
  }

  if(ack!=0xFA) kprintf(KR_OSTREAM,"%s ... [ACK FAILED-%x]",str,ack);
  else kprintf(KR_OSTREAM,"%s ... [DONE]",str);
  return ack;
}

/* Inits the mouse in stream mode (Enabled)*/
int 
mouseInit() {
  int BAT = 0,EN = 0;
  
  while (BAT!=0xAA) {
    kbdWriteC(KbdCAuxDev);
    kbdWriteO(Mouse_RESET);
    mouseAck("Mouse Reset::Ack");
    BAT = mouseAck("Mouse Reset::Self Test");
    mouseAck("Mouse Reset::GETID");
  }
  while (EN!=0xFAu) {
    kbdWriteC(KbdCAuxDev);
    kbdWriteO(Mouse_ENABLE);
    EN = mouseAck("Mouse Enable");
  }
  return 0;
}


/* Set the mouse sampling rate. Possible values: 
           0Ah = 10 samples per second; 
           14h = 20 
           28h = 40
           3Ch = 60 
           50h = 80 
           64h = 100 
           C8h = 200 */
void 
setSampleRate(int sampleRate) {
  kbdWriteC(KbdCAuxDev);
  kbdWriteO(Mouse_SAMPLE);
  mouseAck("Sampling command");

  kbdWriteC(KbdCAuxDev);
  kbdWriteO(sampleRate);
  mouseAck("Sampling Rate");
}

/* Set the mouse resolution. Possible values: 
     00h = 1 count per mm
     01h = 2 counts per mm
     02h = 4 counts per mm
     03h = 8 counts per mm */
void 
setResolution(int res){
  kbdWriteC(KbdCAuxDev);
  kbdWriteO(Mouse_RES);
  mouseAck("Set Resolution Command");

  kbdWriteC(KbdCAuxDev);
  kbdWriteO(res);
  mouseAck("Resolution Rate");
}

/* Returns the statistics of the mouse 3-byte format */
MouseStats 
reqStatus(void){
  MouseStats ms;

  kbdWriteC(KbdCAuxDev);
  kbdWriteO(Mouse_STATREQ);
  mouseAck("Status Ack");
  
  /* FIX ME:: Need to check that the data is not from the keyboard
   * It works for now as we assume that we would do this only in the
   * initialization stage.
   */
  ms.R1 = kbdWaitInput();
  ms.R2 = kbdWaitInput();
  ms.R3 = kbdWaitInput();
  kprintf(KR_OSTREAM,"REQ STATUS::R1 = %d R2 = %d R3 = %d",ms.R1,ms.R2,ms.R3);
  return ms;
}

/* Checks if the installed mouse is an intellimouse i.e 
 * with a scroll wheel(4-button). Look up ps2 manual for
 * sequence of instructions to be sent to identify mouse type */
int 
isIntelliMouse() {
  int isIntelliMouse = 0;
  int deviceID;
  
  setSampleRate(0xC8u);//200
  setSampleRate(0x64u);//100
  setSampleRate(0x50u);//80

  kbdWriteC(KbdCAuxDev);
  kbdWriteO(Mouse_GETID);
  mouseAck("GETID Issued");

  deviceID = kbdWaitInput();
  kprintf(KR_OSTREAM,"DEVICE ID RET = %d",deviceID);

  if(deviceID == 0x03u)
    isIntelliMouse = 1;

  return isIntelliMouse;
}

/* Checks if the installed mouse is an intellimouse
 * i.e with a scroll wheel(5-button) */
int 
isScrollMouse() {
  int isscroll = 0;
  int deviceID;

  setSampleRate(0xC8u);//200
  setSampleRate(0xC8u);//200
  setSampleRate(0x50u);//80

  kbdWriteC(KbdCAuxDev);
  kbdWriteO(Mouse_GETID);
  mouseAck("GETID Issued");

  deviceID = kbdWaitInput();
  kprintf(KR_OSTREAM,"DEVICE ID RET = %d",deviceID);

  if(deviceID == 0x04u)
    isscroll = 1;

  return isscroll;
}

/* We have a node from the spcbank which we have turned into
 * a "parknode". Here we park our client */
int 
parkCall(Message *msg) {
   msg->invType = IT_Retry;
   msg->snd_w1 = RETRY_SET_LIK|RETRY_SET_WAKEINFO;
   msg->snd_w2 = msg->rcv_w1; /* wakeinfo value */
   msg->snd_key0 = KR_PARKWRAP;
  
  return 1;
}

/* We wake up if any bit is set in node_wake...So all the andBits,
 * orBits & match are the same. Now the client given by the parkingNo
 * is woken up and free to retry its call */
int 
wakeCall(int parkingNo) {
  
  node_wake_some_no_retry(KR_PARKNODE,parkingNo,parkingNo,parkingNo);
  return 1;
}


int
main(void)
{
  int result;
  Message msg;
    
  node_extended_copy(KR_CONSTIT, KC_OSTREAM, KR_OSTREAM);
  node_extended_copy(KR_CONSTIT, KC_DEVPRIVS, KR_DEVPRIVS);
  node_extended_copy(KR_CONSTIT, KC_SLEEP, KR_SLEEP);
  node_extended_copy(KR_CONSTIT, KC_HELPER12, KR_HELPER12_C);
  node_extended_copy(KR_CONSTIT, KC_HELPER1, KR_HELPER1_C);
  
  /* Move the DEVPRIVS key to the ProcIoSpace slot so we can do io calls */
  process_swap(KR_SELF, ProcIoSpace, KR_DEVPRIVS, KR_VOID);
  process_make_start_key(KR_SELF, 0, KR_START);
  process_make_start_key(KR_SELF, 1, KR_ALTSTART);
  
  /* We buy a node from the spacebank to make a "park-node" on which 
   * clients can be redirected to wait. We can then selectively wake 
   * up these parked clients when the appropriate trigger fires */
  
  result = spcbank_buy_nodes(KR_BANK, 1, KR_PARKNODE, KR_VOID, KR_VOID);
  if(result != RC_OK){
    kprintf(KR_OSTREAM,"Ps2reader:: Buying node ... [FAILED]");
    return 0;
  }
  
  result = node_make_wrapper_key(KR_PARKNODE,0,0,KR_PARKWRAP);
  if(result != RC_OK){
    kprintf(KR_OSTREAM,"Ps2reader:: Making wrapper key ... [FAILED]");
    return 0;
  }
  
  {
    eros_Number_value nkv;
    nkv.value[0] = WRAPPER_BLOCKED;
    nkv.value[1] = 0;
    nkv.value[2] = 0;
    
    node_write_number(KR_PARKNODE, WrapperFormat, &nkv);
  }
  
  node_swap(KR_PARKNODE, WrapperKeeper, KR_ALTSTART, KR_VOID);
  
  msg.snd_invKey = KR_RETURN;
  msg.snd_key0   = KR_ALTSTART;
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
  msg.rcv_key1   = KR_VOID;
  msg.rcv_key2   = KR_VOID;
  msg.rcv_rsmkey = KR_RETURN;
  msg.rcv_data = 0;
  msg.rcv_limit  = 0;
  msg.rcv_code = 0;
  msg.rcv_w1 = 0;
  msg.rcv_w2 = 0;
  msg.rcv_w3 = 0;
  msg.invType = IT_PReturn;
  
  do {
    INVOKECAP(&msg);
    msg.snd_invKey = KR_RETURN;
    msg.snd_key0 = KR_ALTSTART;
    msg.snd_rsmkey = KR_RETURN;
    msg.invType = IT_PReturn;
  } while (ProcessRequest(&msg));

  return 0;
}
