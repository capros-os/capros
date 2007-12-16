
#include <eros/target.h>
#include <eros/arch/i486/io.h>
#include <domain/domdbg.h>

#define KR_OSTREAM  9

/* It is intended that this should be a small space process. */
const uint32_t __rt_stack_pages = 0;
const uint32_t __rt_stack_pointer = 0x21000;
const uint32_t __rt_unkept = 1; /* do not mess with keeper */

int
main(void)
{
  kprintf(KR_OSTREAM, "IOPL: About to issue IO instruction\n");

  /* Writing anything to port 0x80 is a no-op, so this should be
   * safe. */
  outb(0x5, 0x80);

  kprintf(KR_OSTREAM, "IOPL: Success (no GP fault)\n");

  return 0;
}
