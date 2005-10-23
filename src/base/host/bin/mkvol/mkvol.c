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

#include <getopt.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>

#include <disk/PagePot.h>

#include <erosimg/App.h>
#include <erosimg/Parse.h>
#include <erosimg/ExecImage.h>
#include <erosimg/Volume.h>
#include <erosimg/DiskDescrip.h>

Volume *pVol;
  
const char* targname;
const char* volmap;
const char *bootName = 0;
const char *kernelName = 0;

void 
ProcessVolMap()
{
  char buf[EROS_PAGE_SIZE];
  int line = 0;
  FILE* f = fopen(app_BuildPath(volmap), "r");
  
  if (!f)
    diag_fatal(1, "Couldn't open volume map file\n");
  
  while(fgets(buf, EROS_PAGE_SIZE, f)) {
    uint32_t sz;
    OID oid;
    const char *rest;

    line++;
    
    parse_TrimLine(buf);
    rest = buf;
    
    /* blank lines and lines containing only comments are fine: */
    if (*rest == 0)
      continue;
    
    if (parse_MatchStart(&rest, buf) &&
	parse_MatchKeyword(&rest, "kernel") &&
	parse_MatchWord(&rest, &sz) &&
	parse_MatchOID(&rest, &oid) &&
	parse_MatchEOL(&rest) ) {
      int kerndiv;

      sz *= EROS_PAGE_SECTORS;
      
      if (oid < 0xffff000000000000ull)
	diag_fatal(1, "%s: kernel range start at oid >= 0xffff000000000000\n",
		    volmap);
      
      kerndiv = vol_AddDivisionWithOid(pVol,dt_Kernel, sz, oid);

      if (kernelName) {
	ExecImage *kernelImage = xi_create();
	if ( !xi_SetImage(kernelImage, kernelName) )
	  app_ExitWithCode(1);
      
	if (xi_NumRegions(kernelImage) != 1)
	  diag_fatal(1, "%s: kernel image \"%s\" improperly linked. Use '-n'!\n",
		     volmap, kernelName);

#if 0
	if (!sz) {
	  sz = kernelImage.GetRegion(0).filesz;
	  sz = (sz + (EROS_PAGE_SIZE - 1)) / EROS_PAGE_SIZE;
	}
#endif

	vol_WriteKernelImage(pVol, kerndiv, kernelImage);
	xi_destroy(kernelImage);
      }
    }
    else if (parse_MatchStart(&rest, buf) &&
	     parse_MatchKeyword(&rest, "spare") &&
	     parse_MatchWord(&rest, &sz) &&
	     parse_MatchEOL(&rest) ) {
      if (!sz)
	diag_fatal(1, "%s, line %d: division size needed for spare division.\n",
		    volmap, line); 
	
      /* round up to a page worth! */
      if (sz % EROS_PAGE_SECTORS) {
	sz -= (sz % EROS_PAGE_SECTORS);
	sz += EROS_PAGE_SECTORS;
      }
      vol_AddDivision(pVol, dt_Spare, sz);
    }
    else if (parse_MatchStart(&rest, buf) &&
	     parse_MatchKeyword(&rest, "object") &&
	     parse_MatchKeyword(&rest, "preload") &&
	     parse_MatchWord(&rest, &sz) &&
	     parse_MatchOID(&rest, &oid) &&
	     parse_MatchEOL(&rest) ) {
      int div = vol_AddDivisionWithOid(pVol,dt_Object, sz * EROS_PAGE_SECTORS, oid);
      vol_DivisionSetFlags(pVol, div, DF_PRELOAD);
    }
    else if (parse_MatchStart(&rest, buf) &&
	     parse_MatchKeyword(&rest, "object") &&
	     parse_MatchWord(&rest, &sz) &&
	     parse_MatchOID(&rest, &oid) &&
	     parse_MatchEOL(&rest) ) {
      vol_AddDivisionWithOid(pVol, dt_Object, sz * EROS_PAGE_SECTORS, oid);
    }
    else if (parse_MatchStart(&rest, buf) &&
	     parse_MatchKeyword(&rest, "cklog") &&
	     parse_MatchWord(&rest, &sz) &&
	     parse_MatchLID(&rest, &oid) &&
	     parse_MatchEOL(&rest) ) {

      int diskPages = sz;
      vol_AddDivisionWithOid(pVol, dt_Log, diskPages * EROS_PAGE_SECTORS, oid);
    }
    else if (parse_MatchStart(&rest, buf) &&
	     parse_MatchKeyword(&rest, "divtable") &&
	     parse_MatchEOL(&rest) ) {
      int diskPages = (sz + (DISK_NODES_PER_PAGE - 1)) / DISK_NODES_PER_PAGE;
      vol_AddDivisionWithOid(pVol, dt_DivTbl, diskPages * EROS_PAGE_SECTORS, oid);
    }
    else {
      diag_fatal(1, "%s, line %d: syntax error.\n", volmap, line);
    }
  }

  fclose(f);
}

int main(int argc, char *argv[])
{
  int c;
  extern int optind;
  extern char *optarg;
  int opterr = 0;
  
  app_Init("mkvol");

  while ((c = getopt(argc, argv, "b:k:")) != -1) {
    switch(c) {
    case 'b':
      bootName = optarg;
      break;
    case 'k':
      kernelName = optarg;
      break;
      
    default:
      opterr++;
    }
  }
  
  /* remaining arguments describe node and/or page space divisions */
  argc -= optind;
  argv += optind;
  
  if (argc != 2)
    opterr++;
  
  if (opterr)
    diag_fatal(1, "Usage: mkvol -b bootimage -k kernimage volmap volume-file\n");
  
  volmap = argv[0];
  targname = argv[1];

  pVol = vol_Create(targname, bootName);
  if (!pVol)
    diag_fatal(2, "Couldn't open target file \"%s\"\n", targname);
  
  if (volmap)
    ProcessVolMap();
  
  vol_Close(pVol);
  free(pVol);
  
  app_Exit();
  exit(0);
}
