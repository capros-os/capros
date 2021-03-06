/*
 * Copyright (C) 1998, 1999, 2001, Jonathan S. Shapiro.
 * Copyright (C) 2005, 2007, 2009, Strawberry Development Group
 *
 * This file is part of the CapROS Operating System,
 * and is derived from the EROS Operating System.
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
#include <idl/capros/Sleep.h>
#include <idl/capros/Node.h>
#include <idl/capros/FileServer.h>
#include <idl/capros/File.h>
#include <domain/Runtime.h>
#include <domain/domdbg.h>
#include <domain/assert.h>
#include <domain/ConstructorKey.h>

#define dbg_init    0x1
#define dbg_passes  0x2

/* Following should be an OR of some of the above */
#define dbg_flags   ( 0u )

#define DEBUG(x) if (dbg_##x & dbg_flags)

#define KR_OSTREAM  KR_APP(0)
#define KR_NFILEC   KR_APP(1)
#define KR_SLEEP    KR_APP(2)
#define KR_TMPNODE  KR_APP(3)

#define KR_NFILESRV KR_APP(4)
#define KR_FD0      KR_APP(5)
#define KR_FRO      KR_APP(6)

#define NITER       20

/* MUST use zero stack pages so that seg root doesn't get
   smashed by bootstrap code. */
const uint32_t __rt_stack_pages = 0;
const uint32_t __rt_stack_pointer = 0x20000;

uint32_t buf[capros_key_messageLimit/sizeof(uint32_t)];

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

void
CheckRO(cap_t k)
{
  result_t result;
  uint32_t len;

  result = capros_key_destroy(k);
  assert(result == RC_capros_key_NoAccess);
  result = capros_File_write(k, 0, 1, (uint8_t *)buf, &len);
  assert(result == RC_capros_key_NoAccess);
}

void
FunctionalTests(void)
{
  result_t result;

  result = capros_FileServer_createFile(KR_NFILESRV, KR_FD0);
  assert(result == RC_OK);
  result = capros_File_getReadOnlyCap(KR_FD0, KR_FRO);
  assert(result == RC_OK);
  CheckRO(KR_FRO);

  result = capros_File_getReadOnlyCap(KR_FRO, KR_TEMP0);
  assert(result == RC_OK);
  CheckRO(KR_TEMP0);

  capros_File_fileLocation limit, p, v;
  uint32_t len;
#define BLK 4096
  for (limit = 0; limit < 13*BLK; limit += BLK) {
    // Write the block's address in the block.
    // kprintf(KR_OSTREAM, "About to write block at %#x\n", limit);
    result = capros_File_write(KR_FD0, limit, sizeof(limit), (uint8_t *)&limit,
               &len);
    assert(result == RC_OK);
    assert(len == sizeof(limit));

    // Verify length.
    result = capros_File_getSize(KR_FD0, &p);
    assert(result == RC_OK);
    assert(p == limit + sizeof(limit));

    // Verify contents.
    for (p = 0; p <= limit; p += BLK) {
      result = capros_File_read(KR_FD0, p, sizeof(v), (uint8_t *)&v, &len);
      assert(result == RC_OK);
      assert(len == sizeof(v));
      if (v != p)
        kdprintf(KR_OSTREAM, "Expecting %#x got %#x!\n", p, v);
    }
  }

  result = capros_key_destroy(KR_FD0);
  assert(result == RC_OK);
}

int
main()
{
  result_t result;
  uint32_t len;
  int i, pass, file;
  uint64_t startTime, endTime;
  
  for (i = 0; i < capros_key_messageLimit/sizeof(uint32_t); i++)
    buf[i] = 0xdeadbeef;
  
  DEBUG(init)
    kdprintf(KR_OSTREAM, "About to call nfilec\n");

  result = constructor_request(KR_NFILEC, KR_BANK, KR_SCHED, KR_VOID,
			       KR_NFILESRV);
  DEBUG(init)
    kdprintf(KR_OSTREAM, "Constructor returned ok to reg %d\n", KR_NFILESRV);

  FunctionalTests();

  kprintf(KR_OSTREAM, "Sleep a while\n");
  capros_Sleep_sleep(KR_SLEEP, 2000);
  kprintf(KR_OSTREAM, "Begin tracing\n");

  for (i = 0; i < sizeof(sizes)/sizeof(int); i++) {
    for (pass = 0; pass < NPASS; pass++) {
      DEBUG(passes)
	kprintf(KR_OSTREAM, "sizes[%d] pass %d\n", i, pass);

      result = capros_Sleep_getTimeMonotonic(KR_SLEEP, &startTime);
      if (result != RC_OK)
	kprintf(KR_OSTREAM, "getTime returned %d(0x%x)\n", result);

      DEBUG(passes)
	kdprintf(KR_OSTREAM, "startTime 0x%08x 0x%08x\n",
                 (uint32_t)(startTime>>32), (uint32_t)startTime );

      for (file = 0; file < NITER; file++)
	{
	  result = capros_FileServer_createFile(KR_NFILESRV, KR_FD0);
          assert(result == RC_OK);

          DEBUG(passes)
            kprintf(KR_OSTREAM, "Created nfile %d\n", file);

	  result = capros_File_write(KR_FD0, 0, sizes[i], (uint8_t *)buf, &len);
          assert(result == RC_OK);
          assert(len == sizes[i]);

          DEBUG(passes)
            kprintf(KR_OSTREAM, "Wrote nfile\n");

	  result = capros_Node_swapSlot(KR_TMPNODE, file, KR_FD0, KR_VOID);
          assert(result == RC_OK);
	}
      result = capros_Sleep_getTimeMonotonic(KR_SLEEP, &endTime);

      kprintf(KR_OSTREAM, "%5dK  create: %10u us per iter\n",
              sizes[i], (uint32_t) ((endTime - startTime)/(NITER*1000)) );

#if 0
      kdprintf(KR_OSTREAM, "Done creating\n");
#endif
      result = capros_Sleep_getTimeMonotonic(KR_SLEEP, &startTime);
      for (file = 0; file < NITER; file++)
	{
	  capros_Node_getSlot(KR_TMPNODE, file, KR_FD0);

	  capros_key_destroy(KR_FD0);
	}
      result = capros_Sleep_getTimeMonotonic(KR_SLEEP, &endTime);

      kprintf(KR_OSTREAM, "%5dK  delete: %10u us per iter\n",
              sizes[i], (uint32_t) ((endTime - startTime)/(NITER*1000)) );
    }
  }
  
#if 0
  for (i = 0; i < NITER; i++)
  {
    capros_FileServer_createFile(KR_NFILESRV, KR_BANK, KR_SCHED, KR_FD0);
    capros_Node_swapSlot(KR_TMPNODE, i, KR_FD0, KR_VOID);

    result = capros_File_write(KR_FD0, 0, EROS_PAGE_SIZE, buf, &len);
    if (result != RC_OK || len != EROS_PAGE_SIZE)
      kdprintf(KR_OSTREAM, "nfile_rd: result 0x%x len %d\n", result, len);

    result = capros_File_write(KR_FD0, EROS_PAGE_SIZE, EROS_PAGE_SIZE*2, buf, &len);
    if (result != RC_OK || len != EROS_PAGE_SIZE*2)
      kdprintf(KR_OSTREAM, "nfile_wr: result 0x%x len %d\n", result, len);

    result = capros_File_write(KR_FD0, EROS_PAGE_SIZE*2, EROS_PAGE_SIZE*2, buf, &len);
    if (result != RC_OK || len != EROS_PAGE_SIZE*2)
      kdprintf(KR_OSTREAM, "nfile_rd: result 0x%x len %d\n", result, len);

    capros_key_destroy(KR_FD0);
    
#if 0
    /* Should be able to read 0 bytes from a new file: */
    result = capros_File_read(KR_FD0, 0, 0, buf, &len);
    if (result != RC_OK || len != 0)
      kdprintf(KR_OSTREAM, "nfile_rd: result 0x%x len %d\n", result, len);
#endif
  }
#endif

  kprintf(KR_OSTREAM, "lat_fs: done %d iterations\n", NITER);

  return 0;
}
