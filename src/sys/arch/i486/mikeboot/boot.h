/*
 * Copyright (C) 2001, Jonathan S. Shapiro.
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

struct BiosGeometry {
  unsigned char  drive;                 /* as reported by BIOS. */
  unsigned char  secs;
  unsigned short cyls;
  unsigned char  heads;
  unsigned long  spcyl;
} ;
typedef struct BiosGeometry BiosGeometry;
typedef struct Vbe3Block Vbe3Block;

#ifdef __cplusplus
extern "C" {
#endif
  extern BootInfo *ProbeMemory(void);
  extern void ShowMemory(BootInfo *);
  extern void ShowDivisions(BootInfo *bi);

  void Interact(BootInfo *);
  void LoadRamDisk(BootInfo *);
  void LoadKernel(BootInfo *);
  void StartKernel(uint32_t pa, BootInfo *) __attribute__ ((noreturn));
  int printf(const char*, ...);
  void halt();
  void PreloadDivisions(BootInfo *);

  unsigned old_memsize(unsigned loOrHi);
  void gateA20(void);
  unsigned GetDisplayMode();
  unsigned SetDisplayMode(uint8_t mode);

  extern void CaptureBootDisk(BootInfo *bi);
  extern unsigned bios_diskinfo(unsigned drive);
  int biosread(unsigned drive, unsigned cyl, unsigned head, unsigned
	       sec, unsigned nsec, unsigned short bufseg, void *buf);
  extern void get_diskinfo(unsigned char drive, BiosGeometry *bg);
  extern void read_sectors(unsigned char drive, unsigned long startSec,
			   unsigned long nsec, void *buf);
  extern void finish_load();
  extern int getchar(int);
  extern void halt(void);
  extern void waitkbd(void);
  extern void twiddle();

  unsigned long crc32(unsigned long crc, const unsigned char *buf,
		      unsigned int len);
  extern int printf(const char*, ...);
  extern void ppcpy(void *from, void *to, size_t len);

  extern void *memcpy(void *dest, const void *src, size_t n);
  extern void *memset(void *dest, int c, size_t n);

  extern void *malloc(size_t);

  extern void *BootAlloc(size_t sz, unsigned alignment);
  extern void BindRegion(BootInfo *, kpa_t base, kpsize_t size,
			 unsigned type);

  extern void end();

  extern uint8_t InitVESA();
  extern uint32_t ChooseVESA();

  void SetConsoleMode(BootInfo *bi, uint32_t mode);

  extern uint32_t RamDiskAddress;

#ifdef __cplusplus
}
#endif

extern uint32_t RamDiskAddress;
