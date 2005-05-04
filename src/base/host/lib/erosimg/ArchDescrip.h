#ifndef __REGDESCRIP_H__
#define __REGDESCRIP_H__

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


#include <eros/target.h>
#include <disk/KeyStruct.h>

/* The RegDescrip structures describes the registers associated with a
 * particular architecture, the default values for these registers,
 * and the layout of the registers in the domain root and general
 * registers nodes.
 */


/* The 'RegDescrip' structure describes the architected registers and
 * their default values (if any).  For each register, it gives the
 * name, the length in bytes of the register, and the default value.
 * If a default value is provided the register will be initialized to
 * that value in the domain by mkimage.  If no default makes sense, or
 * if the register should not be initialized (e.g. overlapping
 * registers on x86), use '0' for the default value string.
 * 
 * RegDescrip also provides the externalized interface for reading and
 * writing values.
 */

struct ErosImage;
struct KeyBits;

enum { RD_MaxRegSize = 16 };	/* maximum size in bytes of any */
				/* register -- used in parsing */

typedef struct RegDescrip RegDescrip;
struct RegDescrip {
  const char *arch;		/* architecture name */
  const char *name;		/* register name */
  bool big_endian;
  uint32_t len;			/* register size in bytes */
  const char *dfltValue;	/* default value, if any */
};

#ifdef __cplusplus
extern "C" {
#endif


void RD_WriteValue(const RegDescrip *, struct ErosImage *ei, 
		   KeyBits rootNodeKey,
		   const char *value);
RegDescrip *RD_Lookup(const char *arch, const char *regName);
void RD_InitProcess(struct ErosImage* ei, KeyBits rootNodeKey, 
		    const char *arch);
bool RD_IsArchName(const char *arch);


#ifdef __cplusplus
}
#endif

#endif /* __REGDESCRIP_H__ */
