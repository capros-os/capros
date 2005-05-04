/* Sample small program: the obligatory ``hello world'' demo. */

#include <eros/target.h>
#include <domain/domdbg.h>
#include <domain/SpaceBankKey.h>
#include <eros/Invoke.h>
#include <eros/KeyConst.h>
#include <eros/NodeKey.h>
#include <eros/ProcessKey.h>
#include <eros/DevicePrivs.h>

#include "constituents.h"

#define KR_CONSTIT  1
#define KR_SELF     2
#define KR_OSTREAM  3
#define KR_BANK     4

#define KR_DEVPRIVS  15
#define KR_PHYSRANGE 16
#define KR_SCRATCH   17
#define KR_MYSPACE   18

int
main(void)
{
  node_copy(KR_CONSTIT, KC_OSTREAM,   KR_OSTREAM);
  node_copy(KR_CONSTIT, KC_DEVPRIVS,  KR_DEVPRIVS);
  node_copy(KR_CONSTIT, KC_PHYSRANGE, KR_PHYSRANGE);

  kprintf(KR_OSTREAM, "hello from physpublish!\n");

  spcbank_buy_nodes(KR_BANK, 1, KR_MYSPACE, KR_VOID, KR_VOID);
  node_make_node_key(KR_MYSPACE, EROS_PAGE_BLSS + 2, KR_MYSPACE);

  process_copy(KR_SELF, ProcAddrSpace, KR_SCRATCH);
  node_swap(KR_MYSPACE, 0, KR_SCRATCH, KR_VOID);
  process_swap(KR_SELF, ProcAddrSpace, KR_MYSPACE, KR_VOID);

  kprintf(KR_OSTREAM, "Address space rebuilt....\n");

#ifdef i386
  /* Try the VGA memory area: */
  devprivs_publishMem(KR_DEVPRIVS, 0xa0000u, 0xc0000u, 0);
#else
#error "Valid test region not known for this architecture!"
#endif

  range_waitobjectkey(KR_PHYSRANGE, OT_DataPage, 
		      (0xb8000u / EROS_PAGE_SIZE) * EROS_OBJECTS_PER_FRAME, KR_SCRATCH);
  
  kprintf(KR_OSTREAM, "Physpage key now in  KR_%d....\n", KR_SCRATCH);

  node_swap(KR_MYSPACE, 1, KR_SCRATCH, KR_VOID);

  kprintf(KR_OSTREAM, "Now in addr space....\n");

  /* Okay, now we go to town. This is a really SLEAZY test case! */
  {
    int i;
    uint16_t *screen = (uint16_t *) (EROS_PAGE_SIZE * EROS_NODE_SIZE);
    uint16_t *pos = screen;
    const char *str = "testme ";
    const char *done = 
      "This concludes the PC ascii console test. "
      "It was good for me. ";

    for(i = 0; i < 80*25*20; i++) {
      if (pos == (screen + 80 * 25))
	pos = screen;

      *pos++ = str[i%7] | 0x700u;	/* character + white on black */
    }

    pos = screen + 80 * 15;
    for (i = 0; i < 80 * 4; i++) {
      *pos++ = ' ' | 0x700u;	/* character + white on black */
    }

    for (pos = screen + 80 * 17; *done; done++, pos++)
      *pos = (*done) | 0x700u;
  }

  return 0;
}
