#include <_ansi.h>
#include <sys/types.h>

caddr_t
_sbrk_r (int incr)
{
  extern char end;		/* Defined by the linker */
  static char *heap_end;
  char *prev_heap_end;

  if (heap_end == 0) {
      heap_end = (char *)((((unsigned short) &end) + 7) & ~7);
  }
  prev_heap_end = heap_end;
  /* FIXME: Check that we aren't running into the stack. */
  heap_end += incr;

  return (caddr_t) prev_heap_end;
}
