/* syscalls.c - non-trap system calls for D10V
 *
 * This file contains system calls that cannot be implemented with
 * a simple "trap 15" instruction.  The ones that can are in trap.S.
 */

#include <_ansi.h>
#include <sys/types.h>
#if 0
#include <sys/stat.h>
#endif

register char *stack_ptr asm ("sp");

caddr_t
_sbrk_r (int incr)
{
  extern char end;		/* Defined by the linker */
  static char *heap_end;
  char *prev_heap_end;
  char *sp = (char *)stack_ptr;

  if (heap_end == 0)
    {
      heap_end = (char *)((((unsigned short) &end) + 7) & ~7);
    }
  prev_heap_end = heap_end;
#if 0
  /* FIX ASSERT */
  if (heap_end + incr > sp)
    {
      _write (2, "Heap and stack collision\n", sizeof ("Heap and stack collision\n")-1);
      abort ();
    }
#endif
  heap_end += incr;

  return (caddr_t) prev_heap_end;
}
