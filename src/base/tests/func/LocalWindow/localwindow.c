/* Sample small program: the obligatory ``hello world'' demo. */

#include <eros/target.h>
#include <eros/NodeKey.h>
#include <eros/Invoke.h>
#include <eros/KeyConst.h>
#include <eros/NumberKey.h>
#include <eros/ProcessKey.h>
#include <domain/domdbg.h>
#include <domain/Runtime.h>
#include <domain/SpaceBankKey.h>

#include "constituents.h"

#define KR_OSTREAM   KR_APP(0)
#define KR_NEWNODE   KR_APP(1)
#define KR_MYSPACE   KR_APP(2)
#define KR_ALTSPACE  KR_APP(3)

unsigned long value = 0;

/* Local window is a small program. We test it by injecting a node
 * above the initial address space node, inserting a local window key
 * into that new node, and referencing one of our variables via the
 * now-aliased address. 
 */
int
main(void)
{
  unsigned long *pValue = &value;
  unsigned long *npValue;

  nk_value nkv;
  nkv.value[2] = 0;		/* local window, relative to slot zero */
  nkv.value[1] = 0;
  nkv.value[0] = 0;

  node_copy(KR_CONSTIT, KC_OSTREAM, KR_OSTREAM);

  spcbank_buy_nodes(KR_BANK, 1, KR_NEWNODE, KR_VOID, KR_VOID);

  node_make_node_key(KR_NEWNODE, EROS_PAGE_BLSS + 2, 0, KR_NEWNODE);

  spcbank_buy_nodes(KR_BANK, 1, KR_ALTSPACE, KR_VOID, KR_VOID);

  node_make_node_key(KR_ALTSPACE, EROS_PAGE_BLSS + 2, 0, KR_ALTSPACE);

  /* Fetch our address space and stick it in the node. */
  process_copy(KR_SELF, ProcAddrSpace, KR_MYSPACE);
  node_swap(KR_NEWNODE, 0, KR_MYSPACE, KR_VOID);
  node_swap(KR_ALTSPACE, 2, KR_MYSPACE, KR_VOID);
  node_swap(KR_ALTSPACE, 0, KR_MYSPACE, KR_VOID);
  node_swap(KR_NEWNODE, 1, KR_ALTSPACE, KR_VOID);
  process_swap(KR_SELF, ProcAddrSpace, KR_NEWNODE, KR_VOID);

  /* Insert the first window: */
  node_write_number(KR_NEWNODE, 2, &nkv);

  kprintf(KR_OSTREAM, "Value of ul through normal address 0x%08x: %d\n",
	  pValue, *pValue);

  {
    unsigned char *cpValue = (unsigned char *) pValue;
    cpValue += (1 * EROS_NODE_SIZE * EROS_PAGE_SIZE);
    npValue = (unsigned long *)cpValue;
  }

  kprintf(KR_OSTREAM, "Value of ul through alt address 0x%08x: %d\n",
	  npValue, *npValue);

  {
    unsigned char *cpValue = (unsigned char *) pValue;
    cpValue += (2 * EROS_NODE_SIZE * EROS_PAGE_SIZE);
    npValue = (unsigned long *)cpValue;
  }

  kprintf(KR_OSTREAM, "Value of ul through window address 0x%08x: %d\n",
	  npValue, *npValue);

  /* Insert the second window: */
  nkv.value[2] = 1u << EROS_NODE_LGSIZE;
  node_write_number(KR_NEWNODE, 3, &nkv);

  {
    unsigned char *cpValue = (unsigned char *) pValue;
    cpValue += (3 * EROS_NODE_SIZE * EROS_PAGE_SIZE);
    npValue = (unsigned long *)cpValue;
  }

  kprintf(KR_OSTREAM, "Value of ul through 2nd window address 0x%08x: %d\n",
	  npValue, *npValue);

  /* Insert the third window: */
  nkv.value[2] = 1u << EROS_NODE_LGSIZE;
  nkv.value[0] = 2 * EROS_NODE_SIZE * EROS_PAGE_SIZE;
  node_write_number(KR_NEWNODE, 4, &nkv);

  {
    unsigned char *cpValue = (unsigned char *) pValue;
    cpValue += (4 * EROS_NODE_SIZE * EROS_PAGE_SIZE);
    npValue = (unsigned long *)cpValue;
  }

  kprintf(KR_OSTREAM, "Value of ul through 3rd window address 0x%08x: %d\n",
	  npValue, *npValue);

#if 0
  {
    Message msg;
    msg.snd_invKey = KR_RECEIVER;
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

    msg.rcv_key0 = KR_ARG(0);
    msg.rcv_key1 = KR_ARG(1);
    msg.rcv_key2 = KR_ARG(2);
    msg.rcv_rsmkey = KR_RETURN;
    msg.rcv_code = 0;
    msg.rcv_w1 = 0;
    msg.rcv_w2 = 0;
    msg.rcv_w3 = 0;
    msg.rcv_limit = 0;

    kprintf(KR_OSTREAM, "Caller makes first call\n");

    msg.snd_w1 = 1;
    CALL(&msg);

    kprintf(KR_OSTREAM, "Caller makes second call\n");

    msg.snd_w1 = 2;
    CALL(&msg);
  }
#endif

  kprintf(KR_OSTREAM, "localwindow completes\n");

  return 0;
}
