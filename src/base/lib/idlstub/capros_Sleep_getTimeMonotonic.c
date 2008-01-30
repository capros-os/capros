#include <stdbool.h>
#include <stddef.h>
#include <alloca.h>
#include <eros/target.h>
#include <eros/Invoke.h>
#include <idl/capros/key.h>
#include <idl/capros/Sleep.h>

result_t
capros_Sleep_getTimeMonotonic(cap_t _self, capros_Sleep_nanoseconds_t *_retVal)
{
  Message msg;

  msg.snd_invKey = _self;
  msg.snd_code = OC_capros_Sleep_getTimeMonotonic;
  msg.snd_w1 = 0;
  msg.snd_w2 = 0;
  msg.snd_w3 = 0;
  msg.snd_len = 0;
  msg.snd_key0 = KR_VOID;
  msg.snd_key1 = KR_VOID;
  msg.snd_key2 = KR_VOID;
  msg.snd_rsmkey = KR_VOID;

  msg.rcv_limit = 0;
  msg.rcv_key0 = KR_VOID;
  msg.rcv_key1 = KR_VOID;
  msg.rcv_key2 = KR_VOID;
  msg.rcv_rsmkey = KR_VOID;

  CALL(&msg);

  if (msg.rcv_code != RC_OK) return msg.rcv_code;
  if (_retVal)
    *_retVal = msg.rcv_w1 + ((uint64_t)msg.rcv_w2 << 32);
  return msg.rcv_code;
}
