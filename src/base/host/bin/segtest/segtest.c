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

#include <sys/fcntl.h>
#include <stdio.h>

#include <getopt.h>
#include <stdlib.h>

#include <erosimg/App.h>
#include <erosimg/ErosImage.h>
#include <erosimg/Parse.h>

extern void PrintDiskKey(KeyBits);

int
main(int argc, char *argv[])
{
  int c;
  extern int optind;
#if 0
  extern char *optarg;
#endif
  int opterr = 0;
  char buf[EROS_PAGE_SIZE];

  KeyBits segKey;
  KeyBits rootKey;
  ErosImage *ei = ei_create();

  keyBits_InitToVoid(&segKey);
  
  app_Init("segtest");

  while ((c = getopt(argc, argv, "")) != -1) {
    switch(c) {
    default:
      opterr++;
    }
  }
  
  argc -= optind;
  argv += optind;
  
  if (argc != 0)
    opterr++;
  
  if (opterr)
    diag_fatal(1, "Usage: segtest\n");
  
  for(;;) {
    uint32_t w;
    const char* rest;

    printf("root key, offset: ");
    fflush(stdout);

    if (!fgets(buf, EROS_PAGE_SIZE, stdin))
      continue;
    
    parse_TrimLine(buf);
    rest = buf;
    
    /* blank lines and lines containing only comments are fine: */
    if (*rest == 0)
      continue;
    
    keyBits_InitToVoid(&rootKey);
    if (parse_MatchStart(&rest, buf) &&
	parse_MatchKey(&rest, &rootKey) &&
	parse_Match(&rest, ",") &&
	parse_MatchWord(&rest, &w) &&
	parse_MatchEOL(&rest) ) {
      KeyBits pageKey;

      printf("Add page at offset 0x%x to segment ", (unsigned) w);
      fflush(stdout);
      PrintDiskKey(rootKey);
      printf("\n");

      pageKey = ei_AddZeroDataPage(ei, false);
      segKey = ei_AddPageToSegment(ei, rootKey, w, pageKey);
      ei_PrintSegment(ei, segKey);
    }
    if (parse_MatchStart(&rest, buf) &&
	parse_Match(&rest, "$") &&
	parse_Match(&rest, ",") &&
	parse_MatchWord(&rest, &w) &&
	parse_MatchEOL(&rest) ) {
      KeyBits pageKey;

      printf("Add page at offset 0x%x to segment ", (unsigned) w);
      fflush(stdout);
      PrintDiskKey(segKey);
      printf("\n");

      pageKey = ei_AddZeroDataPage(ei, false);
      segKey = ei_AddPageToSegment(ei, segKey, w, pageKey);
      ei_PrintSegment(ei, segKey);
    }
  }

  ei_destroy(ei);
  free(ei);

  app_Exit();
}
