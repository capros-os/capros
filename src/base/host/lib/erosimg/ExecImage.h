#ifndef __EXECIMAGE_H__
#define __EXECIMAGE_H__
/*
 * Copyright (C) 1998, 1999, 2002, Jonathan S. Shapiro.
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

/* Helper class to encapsulate the different binary formats we need to
 * be prepared to deal with.  This way the rest of the eros library
 * doesn't need to know the formatting details.
 */

#include <eros/target.h>
#include <erosimg/ExecArch.h>
#include <erosimg/Intern.h>

/* If the ExecRegion structure looks suspiciously similar to the
 * equivalent structure in the ELF binary format, that's because it
 * is. Since I helped design ELF, this is no surprise.  The heart of
 * the problem here is that the amount of flexibility inherent in ELF
 * is considerable, so no lesser way of capturing the binary image
 * will suffice in ExecImage.  The good news is that we need only pay
 * attention to the program headers - the section headers are for our
 * purposes irrelevant.
 * 
 */

/* ExecRegion types. Same as ELF, which is not by accident! */
enum {
  ER_X = 0x1,
  ER_W = 0x2,
  ER_R = 0x4,
};

typedef struct ExecRegion ExecRegion;
struct ExecRegion {
  uint32_t vaddr;
  uint32_t memsz;
  uint32_t filesz;
  uint32_t offset;
  uint32_t perm;
}; 

typedef struct ExecImage ExecImage;
struct ExecImage {
  const char *imageTypeName;
  const char *name;

  uint8_t* image;
  uint32_t imgsz;

  ExecRegion* regions;
  uint32_t nRegions;
  
  uint32_t  entryPoint;
};

#ifdef __cplusplus
extern "C" {
#endif
  ExecImage *xi_create();
  void xi_destroy(ExecImage *);

  /* Following two methods are library-internal, but are declared for 
     the sake of separate compilation. */
bool xi_InitElf(ExecImage *pImage);
bool xi_InitAout(ExecImage *pImage);

bool xi_SetImage(ExecImage *pImage, const char *imageName);
bool xi_GetSymbolValue(ExecImage *pImage, const char *symName, uint32_t *);

INLINE const char * 
xi_GetName(const ExecImage *pImage)
{
  return pImage->name; 
}

INLINE const char* 
xi_ImageTypeName(const ExecImage *pImage)
{
  return pImage->imageTypeName; 
}

INLINE uint32_t 
xi_EntryPt(const ExecImage *pImage)
{
  return pImage->entryPoint;
}

INLINE uint32_t 
xi_NumRegions(const ExecImage *pImage)
{
  return pImage->nRegions;
}

INLINE const ExecRegion*
xi_GetRegion(const ExecImage *pImage, uint32_t w)
{
  return &pImage->regions[w];
}

INLINE const uint8_t * 
xi_GetImage(const ExecImage *pImage)
{
  return pImage->image;
}

INLINE uint32_t 
xi_GetImageSz(const ExecImage *pImage)
{
  return pImage->imgsz;
}

#ifdef __cplusplus
}
#endif

#endif /* __EXECIMAGE_H__ */
