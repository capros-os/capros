/*
 * Copyright (C) 1998, 1999, Jonathan S. Shapiro.
 *
 * This file is part of the EROS Operating System.
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

#include <sys/fcntl.h>
#include <sys/stat.h>

#include <assert.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include <errno.h>

#include <erosimg/App.h>
#include <erosimg/Volume.h>

static unsigned char in_buf[EROS_SECTOR_SIZE];

#define min(x,y) ((x) > (y) ? (y) : (x))

int
vol_CompressTarget(Volume *pVol)
{
  char buf[EROS_PAGE_SIZE];

  {
    if (pVol->target_fd != pVol->working_fd) {
      /* Need to copy the decompressed image into the target file. */

      struct stat wkstat;
      fstat(pVol->working_fd, &wkstat);

      if (ftruncate(pVol->target_fd, 0) < 0)
	diag_fatal(3, "Cannot truncate target file\n");

      if (lseek(pVol->target_fd, (int) 0, SEEK_SET) < 0)
	diag_fatal(3, "Cannot seek target file\n");

      if (lseek(pVol->working_fd, (int) 0, SEEK_SET) < 0)
	diag_fatal(3, "Cannot seek working file\n");

      {
	int len = wkstat.st_size;
	while (len) {
	  int sz = min(len, EROS_SECTOR_SIZE);

	  if (read(pVol->working_fd, in_buf, sz) != sz)
	    diag_fatal(3, "Cannot read to copy image\n");
    
	  if (write(pVol->target_fd, in_buf, sz) != sz)
	    diag_fatal(3, "Cannot write to copy image\n");

	  len -= sz;
	}
      }
    }

  }    

  /* Copy volHdr to target file. */
  if (lseek(pVol->target_fd, (int) 0, SEEK_SET) < 0)
    diag_fatal(3, "Cannot seek target file\n");

  read(pVol->target_fd, buf, EROS_PAGE_SIZE);

  if (lseek(pVol->target_fd, (int) 0, SEEK_SET) < 0)
    diag_fatal(3, "Cannot seek target file\n");

  memcpy(buf, &pVol->volHdr, sizeof(pVol->volHdr));

  write(pVol->target_fd, buf, EROS_PAGE_SIZE);

  return 0;
}

