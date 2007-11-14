/*
 * Copyright (C) 1998, 1999, 2001, Jonathan S. Shapiro.
 * Copyright (C) 2007, Strawberry Development Group.
 *
 * This file is part of the CapROS Operating System.
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
/* This material is based upon work supported by the US Defense Advanced
Research Projects Agency under Contract No. W31P4Q-07-C-0070.
Approved for public release, distribution unlimited. */

#include <ctype.h>
#include <string.h>
#include <stdlib.h>

#include <erosimg/DiskKey.h>
#include <erosimg/App.h>
#include <erosimg/Parse.h>
#include <erosimg/ArchDescrip.h>
#include <eros/StdKeyType.h>

#define __EROS_PRIMARY_KEYDEF(name, isValid, bindTo) { #name, isValid },
/* OLD_MISCKEY(name) { #name, 0 }, */

static const struct {
  char *name;
  int isValid;
} KeyNames[KKT_NUM_KEYTYPE] = {
#include <eros/StdKeyType.h>
};

#define MISC_KEYTYPE(kt) (kt >= FIRST_MISC_KEYTYPE)

bool
GetMiscKeyType(const char *s, uint32_t *ty)
{
  uint32_t i;

  for (i = 0; i < KKT_NUM_KEYTYPE; i++) {
    if (strcasecmp(s, KeyNames[i].name) == 0) {
#if 0
      if (KeyNames[i].isValid == 0)
	return false;
#endif
      if (!MISC_KEYTYPE(i))
	return false;
      *ty = i;
      return true;
    }
  }

  return false;
}

void 
parse_TrimLine(char *s)
{
  for(; *s; s++) {
    if (*s == '#' || *s == '\n')
      *s = 0;
  }
  
  do {
    *s = 0;
    s--;
  } while(isspace(*s));
  
  s++;
}

bool
parse_MatchKeyword(const char **txt, const char* kwd)
{
  const char* s = *txt;
  
  int len = strlen(kwd);

  while(isspace(*s))
    s++;

  if (strncasecmp(s, kwd, len))
    return false;

  if (isalnum(s[len]))
    return false;

  s += len;

  *txt = s;
  
  return true;
}

bool
parse_Match(const char **txt, const char* tok)
{
  const char* s = *txt;
  
  int len = strlen(tok);

  while(isspace(*s))
    s++;

  if (strncmp(s, tok, len))
    return false;

  s += len;

  *txt = s;
  
  return true;
}

/* identifiers can be qualified with '.' in them.  We grab the whole
 * thing.
 */
bool
parse_MatchIdent(const char **txt, const char **ident)
{
  const char* s = *txt;

  while(isspace(*s))
    s++;
  
  if (isalpha(*s)) {
    const char* id = s;
    while (isalnum(*s) || *s == '.' || (*s == '_'))
      s++;
    *ident = internWithLength(id, s - id);
    *txt = s;
    return true;
  }

  return false;  
}

bool
parse_MatchArchitecture(const char **txt, const char **arch)
{
  const char* s = *txt;

  const char *nm;

  if (!parse_MatchIdent(&s, &nm))
    return false;

  /* See if it's really an architecture name: */
  if (RD_IsArchName(nm) == false)
    return false;

  *arch = nm;

  *txt = s;
  return true;
}

bool
parse_MatchRegister(const char **txt, const char *arch,
		     RegDescrip** rrd)
{
  const char* s = *txt;

  RegDescrip *rd;
  
  const char *nm;

  if (!parse_MatchIdent(&s, &nm))
    return false;

  /* See if it's really an architecture name: */
  if ( (rd = RD_Lookup(arch, nm)) ) {
    *txt = s;
    *rrd = rd;
    return true;
  }

  return false;
}

/* Basically, match a hex number no longer than the required number of
 * bytes
 */
bool
parse_MatchRegValue(const char **txt, RegDescrip* prd, 
		    const char **v)
{
  const char *s = *txt;
  char digits[RD_MaxRegSize*2 + 3];/* "0x" + \0 */
  char *nextDigit;
  uint32_t hexlen;
  uint32_t len;

  while(isspace(*s))
    s++;

  /* hopefully now looking at 0x... */
  if (strncasecmp(s, "0x", 2))
    return false;
  s += 2;
  /* now should be looking at the raw digits. */

  /* count the hex digits: */
  digits[0] = '0';
  digits[1] = 'x';

  nextDigit = &digits[2];

  hexlen = prd->len * 2;	/* max hex digits to accept */
  
  len = 0;

  while(isxdigit(*s) || *s == ' ') {
    if (len >= hexlen)
      return false;

    if (isxdigit(*s))
      *nextDigit++ = *s;
    
    s++;
  }

  *nextDigit = 0;

  *v = intern(digits);
  
  *txt = s;
  
  return true;
}

bool
parse_MatchFileName(const char **txt, const char **fileName)
{
  const char* s = *txt;
  const char* name = s;

  while(isspace(*s))
    s++;
  
  if (*s == 0) {
    *fileName = 0;
    return false;
  }
  
  /* now looking at a character that is not a space - assume
   * that it is the start of a file name.
   */

  name = s;
  
  while (*s && !isspace(*s))
    s++;

  *fileName = internWithLength(name, s - name);
  *txt = s;
  return true;  
}

bool
parse_MatchEOL(const char **txt)
{
  const char *s = *txt;

  while(isspace(*s))
    s++;
  
  if (*s)
    return false;

  *txt = s;
  return true;
}

/* parse the arguments for the 'page' or 'node' division type: */
bool
parse_MatchWord(const char **txt, uint32_t *w)
{
  const char *s = *txt;

  while(isspace(*s))
    s++;
  
  if (isdigit(*s)) {
    char *next;
    *w = strtoul(s, &next, 0);
    if (s != next) {
      s = next;
      *txt = s;
      return true;
    }
  }
  return false;
}

bool
parse_MatchKeyData(const char **txt, uint32_t *w)
{
  uint32_t w2;
  const char *s = *txt;
  parse_MatchWord(&s, &w2);

  if (w2 > EROS_KEYDATA_MAX)
    return false;

  *w = w2;
  *txt = s;
  return true;
}

bool
parse_MatchSlot(const char **txt, uint32_t *slot)
{
  const char *s = *txt;
  parse_MatchWord(&s, slot);

  if (*slot >= EROS_NODE_SIZE)
    return false;

  *txt = s;
  return true;
}

bool
parse_MatchOID(const char **txt, OID *oid)
{
  const char *s = *txt;

  while(isspace(*s))
    s++;

  if (strncasecmp(s, "oid", 3))
    return false;
  s += 3;

  while(isspace(*s))
    s++;

  if (*s != '=')
    return false;
  s++;

  if (!parse_MatchOIDVal(&s, oid))
    return false;

  *txt = s;
  return true;
}

bool
parse_MatchLID(const char **txt, OID *oid)
{
  const char *s = *txt;

  while(isspace(*s))
    s++;

  if (strncasecmp(s, "lid", 3) && strncasecmp(s, "oid", 3))
    return false;
  if (strncasecmp(s, "lid", 3))
    diag_warning("use of 'oid' in cklog line is obsolete!\n");
  s += 3;

  while(isspace(*s))
    s++;

  if (*s != '=')
    return false;
  s++;

  if (!parse_MatchOIDVal(&s, oid))
    return false;

  *txt = s;
  return true;
}

bool
parse_MatchOIDVal(const char **txt, OID *oid)
{
  const char *s = *txt;
  const char *xstr;
  int len;
  char digits[17];		/* 16 hex digits plus zero */
  uint32_t hi = 0;
  uint32_t lo = 0;

  while(isspace(*s))
    s++;

  /* hopefully now looking at 0x... */
  if (strncasecmp(s, "0x", 2))
    return false;
  s += 2;
  /* now should be looking at the raw digits. */

  /* count the hex digits: */
  xstr = s;

  while(isxdigit(*s))
    s++;

  len = s - xstr;

  if (len > 16 || len == 0)
    return false;
  
  strncpy(digits, xstr, len);
  digits[len] = 0;
    
  if (len > 8) {
    lo = strtoul(&digits[len-8], 0, 16);
    digits[len-8] = 0;
    hi = strtoul(digits, 0, 16);
  }
  else {
    lo = strtoul(digits, 0, 16);
  }

  *oid = hi;
  *oid <<= 32;
  *oid |= lo;

  *txt = s;
  
  return true;
}

bool
parse_MatchNumKeyVal(const char **txt, uint32_t *hi,
		      uint32_t *mid, uint32_t *lo)
{
  const char *s = *txt;
  char digits[25];
  uint32_t len;

  while(isspace(*s))
    s++;

  /* hopefully now looking at 0x... */
  if (strncasecmp(s, "0x", 2))
    return false;
  s += 2;
  /* now should be looking at the raw digits. */

  /* count the hex digits: */

  len = 0;

  while(isxdigit(*s) || *s == ' ') {
    if (len > 24)
      return false;

    if (isxdigit(*s))
      digits[len++] = *s;
    
    s++;
  }

  if (len == 0)
    return false;
  
  digits[len] = 0;
    
  *hi = 0;
  *mid = 0;

  if (len < 8) {
    *lo = strtoul(digits, 0, 16);
  }
  else if (len < 16) {
    *lo = strtoul(&digits[len-8], 0, 16);
    digits[len-8] = 0;
    *mid = strtoul(digits, 0, 16);
  }
  else {
    *lo = strtoul(&digits[len-8], 0, 16);
    digits[len-8] = 0;
    *mid = strtoul(&digits[len-16], 0, 16);
    digits[len-16] = 0;
    *hi = strtoul(digits, 0, 16);
  }

  *txt = s;
  
  return true;
}

bool
parse_MatchKey(const char **txt, KeyBits *key)
{
  const char *s = *txt;
  KeyType kt;

  while(isspace(*s))
    s++;
  
  /* looking for a string that is a plausible key type: */
  if (strncasecmp(s, "start", 5) == 0) {
    s += 5;
    kt = KKT_Start;
  }
  else if (strncasecmp(s, "number", 6) == 0) {
    s += 6;
    kt = KKT_Number;
  }
  else if (strncasecmp(s, "range", 5) == 0) {
    s += 5;
    kt = KKT_Range;
  }
  else if (strncasecmp(s, "page", 4) == 0) {
    s += 4;
    kt = KKT_Page;
  }
  else if (strncasecmp(s, "node", 4) == 0) {
    s += 4;
    kt = KKT_Node;
  }
  else if (strncasecmp(s, "sched", 5) == 0) {
    s += 5;
    kt = KKT_Sched;
  }
  else if (strncasecmp(s, "misc", 4) == 0) {
    s += 4;
    kt = FIRST_MISC_KEYTYPE;
  }
  else {
    return false;
  }

  while(isspace(*s))
    s++;

  if (!parse_Match(&s, "("))
    return false;

  switch (kt) {
  case KKT_Start:
    {
      uint32_t w;
      OID oid;

      if ( parse_MatchOID(&s, &oid) &&
	   parse_Match(&s, ",") &&
	   parse_MatchWord(&s, &w) &&
	   (w <= EROS_KEYDATA_MAX) &&
	   parse_Match(&s, ")") ) {
	init_StartKey(key, oid, w);
	*txt = s;
	return true;
      }
      break;
    }
  case KKT_Number:
    {
      uint32_t hi, mid, lo;
      
      if (parse_MatchNumKeyVal(&s, &hi, &mid, &lo) &&
	  parse_Match(&s, ")") ) {
	init_NumberKey(key, lo, mid, hi);
	*txt = s;
	return true;
      }
      break;
    }
  case KKT_Range:
    {
      OID oidlo, oidhi;

      if (parse_MatchOIDVal(&s, &oidlo) &&
	  parse_Match(&s, ":") &&
	  parse_MatchOIDVal(&s, &oidhi) &&
	  parse_Match(&s, ")") ) {
	init_RangeKey(key, oidlo, oidhi);
	*txt = s;
	return true;
      }
      break;
    }
  case KKT_Page:
  case KKT_Node:
    {
      OID oid;
      bool expectComma = false;
      bool readOnly = false;
      bool rh = false;
      
      /* Node/Seg keys have lots of attributes, so the following is
       * a bit tricky.  First, parse the '(oid' part:
       */
      
      /* Now try to parse the various attributes: */
      while (!parse_Match(&s, ")")) {
	const char *ssave;

	if (expectComma && parse_Match(&s, ",") == false)
	  return false;

	expectComma = true;
	ssave = s;

	if ( parse_MatchStart(&s, ssave) &&
	     parse_MatchKeyword(&s, "oid") &&
	     parse_Match(&s, "=") &&
	     parse_MatchOIDVal(&s, &oid) )
	  continue;
	if ( parse_MatchStart(&s, ssave) &&
	     parse_MatchKeyword(&s, "ro") ) {
	  readOnly = true;
	  continue;
	}
	if ( parse_MatchStart(&s, ssave) &&
	     parse_MatchKeyword(&s, "rh") ) {
	  rh = true;
	  continue;
	}

	return false;
      }

      switch(kt) {
      case KKT_Page:
	init_DataPageKey(key, oid, readOnly);
	break;
      case KKT_Node:
	init_NodeKey(key, oid, readOnly);
	break;
      default:
	break;
      }
      
      if (readOnly)
	keyBits_SetReadOnly(key);
      if (rh)
	keyBits_SetRdHazard(key);

      *txt = s;
      return true;
      break;
    }
  case KKT_Sched:
    {
      uint32_t prio;
      
      if (parse_MatchWord(&s, &prio) &&
	  parse_Match(&s, ")") ) {
	init_SchedKey(key, prio);
	*txt = s;
	return true;
      }
      break;
    }
    break;
  case FIRST_MISC_KEYTYPE:
    {
      const char *name;
      
      if (parse_MatchIdent(&s, &name) &&
	  parse_Match(&s, ")") ) {
	uint32_t miscType;
	
	if (GetMiscKeyType(name, &miscType) == false)
	  return false;

	init_MiscKey(key, miscType, 0);
	*txt = s;
	return true;
      }
      break;
    }
    break;

  default:
    /* Don't know how to parse the others. */
    return false;
  }
  return false;
}
