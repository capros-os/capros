/* Sample small program: the obligatory ``hello world'' demo. */

#include <stddef.h>
#include <eros/target.h>
#include <domain/domdbg.h>
#include <eros/NodeKey.h>

#include "constituents.h"

#define KR_CONSTIT  1
#define KR_SELF     2
#define KR_OSTREAM  3
#define KR_BANK     4

int
main(void)
{
  node_copy(KR_CONSTIT, KC_OSTREAM, KR_OSTREAM);

  kprintf(KR_OSTREAM, "hello, world\n");

  return 0;
}
