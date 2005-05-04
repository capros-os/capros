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


#include <ctype.h>
#include <getopt.h>
#include <stdio.h>
#include "eros.pci.h"

int opt_vendor = 0;
int opt_device = 0;
int opt_show_all = 0;

void ck_vendor(unsigned short vendor, char *name);
void ck_device(unsigned short dev, char *name);

int
main(int argc, char *argv[])
{
  int i = 0;
  int c;
  int err = 0;
  extern char *optarg;
  extern int optind;


  while ((c = getopt(argc, argv, "vda")) != -1) {
    switch (c) {
    case 'v':
      if (opt_device)
	err++;
      opt_vendor = 1;
      break;
    case 'd':
      if (opt_vendor)
	err++;
      opt_device = 1;
      break;
    case 'a':
      opt_show_all ++;
      break;
    default:
      err++;
      break;
    }
  }
  
  if (!opt_vendor && !opt_device) {
    fprintf(stderr, "Either -v or -d is required\n");
    exit(2);
  }
  
  if (err) {
    fprintf(stderr,
	    "Usage: cklist -v files\n"
	    "   or: cklist -d files\n");
    exit(2);
  }

  
  for (; optind < argc; optind++) {
    unsigned int idno;
    char *filename = argv[optind];
    FILE *infile = fopen(filename, "r");
    int count;
    char buf[4096];
    char namebuf[4096];
    
    int curline = 1;
    
    while (fgets(buf, 4096, infile)) {
      int len = strlen(buf);
      if (buf[len-1] == '\n')
	buf[len-1] = 0;

      if (sscanf(buf,  "%x %s", &idno, namebuf) != 2) {
	fprintf(stderr, "Error in input file \"%s\" at line %d\n",
		filename, curline);
	exit(1);
      }

      if (opt_vendor)
	ck_vendor(idno, namebuf);
      if (opt_device)
	ck_device(idno, namebuf);

      curline++;
    }

    fclose(infile);
  }

  return 0;
}

void
ck_vendor(unsigned short vendor, char *name)
{
  int ndx;

  for (ndx = 0; ndx < num_pci_vendors; ndx++) {
    if (PciVendorTable[ndx].VenId == vendor) {
      if (opt_show_all || PciVendorTable[ndx].badEntry)
	printf("%c 0x%04x %s => \"%s\"\n",
	       PciVendorTable[ndx].badEntry ? '!' : '=',
	       vendor, name, PciVendorTable[ndx].VenFull);
      return;
    }
  }

  printf("? 0x%04x %s => \"???\"\n",
	 vendor, name);
}

void
ck_device(unsigned short dev, char *name)
{
  int ndx;

  for (ndx = 0; ndx < num_pci_devices; ndx++) {
    if (PciDeviceTable[ndx].DevId == dev) {
      if (opt_show_all || PciDeviceTable[ndx].badEntry)
	printf("%c 0x%04x 0x%04x %s => \"%s\", \"%s\"\n",
	       PciDeviceTable[ndx].badEntry ? '!' : '=',
	       PciDeviceTable[ndx].VenId, dev, name,
	       PciDeviceTable[ndx].Chip,
	       PciDeviceTable[ndx].ChipDesc);
      return;
    }
  }

  printf("? 0x???? 0x%04x %s => \"???\"\n",
	 dev, name);
}
