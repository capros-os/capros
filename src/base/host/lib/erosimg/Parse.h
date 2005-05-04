#ifndef __PARSE_HXX__
#define __PARSE_HXX__

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


#include <disk/LowVolume.h>
#include <disk/ErosTypes.h>
#include <erosimg/Intern.h>
#include <erosimg/ArchDescrip.h>

/* Parsing assistance */

#ifdef __cplusplus
extern "C" {
#endif

  INLINE 
  bool parse_MatchStart(const char **s, const char* txt)
  { *s = txt; return 1; }

  bool parse_MatchKeyword(const char **s, const char *kwd);
  bool parse_Match(const char **s, const char *str);
  bool parse_MatchIdent(const char **s, const char **ident);
  bool parse_MatchFileName(const char **s, const char **fileName);
  bool parse_MatchWord(const char **s, uint32_t *w);
  bool parse_MatchKeyData(const char **s, uint32_t *kd);
  bool parse_MatchSlot(const char **s, uint32_t *slot);
  /* Also used for MatchLIDVal */
  bool parse_MatchOIDVal(const char **s, OID *oid);
  bool parse_MatchLID(const char **s, OID *oid);
  bool parse_MatchOID(const char **s, OID *oid);
  bool parse_MatchNumKeyVal(const char **s, uint32_t *hi,
			    uint32_t *mid,uint32_t *lo);
  bool parse_MatchEOL(const char **s);
  bool parse_MatchKey(const char **s, KeyBits *key);

  bool parse_MatchArchitecture(const char **s, const char **arch);
  bool parse_MatchRegister(const char **s, const char *arch,
			   RegDescrip**);
  bool parse_MatchRegValue(const char **us, RegDescrip*,
			   const char **value);

  void parse_TrimLine(char *s);

#ifdef __cplusplus
}
#endif

#endif /* __PARSE_HXX__ */
