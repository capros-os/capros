/* Sample small program: the obligatory ``hello world'' sample. */

#include <stddef.h>
#include <eros/target.h>
#include <domain/domdbg.h>

#define KR_SELF     2
#define KR_BANK     4

#define KR_OSTREAM  16

int
main(void)
{
  kprintf(KR_OSTREAM, "hello, world\n");

  return 0;
}
