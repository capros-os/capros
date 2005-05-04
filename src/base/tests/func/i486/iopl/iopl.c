/* Sample small program: the obligatory ``hello world'' demo. */

#include <eros/target.h>
#include <eros/i486/io.h>
#include <domain/domdbg.h>

#define KR_SELF     2
#define KR_OSTREAM  3
#define KR_BANK     4

int
main(void)
{
  kprintf(KR_OSTREAM, "IOPL: About to issue IO instruction\n");

  /* Writing anything to port 0x80 is a no-op, so this should be
   * safe. */
  out8(0x5, 0x80);

  kprintf(KR_OSTREAM, "IOPL: Success (no GP fault)\n");

  return 0;
}
