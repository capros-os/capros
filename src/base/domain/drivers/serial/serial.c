#include <eros/target.h>
#include <eros/NumberKey.h>
#include <domain/domdbg.h>

static int x;

#define KR_WHOAMI  6
#define KR_OSTREAM 7

#define IAM_UPPER 1
#define IAM_LOWER 0

/* We'll be needing this at some point */
#define SIO_BASE 0x3f8

int
main()
{
  uint32_t me;

  (void) number_get_word(KR_WHOAMI, &me);

  if (me == IAM_LOWER) {
    kprintf(KR_OSTREAM, "I am lower (me==%d)\n", me);
  }
  else {
    kprintf(KR_OSTREAM, "I am upper (me==%d)\n", me);
  }

  return (int)&x;
}
