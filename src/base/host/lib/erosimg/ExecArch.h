#ifndef __EXECARCH_H__
#define __EXECARCH_H__
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

/* ALWAYS ADD TO THE END OF THESE ENUMS TO ENSURE COMPATIBILITY */
enum ByteSex {
  bs_unknown,
  bs_big,			/* big endian */
  bs_little,			/* little endian */
  bs_permuted,			/* PDP-11 permuted - you never know */
				/* when it will reappear */
  bs_we32k,			/* WE32000 family (data little, code  */
				/* big) -- this was REALLY dumb. */
  bs_NUM_BS
};

enum ExecArchitecture {
  ExecArch_unknown,
  ExecArch_neutral,
  ExecArch_i486,
  ExecArch_NUM_ARCH
};

typedef struct ExecArchInfo ExecArchInfo;
struct ExecArchInfo {
  uint32_t byteSex;
  const char *name;
};

#ifdef __cplusplus
extern "C" {
#endif

uint32_t ExecArch_FromString(const char *);
const char* ExecArch_GetByteSexName(uint32_t);
const ExecArchInfo* ExecArch_GetArchInfo(uint32_t);

#ifdef __cplusplus
}
#endif

#endif /* __EXECARCH_H__ */
