#include <sys/timeb.h>
#include <stdio.h>
#include <ctype.h>
#include <time.h>

#include "eros.pci.h"

void
dumpname(const char *name, int len)
{
  int curlen = 0;
  
  while (*name) {
    if (isalnum (*name))
      putchar( isalpha(*name) ? toupper(*name) : *name );
    else
      putchar('_');
    curlen ++;
    
    name ++;
  }

  while (curlen < len) {
    putchar(' ');
    curlen++;
  }
}

void
dumpclassname(const char *basename, const char *classname, int len)
{
  int curlen = 0;
  
  dumpname(basename, 0);
  putchar('_');
  
  curlen = strlen(basename) + 1;

  len = (len >= curlen) ? (len - curlen) : 0;

  dumpname(classname, len);
}

const char *
LookupClassBase(unsigned short classid)
{
  int i;
  unsigned short baseid = (classid >> 8) & 0xff;
  
  for (i = 0; i < num_pci_base_classes; i++) {
    if (PciBaseClassTable[i].BaseClassId == baseid)
      return PciBaseClassTable[i].BaseClassName;
  }

  fprintf(stderr,
	  "Inconsistency in database: class 0x%04x has no base class\n",
	  classid);
  exit(2);
}

main()
{
  int i;
  int maxlen;
  struct timeb tmb;
  struct tm *ptm;

  ftime(&tmb);
  ptm = localtime(&tmb.time);

  printf(
"/*
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
\n",
ptm->tm_year );


  printf("\n\n/* PCI class base values */\n\n");
  
  maxlen = 0;
  for (i = 0; i < num_pci_base_classes; i++) {
    const char *name = PciBaseClassTable[i].BaseClassName;
    int len = strlen(name);

    if (PciBaseClassTable[i].badEntry)
      continue;

    if (len > maxlen)
      maxlen = len;
  }
  
  for (i = 0; i < num_pci_base_classes; i++) {
    const char *name = PciBaseClassTable[i].BaseClassName;

    if (PciBaseClassTable[i].badEntry)
      continue;

    printf("#define PCI_BASE_CLASS_");

    dumpname(name, maxlen);

    printf(" 0x%02x\n", PciBaseClassTable[i].BaseClassId);
  }

  printf("\n\n/* PCI class values */\n\n");
  
  maxlen = 0;
  for (i = 0; i < num_pci_classes; i++) {
    const char *basename = LookupClassBase(PciClassTable[i].ClassId);
    const char *name = PciClassTable[i].ClassName;
    int len = strlen(name) + strlen(basename) + 1;

    if (PciClassTable[i].badEntry)
      continue;

    if (len > maxlen)
      maxlen = len;
  }
  
  for (i = 0; i < num_pci_classes; i++) {
    const char *basename = LookupClassBase(PciClassTable[i].ClassId);
    const char *name = PciClassTable[i].ClassName;

    if (PciClassTable[i].badEntry)
      continue;

    printf("#define PCI_CLASS_");

    dumpclassname(basename, name, maxlen);

    printf(" 0x%04x\n", PciClassTable[i].ClassId);
  }

  printf("\n\n/* PCI vendor codes */\n\n");
  
  maxlen = 0;
  for (i = 0; i < num_pci_vendors; i++) {
    const char *name = PciVendorTable[i].VenShort;
    int len = strlen(name);

    if (PciVendorTable[i].badEntry)
      continue;

    if (len > maxlen)
      maxlen = len;
  }
  
  for (i = 0; i < num_pci_vendors; i++) {
    const char *name = PciVendorTable[i].VenShort;
    if (PciVendorTable[i].badEntry)
      continue;

    printf("#define PCI_VENDOR_");

    dumpname(name, maxlen);

    printf(" 0x%04x\n", PciVendorTable[i].VenId);
  }

  printf("\n\n/* PCI device codes */\n\n");
  
  maxlen = 0;
  for (i = 0; i < num_pci_devices; i++) {
    const char *name = PciDeviceTable[i].Chip;
    int len = strlen(name);
    if (PciDeviceTable[i].badEntry)
      continue;

    if (len > maxlen)
      maxlen = len;
  }
  
  for (i = 0; i < num_pci_devices; i++) {
    const char *name = PciDeviceTable[i].Chip;
    if (PciDeviceTable[i].badEntry)
      continue;

    printf("#define PCI_DEVICE_");

    dumpname(name, maxlen);

    printf(" 0x%04x\n", PciDeviceTable[i].DevId);
  }

  exit(0);
}
