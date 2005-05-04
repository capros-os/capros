#include <eros/target.h>
#include <eros/NodeKey.h>
#include <eros/Invoke.h>

#include <domain/ConstructorKey.h>
#include <domain/domdbg.h>
#include <domain/Runtime.h>

#include <domain/drivers/SoundKey.h>

#include "constituents.h"

#define KR_OSTREAM      KR_APP(0)
#define KR_SOUND_C      KR_APP(1)
#define KR_SOUND        KR_APP(2)

//#define VENDOR_VMWARE 0x15ad
//#define VENDOR_INTEL  0x8086

int 
main(void)
{
  Message msg;
  uint32_t result;
  //unsigned short  vendor;
  //  unsigned short device;
  // struct pci_dev_data *rcv_dev=NULL;
  // total = 0;
  //  vendor = VENDOR_VMWARE;
  // vendor = VENDOR_INTEL;

  node_extended_copy(KR_CONSTIT, KC_OSTREAM,   KR_OSTREAM);
  node_extended_copy(KR_CONSTIT, KC_SOUND_C,  KR_SOUND_C);

  constructor_request(KR_SOUND_C,KR_BANK,KR_SCHED,KR_VOID,KR_SOUND_C);

  /********* OC_INIT **************************/

  msg.snd_invKey = KR_SOUND_C;      //Inv_Key
  msg.snd_code   = OC_Sound_Initialize;
  msg.snd_key0   = KR_VOID;
  msg.snd_key1   = KR_VOID;
  msg.snd_key2   = KR_VOID;
  msg.snd_rsmkey = KR_VOID;
  msg.snd_data   = 0;
  msg.snd_len    = 0;
  msg.snd_w1     = 0;
  msg.snd_w2     = 0;
  msg.snd_w3     = 0;
  
  
  msg.rcv_rsmkey = KR_VOID;
  msg.rcv_key0   = KR_VOID;
  msg.rcv_key1   = KR_VOID;
  msg.rcv_key2   = KR_VOID;
  msg.rcv_data   = 0;
  msg.rcv_limit    = 0;
  msg.rcv_code   = 0;
  msg.rcv_w1     = 0;
  msg.rcv_w2     = 0;
  msg.rcv_w3     = 0;
  kprintf(KR_OSTREAM,"Before Sound Initializing routine %s.\n");
  result = CALL(&msg);
  kprintf(KR_OSTREAM,"Invocation for OC_INIT returned %s.\n",(result ==RC_OK) ? "SUCCESS" : "FAILED");

 
  /***********************************/     
  return 0;
}
