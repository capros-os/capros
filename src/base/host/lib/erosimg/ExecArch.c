/*
 * Copyright (C) 1998, 1999, Jonathan S. Shapiro.
 * Copyright (C) 2006, Strawberry Development Group.
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

#include <string.h>
#include <eros/target.h>
#include <erosimg/ExecArch.h>
#include <erosimg/Diag.h>

uint32_t
ExecArch_FromString(const char * name)
{
  if (strcmp(name, "i486") == 0) {
    return ExecArch_i486;
  }
  else if (strcmp(name, "arm") == 0) {
    return ExecArch_arm;
  }
  else {
    diag_fatal(1, "Unknown architecture \"%s\"\n", name);
  }

  return ExecArch_unknown;
}

static const char * const byteSexNames[] = {
  "unknown"
  "big endian",			/* big endian */
  "little endian",		/* little endian */
  "permuted",			/* PDP-11, but you never know when it */
				/* will reappear */
  "we32k",			/* western electric 32000 family (data  */
				/* little, code big) */
};

static const ExecArchInfo archInfo[ExecArch_NUM_ARCH] = {
  { bs_unknown,  "unknown" },
  { bs_unknown,  "neutral" },
  { bs_unknown,  "i486" }
} ;

const char*
ExecArch_GetByteSexName(uint32_t bs)
{
  if (bs >= bs_NUM_BS)
    return byteSexNames[bs_unknown];
  
  return byteSexNames[bs];  
}

const ExecArchInfo*
ExecArch_GetArchInfo(uint32_t arch)
{
  if (arch >= ExecArch_NUM_ARCH)
    return &archInfo[ExecArch_unknown];
  
  return &archInfo[arch];
}
