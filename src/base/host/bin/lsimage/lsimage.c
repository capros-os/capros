/*
 * Copyright (C) 1998, 1999, 2001, Jonathan S. Shapiro.
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

/* Given a UNIX  ELF format binary file, produce an EROS image file
 * from that binary file.
 */
#include <sys/fcntl.h>
#include <sys/stat.h>

#include <getopt.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <erosimg/App.h>
#include <erosimg/ErosImage.h>
#include <erosimg/ExecImage.h>
#include <erosimg/Parse.h>
#include <eros/KeyConst.h>

extern void PrintDiskKey(KeyBits);

void ShowImageDirectory(const ErosImage *image);
void ShowImageThreadDirectory(const ErosImage *image);


int
main(int argc, char *argv[])
{
  int c;
  extern int optind;
#if 0
  extern char *optarg;
#endif
  int opterr = 0;
  const char *source;
  bool showHeaders = false;
  bool showDir = false;
  bool showThreadDir = false;
  bool showNodes = false;
  bool showStrings = false;
  ErosImage *image;
  
  app_Init("lsimage");

  while ((c = getopt(argc, argv, "ndths")) != -1) {
    switch(c) {
    case 'n':
      showNodes = true;
      break;      
    case 'd':
      showDir = true;
      break;      
    case 't':
      showThreadDir = true;
      break;      
    case 'h':
      showHeaders = true;
      break;      
    case 's':
      showStrings = true;
      break;      
    default:
      opterr++;
    }
  }
  
  argc -= optind;
  argv += optind;
  
  if (argc != 1)
    opterr++;
  
  if (opterr)
    diag_fatal(1, "Usage: lsimage [-n|-t|-d|-h|-s] image_file\n");
  
  source = argv[0];

  if (!showHeaders && !showDir && !showNodes && !showThreadDir)
    showDir = true;

  image = ei_create();
  ei_ReadFromFile(image, source);
  
  if (showHeaders) {
    const ExecArchInfo *ai = ExecArch_GetArchInfo(image->hdr.architecture);

    diag_printf("Image Headers:\n");
    diag_printf("  Signature:          %s\n", image->hdr.signature);
    diag_printf("  Byte Sex:           %s\n",
		 ExecArch_GetByteSexName(ai->byteSex)); 
    diag_printf("  Version:            %d\n", image->hdr.version);
    diag_printf("  Platform            %s\n", ai->name);
    
    diag_printf("  Directory Entries:  %d\n", image->hdr.nDirEnt);
    diag_printf("  Directory Offset:   %d\n", image->hdr.dirOffset);
    diag_printf("  Startup Entries:    %d\n", image->hdr.nStartups);
    diag_printf("  Startups Offset:    %d\n", image->hdr.startupsOffset);
    diag_printf("  Data Pages:         %d\n", image->hdr.nPages);
    diag_printf("  Zero Data Pages:    %d\n", image->hdr.nZeroPages);
    diag_printf("  Page Offset:        %d\n", image->hdr.pageOffset);
    diag_printf("  Nodes:              %d\n", image->hdr.nNodes);
    diag_printf("  Nodes Offset:       %d\n", image->hdr.nodeOffset);
    diag_printf("  Str Tbl Size:       %d\n", image->hdr.strSize);
    diag_printf("  Str Tbl Offset:     %d\n", image->hdr.strTableOffset);
  }

  if (showDir)
    ShowImageDirectory(image);
  
  if (showThreadDir)
    ShowImageThreadDirectory(image);
  
  if (showNodes) {
    unsigned ndx;

    diag_printf("Image nodes:\n");

    for (ndx = 0 ; ndx < image->hdr.nNodes; ndx++) {
      unsigned slot;

      diag_printf("  Node 0x%x (%d)\n", ndx, ndx);
      for (slot = 0; slot < EROS_NODE_SIZE; slot++) {
	KeyBits key = ei_GetNodeSlotFromIndex(image, ndx, slot);
	diag_printf("    [%2d]  ", slot);
	PrintDiskKey(key);
	diag_printf("\n");
      }
    }
  }
  
  if (showStrings) {
    const StringPool *strPool = ei_GetStringPool(image);
    const char *buf = strpool_GetPoolBuffer(strPool);
    int sz = strpool_Size(strPool);
    int pos = 0;

    diag_printf("String table:\n");

    while (pos < sz) {
      diag_printf("  %3d  \"%s\"\n", pos, &buf[pos]);
      pos += strlen(&buf[pos]);
      pos++;
    }
  }
  
  ei_destroy(image);
  free(image);

  app_Exit();
  return (0);
}

void ShowImageDirectory(const ErosImage *image)
{
  unsigned i;

  diag_printf("Image directory:\n");

  for (i = 0; i < image->hdr.nDirEnt; i++) {
    EiDirent d = ei_GetDirEntByIndex(image, i);
    diag_printf("  [%2d]", i);
    diag_printf("  %-16s  ", ei_GetString(image, d.name));
    PrintDiskKey(d.key);
    diag_printf("\n");
  }
}

void ShowImageThreadDirectory(const ErosImage *image)
{
  unsigned i;

  diag_printf("Image threads:\n");

  for (i = 0; i < image->hdr.nStartups; i++) {
    EiDirent d = ei_GetStartupEntByIndex(image, i);
    diag_printf("  [%2d]", i);
    diag_printf("  %-16s  ", ei_GetString(image, d.name));
    PrintDiskKey(d.key);
    diag_printf("\n");
  }
}

