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

#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/fcntl.h>
#include <unistd.h>
#include <getopt.h>
#include <stdlib.h>

#ifndef __FreeBSD__
#include <linux/fd.h>
#include <linux/hdreg.h>
#include <syscall.h>
#endif

#include <erosimg/App.h>
#include <erosimg/Volume.h>
#include <disk/LowVolume.h>
#include <erosimg/DiskDescrip.h>

/* #define DEBUG */

#ifdef __linux__
#ifndef __FreeBSD__

/* taken from linux-2.2.26/include/linux/kdev_t.h: */

#define MINORBITS       8
#define MAJOR(dev)      ((unsigned int) ((dev) >> MINORBITS))

/* taken from linux-2.2.26/include/linux/major.h: */

#define FLOPPY_MAJOR    2
#define IDE0_MAJOR      3
#define SCSI_DISK0_MAJOR 8
#define XT_DISK_MAJOR   13
#define IDE1_MAJOR      22
#define IDE2_MAJOR      33
#define IDE3_MAJOR      34
#define SCSI_DISK1_MAJOR        65
#define SCSI_DISK7_MAJOR        71

#define SCSI_DISK_MAJOR(M) ((M) == SCSI_DISK0_MAJOR || \
  ((M) >= SCSI_DISK1_MAJOR && (M) <= SCSI_DISK7_MAJOR))

/* end of code from linux headers */ 

#define IS_FLOPPY(st) (MAJOR(st.st_rdev) == FLOPPY_MAJOR)

#define IS_SCSI_DISK(m) (SCSI_DISK_MAJOR(m))

#define IS_HD(st) (MAJOR(st.st_rdev) == IDE0_MAJOR \
		    || IS_SCSI_DISK(MAJOR(st.st_rdev)) \
		    || MAJOR(st.st_rdev) == XT_DISK_MAJOR \
		    || MAJOR(st.st_rdev) == IDE1_MAJOR \
		    || MAJOR(st.st_rdev) == IDE2_MAJOR \
		    || MAJOR(st.st_rdev) == IDE3_MAJOR)
#endif
#endif

const char *target;

typedef struct Geom Geom;
struct Geom {
  uint32_t hd;
  uint32_t sec;
  uint32_t spcyl;
  uint32_t cyl;
  uint32_t start;
} ;

#ifdef __FreeBSD__
#include <sys/types.h>
#include <sys/disklabel.h>

void
GetGeometry(int fd, Geom *geom)
{
  /* Following works for floppies, but something isn't right
   * about it for hard disks. Not clear what yet. */

  struct stat stat;

  if (0 == fstat(fd, &stat)) {
    if (S_ISBLK( stat.st_mode)) {
      struct disklabel disklabel;

      if (-1 != ioctl(fd, DIOCFGINFO, &disklabel)) {
	int partition;

	partition = DISKPART(stat.st_rdev);

	if (partition < disklabel.d_npartitions)
	  geom->start = disklabel.d_partitions[partition].p_offset;
	else
	  Diag::fatal(3, "setboot: Unable to obtain disk start\n");
	
	geom->hd = disklabel.d_ntracks;
	geom->cyl = disklabel.d_ncylinders;
	geom->sec = disklabel.d_nsectors;
	geom->spcyl = geom->sec * geom->hd;
      }
      else {
	Diag::fatal(3, "setboot: Unable to obtain disk geometry\n");
      }
    }
    else {
      Diag::fatal(1, "setboot: \"%s\" is not a block device\n", target);
    }
  }
  else {
      Diag::fatal(1, "setboot: Unable to stat \"%s\"\n", target);
  }

  return;
}
#else
void
GetGeometry(int fd, Geom *geom)
{
  struct floppy_struct fdprm;
  struct hd_geometry hdprm;

  struct stat st;

  fstat(fd, &st);

  if (S_ISBLK(st.st_mode) == 0)
    diag_fatal(1, "setboot: \"%s\" is not a block device\n", target);
  
  diag_printf("Major number for \"%s\" is %d\n", target, MAJOR(st.st_rdev));

  if (IS_FLOPPY(st)) {
    if (ioctl(fd, FDGETPRM, &fdprm) < 0)
      diag_fatal(3, "setboot: Unable to obtain floppy geometry\n");

    geom->hd = fdprm.head;
    geom->cyl = fdprm.track;
    geom->sec = fdprm.sect;
    geom->start = 0;
  }
  else if (IS_HD(st)) {
    if (ioctl(fd,HDIO_GETGEO,&hdprm) < 0)
      diag_fatal(3, "setboot: Unable to obtain hard disk geometry\n");

    geom->hd = hdprm.heads;
    geom->cyl = hdprm.cylinders;
    geom->sec = hdprm.sectors;
    geom->start = hdprm.start;
  }
  else {
    diag_fatal(3, "setboot: \"%s\" has unknown device type\n", target);
  }

  geom->spcyl = geom->sec * geom->hd;
}
#endif

typedef struct BlockTable BlockTable;
struct BlockTable {
  uint16_t cylsec;
  uint8_t  head;
  uint8_t  nsec;
};

void
UpdateBootTable(int fd, Geom *geom, int dowrite)
{
  Geom p;
  uint32_t start = geom->start;
  char bootstrap[512];
  BlockTable *btbl;
  uint32_t resid;
  uint16_t count;

  p.start = start;
  p.spcyl = geom->spcyl;
  p.sec = geom->sec;
  p.cyl = start / p.spcyl;
  start %= p.spcyl;
  p.hd = start / p.sec;
  start %= p.sec;
  p.sec = start + 1;

  diag_printf("Geometry c/h/s:      %d/%d/%d\n", geom->cyl, geom->hd,
	       geom->sec);
  diag_printf("Partition starts at: %d/%d/%d  (sector %d)\n", p.cyl,
	       p.hd, p.sec, geom->start);


  lseek(fd, SEEK_SET, 0);
  read(fd, bootstrap, 512);

  btbl = (BlockTable *) &bootstrap[sizeof(VolHdr)+sizeof(uint16_t)];

  resid = DISK_BOOTSTRAP_SECTORS;
  count = 0;
  
#define MIN(x, y) ((x < y) ? x : y)
#define CYLSEC(cyl, sec) ((sec & 0x3fu) | ((cyl << 8)&0xff00u) \
			  | ((cyl >> 2) & 0xC0u))

  for ( ; resid; btbl++, count++) {
    btbl->nsec = MIN(resid, geom->sec);
    if (p.sec > 1)		/* first one might be short */
      btbl->nsec -= (p.sec - 1);
    btbl->cylsec = CYLSEC(p.cyl, p.sec);
    btbl->head = p.hd;

    diag_printf("c/h/s %d/%d/%d cs=0x%04x nsec=%d\n", p.cyl, p.hd,
		 p.sec, btbl->cylsec,
		 btbl->nsec);
    
    p.sec = 1;
    p.hd++;
    if (p.hd == geom->hd) {
      p.cyl++;
      p.hd = 0;
    }

    resid -= btbl->nsec;
  }
  
  * ((uint16_t *) &bootstrap[sizeof(VolHdr)]) = count;

  if (dowrite) {
    lseek(fd, SEEK_SET, 0);
    write(fd, bootstrap, 512);
  }
}

void
ReadLoadList(int fd)
{
  BlockTable *btbl;
  char bootstrap[512];
  uint16_t count;
  uint16_t i;

  lseek(fd, SEEK_SET, 0);
  read(fd, bootstrap, 512);

  btbl = (BlockTable *) &bootstrap[sizeof(VolHdr)+sizeof(uint16_t)];
  count = * ((uint16_t *) &bootstrap[sizeof(VolHdr)]);

#define SEC(x) (x & 0x3fu)
#define CYL(x) ( ((x << 2) & 0x300u) | ((x >> 8) & 0xffu) )
  
  for (i = 0; i < count; i++) {
    diag_printf("c/h/s %d/%d/%d cs=0x%04x nsec=%d\n",
		 CYL(btbl->cylsec), btbl->head,
		 SEC(btbl->cylsec), btbl->cylsec,
		 btbl->nsec);
    btbl++;
  }
}

int
main(int argc, char *argv[])
{
  int c;
  extern int optind;
  int opterr = 0;

  int larg = 0;
  int warg = 0;
  int garg = 0;
  
  int fd;

  app_Init("setboot");

  while ((c = getopt(argc, argv, "lgw")) != -1) {
    switch(c) {
    case 'l':
      larg = 1;
      break;
    case 'w':
      warg = 1;
      break;
    case 'g':
      garg = 1;
      break;
    default:
      opterr++;
      break;
    }
  }

  argc -= optind;
  argv += optind;
  
  if (argc != 1)
    opterr++;
  
  if (opterr)
    diag_fatal(1, "Usage: setboot disk_image\n");

  target = argv[0];

  fd = open(target, O_RDWR);
  if (fd == -1)
    diag_fatal(1, "setboot: can't open \"%s\"\n", argv[0]);

  if (larg) {
    ReadLoadList(fd);
  }
  else if (garg) {
    Geom geometry;
  
    GetGeometry(fd, &geometry);
    diag_printf("Geometry c/h/s:      %d/%d/%d\n",
		 geometry.cyl, geometry.hd,
		 geometry.sec);
  }
  else {
    Geom geometry;
  
    GetGeometry(fd, &geometry);

    UpdateBootTable(fd, &geometry, warg);
  }

  close(fd);

  app_Exit();
  exit(0);
}
