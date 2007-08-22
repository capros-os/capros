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

#include <stddef.h>
#include <eros/target.h>
#include <eros/KeyConst.h>
#include <eros/Invoke.h>
#include <eros/machine/io.h>

#include <idl/capros/DevPrivs.h>
#include <idl/capros/Sleep.h>
#include <idl/capros/Node.h>
#include <idl/capros/Process.h>
#include <idl/capros/arch/i386/Process.h>

#include <domain/domdbg.h>
#include <domain/ConstructorKey.h>
#include <domain/Runtime.h>

#include <domain/drivers/SoundKey.h>

#include "constituents.h"
#include "soundcard.h"

#define KR_OSTREAM   KR_APP(0)
#define KR_START     KR_APP(1)
#define KR_DEVPRIVS  KR_APP(2)
#define KR_SLEEP     KR_APP(3)
#define KR_MYNODE    KR_APP(4)
#define KR_MYSPACE   KR_APP(5)
#define KR_PHYSRANGE KR_APP(6)
#define KR_SCRATCH   KR_APP(7)
#define KR_SCRATCH2  KR_APP(8)

#define DEBUG        if(1)
#define DEBUG_FULL   if(0)


static bool  ProcessRequest(Message *msg);

/*
Todo Begins ... 
-check to ensure the intialization happens only once
-

End of Todo*/


/* Allocate IRQ given by irq */
uint32_t
AllocIRQ(unsigned int irq) 
{
  uint32_t result;

  result = capros_DevPrivs_allocIRQ(KR_DEVPRIVS, irq, 0);
  if (result != RC_OK) {
    kprintf(KR_OSTREAM, "Alloc of IRQ %d failed\n", irq);
  }
  return result;
}




#if 0
uint32_t sb16_dsp_init()
{
  sb_dev *dev;
  char name[100];
  int mixer22,mixer30;

  DEBUG_FULL kprintf(KR_OSTREAM,"Inside sb16_dsp_init\n");


}  //end of sb16_dsp_init


uint32_t sb16_read_voc_header()
{


}

void sb16_play()
{
  outb(DSP_WRITE,0xD1); //Turn on the DAC speaker




}
#endif

uint32_t sb16_dsp_reset()   //DSP_Reset 
{
  int tmp,loop_i;

  outb(1,DSP_RESET);
  capros_Sleep_sleep(KR_SLEEP,100);
  outb(0,DSP_RESET);
  capros_Sleep_sleep(KR_SLEEP,300);
  
  for(loop_i=0;loop_i<1000 && !(inb(DSP_DATA_AV) & 0x80); loop_i++);
  tmp=inb(DSP_READ);
 
  DEBUG_FULL kprintf(KR_OSTREAM,"DSP_READ Value  %04x\n",tmp);
  
  if(tmp != 0xAA)
    {
      DEBUG kprintf(KR_OSTREAM,"DSP_RESET : Failed to locate the sound card \n");
      return RC_SOUNDCARD_NOT_FOUND;
    }
  
  DEBUG  kprintf(KR_OSTREAM,"DSP_RESET: Success . Found the sound card %04x\n",tmp);

  //  sb16_play();

  return RC_OK;
}

uint32_t sound_initialize()
{
  if (AllocIRQ(IRQ5) != RC_OK) {
   DEBUG kprintf(KR_OSTREAM,"IRQ 5 Alloc ... [FAILED]");
   return RC_IRQALLOC_FAILURE;
  }
  else return sb16_dsp_reset();

}

static bool
ProcessRequest(Message *msg)
{
  switch (msg->rcv_code) {
    
  case OC_Sound_Initialize:
    {
    msg->snd_code = sound_initialize();

    }/*End of OC_INIT_SOUND*/
    break;
  }

  return true;
}



/*************************** MAIN **********************/
int
main(void)
{

  Message msg;

  capros_Node_getSlot(KR_CONSTIT, KC_OSTREAM,   KR_OSTREAM);
  capros_Node_getSlot(KR_CONSTIT, KC_DEVPRIVS,  KR_DEVPRIVS);
  capros_Node_getSlot(KR_CONSTIT, KC_PHYSRANGE, KR_PHYSRANGE);
  capros_Node_getSlot(KR_CONSTIT, KC_SLEEP, KR_SLEEP);
  capros_arch_i386_Process_setIoSpace(KR_SELF, KR_DEVPRIVS);

 /*Make a start key to pass back to the constructor*/
  capros_Process_makeStartKey(KR_SELF,0,KR_START);

  kprintf(KR_OSTREAM,"*******************************************\n");
  kprintf(KR_OSTREAM,"Initializing SOUND DRIVER ...\n");




    msg.snd_invKey   = KR_RETURN;      //Inv_Key
    msg.snd_code   = 0;
    msg.snd_key0   = KR_VOID;
    msg.snd_key1   = KR_VOID;
    msg.snd_key2   = KR_VOID;
    msg.snd_rsmkey = KR_RETURN;
    msg.snd_data   = 0;
    msg.snd_len    = 0;
    msg.snd_w1     = 0;
    msg.snd_w2     = 0;
    msg.snd_w3     = 0;
    

    msg.rcv_rsmkey = KR_RETURN;
    msg.rcv_key0   = KR_VOID;
    msg.rcv_key1   = KR_VOID;
    msg.rcv_key2   = KR_VOID;
    msg.rcv_data   = 0;
    msg.rcv_limit    = 0;
    msg.rcv_code   = 0;
    msg.rcv_w1     = 0;
    msg.rcv_w2     = 0;
    msg.rcv_w3     = 0;

    msg.rcv_limit = 8;
    msg.snd_key0=KR_START;
    RETURN(&msg); /*returning the start key*/

  kprintf(KR_OSTREAM,"got invoked \n");
  /* Ready to process request until termination */
  while(ProcessRequest(&msg)){
    msg.snd_invKey = KR_RETURN;
    kprintf(KR_OSTREAM,"main is available\n");
    RETURN(&msg);
    kprintf(KR_OSTREAM,".... just got invoked again\n");
  }
  return 0;
}

