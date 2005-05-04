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

#include <sys/fcntl.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <erosimg/App.h>
#include <erosimg/Intern.h>
#include <erosimg/ExecImage.h>

static void
xi_ResetImage(ExecImage *pImage)
{
  pImage->name = "pImage->unknown";
  pImage->entryPoint = 0;
  pImage->imageTypeName = "unknown";
  pImage->nRegions = 0;

  if (pImage->image)
    free (pImage->image);
  pImage->image = 0;

  if (pImage->regions)
    free(pImage->regions);
  pImage->regions = 0;
}

ExecImage *
xi_create()
{
  ExecImage *pImage = (ExecImage *) malloc(sizeof(ExecImage));
  pImage->image = 0;
  pImage->regions = 0;
  xi_ResetImage(pImage);
  return pImage;
}

void xi_destroy(ExecImage *pImage)
{
  free(pImage->image);
  if (pImage->regions)
    free(pImage->regions);
}

bool
xi_SetImage(ExecImage *pImage, const char *imageName)
{
  const char *fileName;
  int imagefd;
  struct stat statbuf;
  bool win = false;

  pImage->name = imageName;
  
  fileName = app_BuildPath(pImage->name);
  
  imagefd = open(fileName, O_RDONLY);
  if (imagefd < 0) {
    diag_error(1, "Unable to open image file \"%s\"\n", pImage->name);
    return false;
  }

  if (fstat(imagefd, &statbuf) < 0) {
    diag_error(1, "Can't stat image file \"%s\"\n", pImage->name);
    close(imagefd);
    return false;
  }

  pImage->imgsz = statbuf.st_size;
  pImage->image = (uint8_t *) malloc(sizeof(uint8_t) * pImage->imgsz);
  
  if (read(imagefd, pImage->image, statbuf.st_size) != statbuf.st_size) {
    diag_error(1, "Can't read image file \"%s\"\n", pImage->name);
    close(imagefd);
    return false;
  }

  if (win == false)
    win=xi_InitElf(pImage);

#ifdef SUPPORT_AOUT
  if (win == false)
    win=xi_InitAout(pImage);
#endif
  
  /* Tries ELF first, then a.out format: */
  if (!win) {
    diag_fatal(1, "Couldn't interpret image\n");
    close(imagefd);
    return false;
  }

  close(imagefd);
  
  return true;
}

#if 0
uint32_t
ExecImage::GetSize(uint32_t unitSize) const
{
  uint32_t imageSize = txtSize + rodataSize + dataSize + bssSize;
  uint32_t units = (imageSize + (unitSize - 1)) / unitSize;
  return units;
}

uint32_t
ExecImage::GetRoSize(uint32_t unitSize) const
{
  uint32_t units = txtSize / unitSize;

  return units;
}

uint32_t
ExecImage::GetContentSize(uint32_t unitSize) const
{
  uint32_t imageSize = txtSize + rodataSize + dataSize + bssSize;
  uint32_t units = ((txtSize + dataSize) + (unitSize - 1)) / unitSize;

  diag_printf("Image \"%s\" txt %d data %d bss %d sz %d units %d * %d\n",
	name.str(), txtSize, dataSize, bssSize, imageSize, units, unitSize);

  return units;
}

char *
ExecImage::GetBuffer(uint32_t unitSize) const
{
  uint32_t imageSize = txtSize + rodataSize + dataSize + bssSize;
  uint32_t units = (imageSize + (unitSize-1)) / unitSize;

  /* allocate image buffer - round up to sector multiple */
  char * imageBuf = new char[units * unitSize];
  bzero(imageBuf, units * unitSize);

  char *buf = imageBuf;

  ::lseek(imagefd, txtOffset, SEEK_SET);
  if (::read(imagefd, buf, txtSize) != txtSize)
    diag_fatal(1, "Couldn't read text image\n");
  buf += txtSize;

  ::lseek(imagefd, rodataOffset, SEEK_SET);
  if (::read(imagefd, buf, rodataSize) != rodataSize)
    diag_fatal(1, "Couldn't read rodata image\n");
  buf += rodataSize;

  ::lseek(imagefd, dataOffset, SEEK_SET);
  if (::read(imagefd, buf, dataSize) != dataSize)
    diag_fatal(1, "Couldn't read data image\n");
  buf += dataSize;

  return imageBuf;
}
#endif

