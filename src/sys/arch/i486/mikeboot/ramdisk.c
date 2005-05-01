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

#include <disk/LowVolume.h>
#include <kerninc/BootInfo.h>
#include "boot-asm.h"
#include "boot.h"
#include <zlib/zlib.h>

uint32_t RamDiskAddress = 0;

void
LoadRamDisk(BootInfo * bi)
{
  uint32_t outSectors = 0;
  uint32_t memSectors = VolSectors;
  uint8_t *pdest;
  uint32_t in_sectors;
  VolHdr *rd_vh;

  /* Round up to a page multiple: */
  memSectors += (EROS_PAGE_SECTORS - 1);
  memSectors -= (memSectors % EROS_PAGE_SECTORS);
  
  /* Allocate space for the ram disk: */
  pdest = 
    (uint8_t *) BootAlloc(memSectors * EROS_SECTOR_SIZE, EROS_PAGE_SIZE);

  /* In this case, pdest really should be the physical destination. */
  pdest = BOOT2PA(pdest, uint8_t *);
  RamDiskAddress = (uint32_t) pdest;

  BindRegion(bi, PtoKPA(pdest), memSectors * EROS_SECTOR_SIZE, MI_RAMDISK);

  if (VolFlags & VF_COMPRESSED) {
#ifdef SUPPORT_COMPRESSED
#define IN_BUF_SECS 24
#define OUT_BUF_SECS 8
#define IN_BUF_SZ (IN_BUF_SECS * EROS_SECTOR_SIZE)
#define OUT_BUF_SZ (OUT_BUF_SECS * EROS_SECTOR_SIZE)

    unsigned char *in_buf = (unsigned char *) malloc(IN_BUF_SZ);
    unsigned char *out_buf = (unsigned char *) malloc(OUT_BUF_SZ);

#if 0
    printf("in_buf: 0x%x out_buf: 0x%x\n", (unsigned) in_buf,
	   (unsigned) out_buf);
#endif
    
    in_sectors = ZipLen + EROS_SECTOR_SIZE - 1;
    in_sectors /= EROS_SECTOR_SIZE;
    printf(">> Loading %ld compressed (%ld decompressed) sectors\n"
	   "      into ramdisk at 0x%x... ",
	   in_sectors, VolSectors, pdest);

    /* I originally thought to just copy the in-memory bootstrap into
       high memory, but this would not mirror the bits on the disk
       correctly because by this point the state of the .data and .bss
       sections has changed.  At this time that would not impact
       anything much, but in the future I want to be able to warm boot
       the system, and it seems like a bad plan to have a modified
       ramdisk image.  I cannot point to anything specific, but the
       cost of reading the extra 32k to 64k is small enough that I
       don't care about it. */
       
    read_sectors(bi->bootDrive, bi->bootStartSec,
		 DISK_BOOTSTRAP_SECTORS, (void *) pdest); 

    /* Turn off the compressed image bit in the ramdisk bootstrap
       header... */
    rd_vh = PA2BOOT(pdest, VolHdr *);
    rd_vh->BootFlags &= ~VF_COMPRESSED;
    
    pdest += EROS_SECTOR_SIZE * DISK_BOOTSTRAP_SECTORS;

    if (BootSectors != DISK_BOOTSTRAP_SECTORS) {
      printf("BootSectors and DISK_BOOTSTRAP_SECTORS do not match!\n");
      halt();
    }
    
    /* Decompress the volume */
    {
      int err;
      int mode;
      uint32_t len;
      uint32_t cur_sec;
      z_stream z;

      z.total_in = 0;
      z.avail_in = 0;
      z.next_out = out_buf;
      z.total_out = 0;
      z.avail_out = OUT_BUF_SZ;
  
      z.zalloc = (alloc_func)0;
      z.zfree = (free_func)0;
      z.opaque = (voidpf)0;
    
      z.msg = NULL;

      err = inflateInit(&z);

      if (err != Z_OK) {
	printf("Could not initialize decompression library\n");
	halt();
      }

      len = ZipLen;
      cur_sec = DISK_BOOTSTRAP_SECTORS;

      do {
	mode = Z_NO_FLUSH;

	if (z.avail_in == 0 && len) {
	  read_sectors(bi->bootDrive, bi->bootStartSec + cur_sec, IN_BUF_SECS,
		       BOOT2PA(in_buf, void *));
	  z.avail_in = min(len, IN_BUF_SZ);
	  z.next_in = in_buf;
	  len -= z.avail_in;
	  cur_sec += IN_BUF_SECS;
	}

	if (z.avail_in == 0)
	  mode = Z_FINISH;

	if (z.avail_out == 0) {
	  ppcpy(BOOT2PA(out_buf, void *), (void *) pdest, OUT_BUF_SZ);
	  pdest += OUT_BUF_SZ;
	  z.avail_out = OUT_BUF_SZ;
	  z.next_out = out_buf;

	  outSectors += OUT_BUF_SECS;
	}
      } while ((err = inflate(&z, mode)) == Z_OK);

      if (z.avail_out < OUT_BUF_SZ) {
	int len = OUT_BUF_SZ - z.avail_out;
      
	ppcpy(BOOT2PA(out_buf, void *), (void *) pdest, len);
	pdest += len;
	z.avail_out = OUT_BUF_SZ;
	z.next_out = out_buf;

	outSectors += (len / EROS_SECTOR_SIZE);
      }

      inflateEnd(&z);

      if (err != Z_STREAM_END) {
	printf("Compressed floppy image is corrupt (zlib err %d)\n", err);
	halt();
      }
    }

    printf("done \001\n"); /* IBM character set happy face*/

    outSectors += DISK_BOOTSTRAP_SECTORS;
    
    if (outSectors != VolSectors) {
      printf("!! VolSectors %ld, decompressed sectors %ld -- press any key\n",
	     VolSectors, outSectors);
      halt();
    }
#else
    printf("Compressed volume load not yet supported\n");
#endif
  }
  else {
    printf(">> Loading %d sectors into ramdisk at 0x%x... ",
	   (int) VolSectors, pdest);

    read_sectors(bi->bootDrive, bi->bootStartSec, VolSectors,
		 (void *) pdest);

    printf("done \001\n"); /* IBM character set happy face*/
  }
  
  bi->isRamImage = true;
}

