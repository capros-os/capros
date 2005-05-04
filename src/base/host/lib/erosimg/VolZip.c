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

#include "zlib.h"

static unsigned char in_buf[EROS_SECTOR_SIZE];
static unsigned char out_buf[EROS_SECTOR_SIZE];

#define COMPRESS_LVL Z_DEFAULT_COMPRESSION
#define min(x,y) ((x) > (y) ? (y) : (x))

static uint32_t
vol_fill_input(Volume *pVol, z_stream *pz, int fd, uint32_t len)
{
  if (pz->avail_in == 0 && len > 0) {
    int sz = min(len, EROS_SECTOR_SIZE);
      
    if (read(fd, in_buf, sz) != sz)
      diag_fatal(3, "Compressed read failed\n");
    
    pz->avail_in = sz;
    pz->next_in = in_buf;
    len -= sz;
  }

  return len;
}

static void
vol_flush_output(Volume *pVol, z_stream *pz, int fd)
{
  int out_count = EROS_SECTOR_SIZE - pz->avail_out;

  if (out_count) {
    if (write(fd, out_buf, out_count) != out_count)
      diag_fatal(3, "Cannot write decompressed image\n");

    pz->next_out = out_buf;
    pz->avail_out = EROS_SECTOR_SIZE;
  }
}

/* If this is a compressed volume, decompress it to a temporary file. */
int
vol_DecompressTarget(Volume *pVol)
{
  uint32_t s;
  int err;
  int mode = Z_NO_FLUSH;
  z_stream z;
  uint32_t len;

  assert (pVol->working_fd == pVol->target_fd);

  if ((pVol->volHdr.BootFlags & VF_COMPRESSED) == 0)
    return 0;			/* success */

  /* Note /working_fd/ may be -1 at this point...  */
  if (pVol->target_fd == pVol->working_fd) {
    char wkname[] = "/tmp/volzip-XXXXXX";
    mkstemp(wkname);

    pVol->working_fd = open(wkname, O_RDWR|O_CREAT, 0666);
    if (pVol->working_fd < 0)
      return errno;

    unlink(wkname);
  }
  
  if (lseek(pVol->target_fd, (int) 0, SEEK_SET) < 0)
    return false;

  /* Copy the bootstrap portion unmodified. */
  for (s = 0; s < pVol->volHdr.BootSectors; s++) {
    if (read(pVol->target_fd, in_buf, EROS_SECTOR_SIZE) != EROS_SECTOR_SIZE)
      diag_fatal(3, "Cannot read compressed image\n");
    
    if (write(pVol->working_fd, in_buf, EROS_SECTOR_SIZE) != EROS_SECTOR_SIZE)
      diag_fatal(3, "Cannot write decompressed image\n");
  }

  /* Now decompress the rest. */

  z.total_in = 0;
  z.avail_in = 0;
  z.next_out = out_buf;
  z.total_out = 0;
  z.avail_out = EROS_SECTOR_SIZE;
  
  z.zalloc = (alloc_func)0;
  z.zfree = (free_func)0;
  z.opaque = (voidpf)0;

  z.msg = NULL;

  err = inflateInit(&z);

  if (err != Z_OK)
    diag_fatal(3, "Could not initialize decompression library\n");

  len = pVol->volHdr.zipLen;

  do {
    mode = Z_NO_FLUSH;

    len = vol_fill_input(pVol, &z, pVol->target_fd, len);

    if (z.avail_in == 0)
      mode = Z_FINISH;

    vol_flush_output(pVol, &z, pVol->working_fd);
  } while ((err = inflate(&z, mode)) == Z_OK);

  vol_flush_output(pVol, &z, pVol->working_fd);

  inflateEnd(&z);

  if (err != Z_STREAM_END)
    diag_fatal(3, "Decompression failed with code %d\n", err);

  return err;
}

int
vol_CompressTarget(Volume *pVol)
{
  int err = Z_OK;
  uint32_t s;
  int mode = Z_NO_FLUSH;
  z_stream z;
  uint32_t len;
  char buf[EROS_PAGE_SIZE];

  if (pVol->volHdr.BootFlags & VF_COMPRESSED) {
    if (pVol->target_fd == pVol->working_fd) {
      /* target_fd is an open file descriptor onto the named file, so
       * we regrettably need to preserve that file descriptor.  First
       * step is therefore to xerox the WORKING file.
       */

      struct stat wkstat;
      char wkname[] = "/tmp/working-XXXXXX";

      fstat(pVol->working_fd, &wkstat);

      mkstemp(wkname);

      pVol->working_fd = open(wkname, O_RDWR|O_CREAT, 0666);
      if (pVol->working_fd < 0)
	return errno;

      unlink(wkname);

      if (lseek(pVol->target_fd, (int) 0, SEEK_SET) < 0)
	diag_fatal(3, "Cannot seek target file\n");

      {
	int len = wkstat.st_size;
	while (len) {
	  int sz = min(len, EROS_SECTOR_SIZE);

	  if (read(pVol->target_fd, in_buf, sz) != sz)
	    diag_fatal(3, "Cannot read to copy image\n");
    
	  if (write(pVol->working_fd, in_buf, sz) != sz)
	    diag_fatal(3, "Cannot write to copy image\n");

	  len -= sz;
	}
      }

      if (lseek(pVol->target_fd, (int) 0, SEEK_SET) < 0)
	diag_fatal(3, "Cannot seek target file\n");

      if (ftruncate(pVol->target_fd, 0) < 0)
	diag_fatal(3, "Cannot truncate target file\n");
    }
  
    assert (pVol->target_fd != -1);
    assert (pVol->working_fd != -1);

    assert (pVol->target_fd != pVol->working_fd);
  
    if (lseek(pVol->working_fd, (int) 0, SEEK_SET) < 0)
      diag_fatal(3, "Cannot seek working file\n");

    if (lseek(pVol->target_fd, (int) 0, SEEK_SET) < 0)
      diag_fatal(3, "Cannot seek target file\n");

    /* Copy the bootstrap portion unmodified. */
    for (s = 0; s < pVol->volHdr.BootSectors; s++) {
      if (read(pVol->working_fd, in_buf, EROS_SECTOR_SIZE) != EROS_SECTOR_SIZE)
	diag_fatal(3, "Cannot read decompressed image\n");
    
      if (write(pVol->target_fd, in_buf, EROS_SECTOR_SIZE) != EROS_SECTOR_SIZE)
	diag_fatal(3, "Cannot write compressed image\n");
    }

    /* Now compress the rest. */

    z.total_in = 0;
    z.avail_in = 0;
    z.next_out = out_buf;
    z.total_out = 0;
    z.avail_out = EROS_SECTOR_SIZE;
  
    z.zalloc = (alloc_func)0;
    z.zfree = (free_func)0;
    z.opaque = (voidpf)0;

    z.msg = NULL;

    err = deflateInit(&z, COMPRESS_LVL);

    if (err != Z_OK)
      diag_fatal(3, "Could not initialize decompression library\n");

    len = pVol->volHdr.VolSectors - pVol->volHdr.BootSectors;
    len *= EROS_SECTOR_SIZE;

    do {
      mode = Z_NO_FLUSH;

      len = vol_fill_input(pVol, &z, pVol->working_fd, len);

      if (z.avail_in == 0)
	mode = Z_FINISH;

      vol_flush_output(pVol, &z, pVol->target_fd);
    } while ((err = deflate(&z, mode)) == Z_OK);

    vol_flush_output(pVol, &z, pVol->target_fd);

    deflateEnd(&z);

    if (err != Z_STREAM_END)
      diag_fatal(3, "Decompression failed with code %d\n", err);

    err = Z_OK;

    assert(pVol->needSyncHdr == 0);
  
    pVol->volHdr.zipLen = z.total_out;
  }
  else {
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

    pVol->volHdr.zipLen = 0;
  }    

  if (lseek(pVol->target_fd, (int) 0, SEEK_SET) < 0)
    diag_fatal(3, "Cannot seek target file\n");

  read(pVol->target_fd, buf, EROS_PAGE_SIZE);

  if (lseek(pVol->target_fd, (int) 0, SEEK_SET) < 0)
    diag_fatal(3, "Cannot seek target file\n");

  memcpy(buf, &pVol->volHdr, sizeof(pVol->volHdr));

  write(pVol->target_fd, buf, EROS_PAGE_SIZE);

  return err;
}

