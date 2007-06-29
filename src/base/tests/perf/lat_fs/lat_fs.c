/*
 * Copyright (C) 1998, 1999, 2001, Jonathan S. Shapiro.
 * Copyright (C) 2005, 2007, Strawberry Development Group
 *
 * This file is part of the CapROS Operating System.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2,
 * or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */
/* This material is based upon work supported by the US Defense Advanced
Research Projects Agency under Contract No. W31P4Q-07-C-0070.
Approved for public release, distribution unlimited. */

#include <eros/target.h>
#include <eros/Invoke.h>
#include <idl/eros/Sleep.h>
#include <eros/NodeKey.h>
#include <domain/domdbg.h>
#include <domain/ConstructorKey.h>
#include <domain/NFileKey.h>

#define dbg_init    0x1
#define dbg_passes  0x2

/* Following should be an OR of some of the above */
#define dbg_flags   ( 0u )

#define DEBUG(x) if (dbg_##x & dbg_flags)

#define KR_NFILEC   1
#define KR_SELF     2
#define KR_SCHED    3
#define KR_BANK     4
#define KR_OSTREAM  5
#define KR_SYSTRACE 6
#define KR_MYSEG    8
#define KR_NFILESRV 9
#define KR_SCRATCH  10
#define KR_TMPNODE  11

#define KR_FD       12
#define KR_FD0      12
#define KR_FD1      13

#define KR_SLEEP    27


#define NITER       20

/* MUST use zero stack pages so that seg root doesn't get
   smashed by bootstrap code. */
const uint32_t __rt_stack_pages = 0;
const uint32_t __rt_stack_pointer = 0x20000;

uint32_t buf[EROS_MESSAGE_LIMIT/sizeof(uint32_t)];

#ifdef NDEBUG
#define assert(ignore) ((void) 0)
#else

extern int __assert(const char *, const char *, int);

#define assert(expression)  \
  ((void) ((expression) ? 0 : __assert (#expression, __FILE__, __LINE__)))

int __assert(const char *expr, const char *file, int line)
{
  kdprintf(KR_OSTREAM, "%s:%d: Assertion failed: '%s'\n",
	   file, line, expr);
  return 0;
}
#endif

void
setup()
{
  uint32_t result;
  
  DEBUG(init)
    kdprintf(KR_OSTREAM, "About to call nfilec\n");

  result = constructor_request(KR_NFILEC, KR_BANK, KR_SCHED, KR_VOID,
			       KR_NFILESRV);
  DEBUG(init)
    kdprintf(KR_OSTREAM, "Constructor returned ok to reg %d\n", KR_NFILESRV);
}

#define NPASS 3
#define BENCHMARK
static int sizes[] = { 0,
		       1 * 1024,
#ifndef BENCHMARK
		       2 * 1024,
		       3 * 1024,
#endif
		       4 * 1024,
#ifndef BENCHMARK
		       5 * 1024,
		       6 * 1024,
		       7 * 1024,
		       8 * 1024,
		       9 * 1024,
#endif
		       10 * 1024
};

int
main()
{
  uint32_t result;
  uint32_t len;
  int i, pass, file;
  uint64_t startTime, endTime;
  
  for (i = 0; i < EROS_MESSAGE_LIMIT/sizeof(uint32_t); i++)
    buf[i] = 0xdeadbeef;
  
  setup();

  kprintf(KR_OSTREAM, "Sleep a while\n");
  eros_Sleep_sleep(KR_SLEEP, 2000);
  kprintf(KR_OSTREAM, "Begin tracing\n");

  for (i = 0; i < sizeof(sizes)/sizeof(int); i++) {
    for (pass = 0; pass < NPASS; pass++) {
      DEBUG(passes)
	kprintf(KR_OSTREAM, "pass %d\n", pass);

      result = eros_Sleep_getTimeMonotonic(KR_SLEEP, &startTime);
      if (result != RC_OK)
	kprintf(KR_OSTREAM, "getTime returned %d(0x%x)\n", result);

      DEBUG(passes)
	kdprintf(KR_OSTREAM, "startTime 0x%08x 0x%08x\n",
                 (uint32_t)(startTime>>32), (uint32_t)startTime );

      for (file = 0; file < NITER; file++)
	{
	  result = nfile_create(KR_NFILESRV, KR_FD0);
          assert(result == RC_OK);

          DEBUG(passes)
            kdprintf(KR_OSTREAM, "Created nfile\n");

	  result = nfile_write(KR_FD0, 0, sizes[i], buf, &len);
          assert(result == RC_OK);

          DEBUG(passes)
            kdprintf(KR_OSTREAM, "Wrote nfile\n");

	  result = node_swap(KR_TMPNODE, file, KR_FD0, KR_VOID);
          assert(result == RC_OK);
	}
      result = eros_Sleep_getTimeMonotonic(KR_SLEEP, &endTime);

      kprintf(KR_OSTREAM, "%5dK  create: %10u us per iter\n",
              sizes[i], (uint32_t) ((endTime - startTime)/(NITER*1000)) );

#if 0
      kdprintf(KR_OSTREAM, "Done creating\n");
#endif
      result = eros_Sleep_getTimeMonotonic(KR_SLEEP, &startTime);
      for (file = 0; file < NITER; file++)
	{
	  node_copy(KR_TMPNODE, file, KR_FD0);

	  nfile_destroy(KR_FD0);
	}
      result = eros_Sleep_getTimeMonotonic(KR_SLEEP, &endTime);

      kprintf(KR_OSTREAM, "%5dK  delete: %10u us per iter\n",
              sizes[i], (uint32_t) ((endTime - startTime)/(NITER*1000)) );
    }
  }
  
#if 0
  for (i = 0; i < NITER; i++)
  {
    nfile_create(KR_NFILESRV, KR_FD0);
    node_swap(KR_TMPNODE, i, KR_FD0, KR_VOID);

    result = nfile_write(KR_FD0, 0, EROS_PAGE_SIZE, buf, &len);
    if (result != RC_OK || len != EROS_PAGE_SIZE)
      kdprintf(KR_OSTREAM, "nfile_rd: result 0x%x len %d\n", result, len);

    result = nfile_write(KR_FD0, EROS_PAGE_SIZE, EROS_PAGE_SIZE*2, buf, &len);
    if (result != RC_OK || len != EROS_PAGE_SIZE*2)
      kdprintf(KR_OSTREAM, "nfile_wr: result 0x%x len %d\n", result, len);

    result = nfile_write(KR_FD0, EROS_PAGE_SIZE*2, EROS_PAGE_SIZE*2, buf, &len);
    if (result != RC_OK || len != EROS_PAGE_SIZE*2)
      kdprintf(KR_OSTREAM, "nfile_rd: result 0x%x len %d\n", result, len);

    nfile_destroy(KR_FD0);
    
#if 0
    /* Should be able to read 0 bytes from a new file: */
    result = nfile_read(KR_FD0, 0, 0, buf, &len);
    if (result != RC_OK || len != 0)
      kdprintf(KR_OSTREAM, "nfile_rd: result 0x%x len %d\n", result, len);
#endif
  }
#endif

  kprintf(KR_OSTREAM, "lat_fs: done %d iterations\n", NITER);

  return 0;
}
