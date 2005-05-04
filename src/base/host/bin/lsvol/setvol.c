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
#include <erosimg/ExecImage.h>
#include <erosimg/Volume.h>
#include <erosimg/DiskDescrip.h>

const char* targname;

int wantRamDisk = 0;
int wantCompress = 0;
int wantBoot = 0;
int wantDebug = 0;
const char *kernel_name = 0;
const char *boot_name = 0;
#if 0
int showbad = 0;
#endif

int
main(int argc, char *argv[])
{
  Volume *pVol;

  int c;
  extern int optind;
  extern char *optarg;
  int opterr = 0;
  
  app_Init("setvol");

  while ((c = getopt(argc, argv, "dDzZrRk:b:B")) != -1) {
    switch(c) {
    case 'r':
      wantRamDisk = 1;
      break;
    case 'R':
      wantRamDisk = -1;
      break;
    case 'z':
      wantCompress = 1;
      break;
    case 'Z':
      wantCompress = -1;
      break;
    case 'k':
      kernel_name = optarg;
      break;
    case 'b':
      wantBoot = 1;
      boot_name = optarg;
      break;
    case 'B':
      wantBoot = -1;
      boot_name = 0;
      break;
    case 'd':
      wantDebug = 1;
      break;
    case 'D':
      wantDebug = -1;
      break;
    default:
      opterr++;
    }
  }
  
      /* remaining arguments describe node and/or page space divisions */
  argc -= optind;
  argv += optind;
  
  if (wantCompress == 0 && wantRamDisk == 0
      && kernel_name == 0 && wantBoot == 0
      && wantDebug == 0)
    opterr++;
  
  if (argc != 1)
    opterr++;
  
  if (opterr)
    diag_fatal(1, "Usage: setvol [-r|-R] [-z|-Z] [-d|-D] [-b image|-B] [-k kernel_name] file\n");
  
  targname = *argv;
  
  pVol = vol_Open(targname, false);
  
  if (kernel_name) {
    int i;

    ExecImage *kernelImage = xi_create();
    if ( !xi_SetImage(kernelImage, kernel_name) ) {
      diag_error(1, "Couldn't load kernel image\n");
      return 1;
    }
      
    if (xi_NumRegions(kernelImage) != 1) {
      diag_error(1, "%s: kernel image improperly linked. Use '-n'!\n",
		  kernel_name);
      return 1;
    }

    for (i = 0; i < vol_MaxDiv(pVol); i++) {
      const Division *d = vol_GetDivision(pVol, i);
      if (d->type == dt_Kernel)
	vol_WriteKernelImage(pVol, i, kernelImage);
    }

    xi_destroy(kernelImage);
  }
  
  if (wantBoot != 0)
    vol_WriteBootImage(pVol, boot_name);
  
  if (wantRamDisk > 0)
    vol_SetVolFlag(pVol, VF_RAMDISK);
  else if (wantRamDisk < 0) {
    vol_ClearVolFlag(pVol, VF_COMPRESSED);
    vol_ClearVolFlag(pVol, VF_RAMDISK);
  }
  
  if (wantDebug > 0)
    vol_SetVolFlag(pVol, VF_DEBUG);
  else if (wantDebug < 0) {
    vol_ClearVolFlag(pVol, VF_DEBUG);
  }
  
  if (wantCompress > 0) {
    const VolHdr *vh = vol_GetVolHdr(pVol);
    if ((vh->BootFlags & VF_RAMDISK) == 0)
      diag_error(1,
		  "Compressed volumes must presently be "
		  "ramdisks. Use '-r'!\n");
    vol_SetVolFlag(pVol, VF_COMPRESSED);
  }
  else if (wantCompress < 0)
    vol_ClearVolFlag(pVol, VF_COMPRESSED);

  vol_Close(pVol);
  free(pVol);

  app_Exit();
  exit(0);
}
