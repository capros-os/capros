/* Sample small program: the obligatory ``hello world'' sample. */

#include <stddef.h>
#include <eros/target.h>
#include <eros/Invoke.h>
#include <domain/Runtime.h>
#include <domain/domdbg.h>
#include <idl/capros/Node.h>

#define KR_OSTREAM  KR_APP(0)

int
main(void)
{
  Message Msg;

  capros_Node_getSlot(KR_CONSTIT, 0, KR_OSTREAM);
  kprintf(KR_OSTREAM, "hello, world\n");

  Msg.snd_invKey = KR_RETURN;
  Msg.snd_key0 = KR_VOID;
  Msg.snd_key1 = KR_VOID;
  Msg.snd_key2 = KR_VOID;
  Msg.snd_rsmkey = KR_VOID;
  Msg.snd_code = 0;
  Msg.snd_w1 = 0;
  Msg.snd_w2 = 0;
  Msg.snd_w3 = 0;
  Msg.snd_len = 0;

  SEND(&Msg);

  return 0;
}
