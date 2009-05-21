/*
 * Copyright (C) 2009, Strawberry Development Group.
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

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <getopt.h>
#include <time.h>
//#include <unistd.h>
#include <idl/capros/Logfile.h>

int
main(int argc, char *argv[])
{
  int c;
  extern int optind;
#if 0
  extern char *optarg;
#endif
  int opterr = 0;
  enum {dataUnspecified, dataHalf, dataLong} dataLength = dataUnspecified;
  
  while ((c = getopt(argc, argv, "hl")) != -1) {
    switch(c) {
    default:
      opterr++;
      break;
    case 'h':
      dataLength = dataHalf;
      break;
    case 'l':
      dataLength = dataLong;
      break;
    }
  }
  
  argc -= optind;
  argv += optind;
  
  if (argc != 1)
    opterr++;
  
  if (opterr) {
    fprintf(stderr, "Usage: lslog file\n");
    exit(1);
  }
  
  const char * targname = *argv;

  FILE * fd = fopen(targname, "r");
  if (!fd) {
    fprintf(stderr, "Error %d opening file \"%s\".\n", errno, targname);
    exit(1);
  }

  capros_Logfile_LogRecord16 rec16;

  bool eof = false;
  while (! eof) {
    size_t cnt = fread(&rec16.header, sizeof(rec16.header), 1, fd);
    if (cnt == 0) {
      // most likely, end of file
      eof = true;
      break;
    }

    switch (rec16.header.length) {
    default:
      fprintf(stderr, "Record length %d is unsupported.\n",
              rec16.header.length);
      exit(1);

    case sizeof(rec16):	// same as sizeof(rec32)
      cnt = fread(&rec16.value, sizeof(rec16) - sizeof(rec16.header), 1, fd);
      if (cnt == 0) {
        fprintf(stderr, "EOF or error in the middle of a record\n");
        eof = true;
        break;
      }
      if (rec16.trailer != rec16.header.length) {
        fprintf(stderr, "Header length %d does not match trailer length %d\n",
                rec16.header.length, rec16.trailer);
        eof = true;
        break;
      }
      // Print the record
      time_t tim = rec16.header.rtc;
      struct tm * rt = gmtime(&tim);
      printf("%.4d/%.2d/%.2d %.2d:%.2d:%.2d",
             rt->tm_year+1900, rt->tm_mon+1, rt->tm_mday,
             rt->tm_hour, rt->tm_min, rt->tm_sec);
      switch (dataLength) {
      default:		// dataUnspecified
      case dataHalf:
        printf("\t%d\t%d", rec16.value, rec16.param);
        break;

      case dataLong:
        printf("\t%d", ((capros_Logfile_LogRecord32 *)&rec16)->value);
        break;
      }
      printf("\n");
      break;
    }
  }

  fclose(fd);
  return 0;
}
