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
#include <erosimg/App.h>
#include <erosimg/Volume.h>
#include <erosimg/DiskDescrip.h>

const char* targname;

int showdiv = 0;
int showhdr = 0;
int showckdir = 0;
int showrsdir = 0;
#if 0
int showbad = 0;
#endif

int main(int argc, char *argv[])
{
  Volume *pVol;

  int c;
  extern int optind;
#if 0
  extern char *optarg;
#endif
  int opterr = 0;
   
 app_Init("lsvol");

  while ((c = getopt(argc, argv, "hdrc")) != -1) {
    switch(c) {
#if 0
    case 'b':
      showbad = 1;
      break;
#endif
    case 'd':
      showdiv = 1;
      break;
    case 'c':
      showckdir = 1;
      break;
    case 'r':
      showrsdir = 1;
      break;
    case 'h':
      showhdr = 1;
      break;
    default:
      opterr++;
    }
  }
  
  if (!showdiv && !showhdr && !showckdir /* && !showbad */)
    showdiv = 1;
  
      /* remaining arguments describe node and/or page space divisions */
  argc -= optind;
  argv += optind;
  
  if (argc != 1)
    opterr++;
  
  if (opterr)
    diag_fatal(1, "Usage: lsvol [-h | -d | -r | -c ] file\n");
  
  targname = *argv;
  
  pVol = vol_Open(targname, false, 0, 0, 0);
  
  if (showhdr) {
    extern void PrintVolHdr(Volume*);
    PrintVolHdr(pVol);
  }
  
  if (showdiv) {
    extern void PrintDivTable(Volume*);
    PrintDivTable(pVol);
  }

  if (showckdir) {
    extern void PrintCkptDir(Volume*);
    PrintCkptDir(pVol);
  }
  
  if (showrsdir) {
    extern void PrintRsrvDir(Volume*);
    PrintRsrvDir(pVol);
  }
  
#if 0
  if (showbad) {
    int i;
    
    if (vol.MaxBadEnt())
      diag_printf("From           To\n");
    else
      diag_printf("Badmap Empty\n");

    for (i = 0; i < pVol->MaxBadEnt(); i++) {
      const BadEnt& be = pVol->GetBadEnt(i);
      diag_printf("%-8d  ==>  %-8d\n", be.badSec, be.goodSec);
    }
  }
#endif

  vol_Close(pVol);
  free(pVol);

  app_Exit();
  exit(0);
}
